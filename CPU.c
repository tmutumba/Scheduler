#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include "systemcall.h"

#define NUM_SECONDS 20
#define ever ;;


enum STATE { NEW, RUNNING, WAITING, READY, TERMINATED, EMPTY };

struct PCB {
    enum STATE state;
    const char *name;   // name of the executable
    int pid;            // process id from fork();
    int ppid;           // parent process id
    int interrupts;     // number of times interrupted
    int switches;       // may be < interrupts
    int started;        // the time this process started
};

typedef enum { false, true } bool;

#define PROCESSTABLESIZE 10
struct PCB processes[PROCESSTABLESIZE];

//Enable interrupts on this process


struct PCB idle;
struct PCB *running;

int sys_time;
int timer;
int status;
int process_num;
int process;
int counter;
int found;
struct sigaction alarm_handler;
struct sigaction child_handler;

void bad(int signum) {
    WRITESTRING("bad signal: ");
    WRITEINT(signum, 4);
    WRITESTRING("\n");
}

// cdecl> declare ISV as array 32 of pointer to function(int) returning void
void(*ISV[32])(int) = {
/*       00   01   02   03   04   05   06   07   08   09 */
/*  0 */ bad, bad, bad, bad, bad, bad, bad, bad, bad, bad,
/* 10 */ bad, bad, bad, bad, bad, bad, bad, bad, bad, bad,
/* 20 */ bad, bad, bad, bad, bad, bad, bad, bad, bad, bad,
/* 30 */ bad, bad
};

void ISR (int signum) {
    if (signum != SIGCHLD) {
        kill (running->pid, SIGSTOP);
        WRITESTRING("Stopping: ");
        WRITEINT(running->pid, 6);
        WRITESTRING("\n");
    }

    ISV[signum](signum);
}

void send_signals(int signal, int pid, int interval, int number) {
    for(int i = 1; i <= number; i++) {
        sleep(interval);
        WRITESTRING("Sending signal: ");
        WRITEINT(signal, 4);
        WRITESTRING(" to process: ");
        WRITEINT(pid, 6);
        WRITESTRING("\n");
        systemcall(kill(pid, signal));
    }

    WRITESTRING("At the end of send_signals\n");
}

void create_handler(int signum, struct sigaction action, void(*handler)(int)) {
    action.sa_handler = handler;

    if (signum == SIGCHLD) {
        action.sa_flags = SA_NOCLDSTOP | SA_RESTART;
    } else {
        action.sa_flags =  SA_RESTART;
    }

    systemcall(sigemptyset(&action.sa_mask));
    systemcall(sigaction(signum, &action, NULL));
}

void scheduler (int signum) {
    WRITESTRING("---- entering scheduler\n");
    assert(signum == SIGALRM);

    if (running -> state == TERMINATED) {
        WRITESTRING ("Continuing idle: ");
        WRITEINT (idle.pid, 6);
        WRITESTRING ("\n");
        running = &idle;
        idle.state = RUNNING;
        systemcall (kill(idle.pid, SIGCONT));
        running->pid = idle.ppid;
        running->name = idle.name;
    }
    /* forking starts */

    running ->state = READY;
    running ->switches ++;

    process_num = 0;
    found = 0;
    while(found == 0 && process_num < counter){
        if(processes[process_num].state == NEW){
            found = 1;
        }else{
            process_num ++;
        }
    }
    if(found){
        running = &processes[process_num];
        running->state = RUNNING;
        assert(processes[process_num].started = time(NULL));
        running->interrupts = 0;
        running->switches = 0;
        assert((running->pid = fork()) != -1);

        if(running->pid <0){
            perror("fork()");
            exit(-1);
        }
        else if (running->pid == 0){
            int status = execl(running->name, running->name, NULL);
            perror("execl() failed \n");
            exit(status);
        }
        if(running->pid != processes[process_num].pid){
            processes[process_num].switches ++;
        }
    } else{
        if(process == counter){
            process = 0;
        }
        if(processes[process].state == READY){
            if(running->pid != processes[process].pid){
                running->switches ++;
            }

            running = &processes[process];
            systemcall(kill(running->pid, SIGCONT));
            process++;
        }
    }
    /*running ->interrupts ++;


    processes[getpid()].pid = fork();
    running = &processes[getpid()];

    processes[getppid()].ppid = getppid();
    processes[getpid()].pid = getpid();

    processes[getpid()].started = clock();
    processes[getpid()].state = RUNNING;

    status = execl(processes[getpid()].name, processes[getpid()].name, NULL);*/

    sleep(2);
    WRITESTRING("---- leaving scheduler\n");
}

void process_done (int signum) {
    WRITESTRING("---- entering process_done\n");
    assert (signum == SIGCHLD);

    if(running->pid != idle.pid){
        running->state = TERMINATED;
        WRITESTRING("\nProcess: ");
        WRITEINT(running->pid, 7);
        WRITESTRING("\nInterrupts: ");
        WRITEINT(running->interrupts, 7);
        WRITESTRING("\nTime: ");
        WRITEINT(time(NULL) - running->started, 10);
    } else {

        WRITESTRING ("Timer died, cleaning up and killing everything\n");
        systemcall(kill(0, SIGTERM));
    }

    WRITESTRING ("---- leaving process_done\n");
}

void boot()
{
    sys_time = 0;

    ISV[SIGALRM] = scheduler;
    ISV[SIGCHLD] = process_done;
    create_handler(SIGALRM, alarm_handler, ISR);
    create_handler(SIGCHLD, child_handler, ISR);

    assert((timer = fork()) != -1);
    if (timer == 0) {
        send_signals(SIGALRM, getppid(), 1, NUM_SECONDS);
        exit(0);
    }
}

void create_idle() {
    idle.state = READY;
    idle.name = "IDLE";
    idle.ppid = getpid();
    idle.interrupts = 0;
    idle.switches = 0;
    idle.started = sys_time;

    assert((idle.pid = fork()) != -1);
    if (idle.pid == 0) {
        systemcall(pause());
    }
}

int main(int argc, char **argv) {

    boot();

    create_idle();
    running = &idle;

    if (argc > 1) {
        for (int i = 0; i < argc; i++) {
            processes[i].name = argv[i + 1];
            processes[i].state = NEW;
            processes[i].ppid = 0;
            processes[i].interrupts = 0;
            processes[i].switches = 0;

            WRITESTRING("Process");

            //printf(process);
            //assert(printf("this is the process", i) != -1);

            //new_list.push_back(processes);
        }
    }


    for(ever) {
        pause();
    }
}