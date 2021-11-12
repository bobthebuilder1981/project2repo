#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NUM_JOBTYPES 4
#define RR_QUANTUM_LENGTH 10

enum states {
    RTR, CPU, IO, DONE
};

enum algs {
    FCFS, PS, SJF, RR
};

typedef struct job {

    int id;
    int priority;

    int start_time;
    int end_time;
    int wait_time;

    int state;
    int burst_countdown;

    int cpu_burst_length;
    int io_burst_length;

    int quant_countdown;

    int reps;

} job_t;

typedef struct node {
    int priority;
    job_t *job;
    struct node *next;
} node_t;

node_t *rtr_queue;
job_t **jobs;
job_t *active = NULL;

int num_jobs = 0;
int finished_jobs = 0;

int time = 1;
int cpu_busy_time = 0;
int cpu_idle_time = 0;

const char *alg_names[] = {"fcfs", "ps", "sjf", "rr"};
int alg_id = -1;

int iscomment(const char *line);
void load_jobs(FILE *f);
job_t *set_next_job();
void run();
void print_status_line();
void print_report();

void push(node_t **head, job_t *j);
job_t *pop(node_t **);
int getpri(job_t *j);

/*-------------------------------------------------------------*/

int main(int argc, char *argv[]) {

    if (argc != 3) {
        puts("usage: cpu_sim <process filename> <algorithm>");
        return 0;
    }

    for (int i = 0; i < NUM_JOBTYPES; i++) {
        if (!strcmp(argv[2], alg_names[i])) {
            alg_id = i;
        }
    }

    FILE *f = fopen(argv[1], "r");
    load_jobs(f);

    fclose(f);

    if (alg_id != -1) {

        run();
    } else {
        printf("\"%s\" is not a valid algorithm.\n", alg_names[alg_id]);
        return 0;
    }
}

/*-------------------------------------------------------------*/

void run() {

    puts("┌───────────────────────────────┐");
    puts("│   Time   :   CPU   :    IO    │");
    puts("├───────────────────────────────┤");

    for (;;) {

        if (active) {

            --active->burst_countdown;
            --active->quant_countdown;

            // if CPU burst is done, switch to IO
            // and start running a new job.
            if (active->burst_countdown == 0) {
                active->state = IO;
                active->burst_countdown = active->io_burst_length;
                active = set_next_job();
            }

            // else if the current job's quantum is expired, put it
            // at the end of the RTR queue and start the next one.
            else if (active->quant_countdown == 0) {
                active->state = RTR;
                push(&rtr_queue, active);
                active = set_next_job();
            }

            ++cpu_busy_time;

        }

        // if no job is using the CPU, try to pop a new one from the queue.
        // increment the idle time if there is nothing ready to run.
        else {
            if (!(active = set_next_job())) {
                ++cpu_idle_time;
            }
        }

        // now look at the jobs that are not using the CPU.
        for (int i = 0; i < num_jobs; i++) {

            job_t *j = jobs[i];

            if (j->state == IO) {
                // if IO burst is done, put back into RTR queue.
                if (j->burst_countdown-- == 0) {
                    if (j->reps == 0) {
                        j->state = DONE;
                        j->end_time = time;
                        finished_jobs++;
                    } else {
                        j->state = RTR;
                        push(&rtr_queue, j);
                    }
                }
            }

            if (j->state == RTR) {
                ++j->wait_time;
            }
        }

        if(finished_jobs<num_jobs)
            print_status_line();
        else
            break;

        ++time;
    }

    print_report();

    for (int i = 0; i < num_jobs; i++) {
        free(jobs[i]);
    }

    if (rtr_queue) {
        free(rtr_queue);
    }

}

/*-------------------------------------------------------------*/

job_t *set_next_job() {

    job_t *j = NULL;
    if ((j = pop(&rtr_queue))) {

        j->state = CPU;

        if (!j->start_time) {
            j->start_time = time;
        }

        if (j->burst_countdown <= 0) {
            j->burst_countdown = j->cpu_burst_length;
            j->reps--;
        }

        j->quant_countdown = (alg_id == RR ? RR_QUANTUM_LENGTH : -1);
    }
    return j;
}

/*-------------------------------------------------------------*/

void push(node_t **head, job_t *j) {

    node_t *new = malloc(sizeof(node_t));
    new->job = j;
    new->next = NULL;
    new->priority = getpri(j);

    if (*head == NULL) {
        *head = new;
        return;
    }

    if (new->priority < (*head)->priority) {
        new->next = *head;
        *head = new;
        return;
    }

    node_t *cur = *head;

    while (cur->next && new->priority > cur->next->priority) {
        cur = cur->next;
    }

    new->next = cur->next;
    cur->next = new;
}

/*-------------------------------------------------------------*/

job_t *pop(node_t **head) {

    node_t *cur = *head;

    if (!cur) {
        return NULL;
    }

    job_t *j = cur->job;
    *head = (*head)->next;
    free(cur);

    return j;
}

/*-------------------------------------------------------------*/

int getpri(job_t *j) {

    int p = -1;

    if (alg_id == PS) {
        p = j->priority;
    }

    if (alg_id == SJF) {
        p = j->cpu_burst_length * j->reps;
    }

    if (alg_id == RR) {
        p = time;
    }

    return p;
}

/*-------------------------------------------------------------*/

void print_status_line() {

    char *iostring = malloc(16 * sizeof(char));
    int c = 0;

    for (int i = 0; i < num_jobs; i++) {
        if (jobs[i]->state == IO) {
            c += sprintf(&iostring[c], "%d ", jobs[i]->id);
        }
    }

    if (!c) {
        strcpy(iostring, "xx");
    }

    if (active) printf("│  %4d %9d %9s     │\n", time, active->id, iostring);
    else printf("│  %4d %9s %9s     │\n", time, "xx", iostring);

    free(iostring);
}

/*-------------------------------------------------------------*/

void print_report() {

    puts("└───────────────────────────────┘\n");

    int turn_time = 0;

    for (int i = 0; i < num_jobs; i++) {

        printf("   Process ID: %5d\n", jobs[i]->id);
        printf("   Start Time: %5d\n", jobs[i]->start_time);
        printf("   End Time:   %5d\n", jobs[i]->end_time);
        printf("   Wait Time:  %5d\n", jobs[i]->wait_time);

        turn_time += jobs[i]->end_time;
        puts("─────────────────────────────────");
    }

    printf("   Average Turnaround Time: %d\n", turn_time / num_jobs);
    printf("   CPU Busy Time: %d\n", cpu_busy_time);
    printf("   CPU Idle Time: %d\n\n", cpu_idle_time);

}

/*-------------------------------------------------------------*/

void load_jobs(FILE *f) {

    int bufsize = 128;
    char line[bufsize];

    while (fgets(line, bufsize, f)) {

        if (!iscomment(line)) {

            job_t *j = calloc(1, sizeof(job_t));

            sscanf(line, "%d %d %d %d %d",
                   &j->id,
                   &j->cpu_burst_length,
                   &j->io_burst_length,
                   &j->reps,
                   &j->priority);

            push(&rtr_queue, j);
            ++num_jobs;
        }
    }

    jobs = malloc(num_jobs * sizeof(job_t));

    node_t *cur = *&rtr_queue;

    int i = 0;
    while (cur) {
        jobs[i++] = cur->job;
        cur = cur->next;
    }
}

/*-------------------------------------------------------------*/

int iscomment(const char *line) {
    return (line[0] == '/' && line[1] == '/');
}
