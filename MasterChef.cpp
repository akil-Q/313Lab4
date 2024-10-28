#include <getopt.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string>
#include "StepList.h"

using namespace std;

StepList* recipeSteps;
vector<int>* completedSteps;
int completeCount = 0;

void PrintHelp() // Given
{
    cout << "Usage: ./MasterChef -i <file>\n\n";
    cout<<"--------------------------------------------------------------------------\n";
    cout<<"<file>:    "<<"csv file with Step, Dependencies, Time (m), Description\n";
    cout<<"--------------------------------------------------------------------------\n";
    exit(1);
}

string ProcessArgs(int argc, char** argv) // Given
{
    string result = "";
    // print help if odd number of args are provided
    if (argc < 3) {
        PrintHelp();
    }

    while (true)
    {
        const auto opt = getopt(argc, argv, "i:h");

        if (-1 == opt)
            break;

        switch (opt)
        {
        case 'i':
            result = std::string(optarg);
            break;
        case 'h': // -h or --help
        default:
            PrintHelp();
            break;
        }
    }

    return result;
}

void makeTimer(Step *timerID, int expire) // Given
{
    struct sigevent te;
    struct itimerspec its;

    /* Set and enable alarm */
    te.sigev_notify = SIGEV_SIGNAL;
    te.sigev_signo = SIGRTMIN;
    te.sigev_value.sival_ptr = timerID;
    timer_create(CLOCK_REALTIME, &te, &(timerID->t_id));

    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    its.it_value.tv_sec = expire;
    its.it_value.tv_nsec = 0;
    timer_settime(timerID->t_id, 0, &its, NULL);
}

static void timerHandler(int sig, siginfo_t *si, void *uc)
{
    (void) sig;
    (void) uc;
    // Retrieve timer pointer from the si->si_value
    Step* comp_item = (Step*)si->si_value.sival_ptr;
    // Mark the step as completed
    completedSteps->push_back(comp_item->id);
    completeCount++;
    // Print completion message
    comp_item->PrintComplete();
    // Raise SIGUSR1 signal to handle dependency removal
    raise(SIGUSR1);
}

void RemoveDepHandler(int sig) {
    // For each completed step, remove it from dependency lists
    for (int stepId : *completedSteps) {
        recipeSteps->RemoveDependency(stepId);
    }
    completedSteps->clear();
}

int main(int argc, char **argv)
{
    string input_file = ProcessArgs(argc, argv);
    if(input_file.empty()) {
        exit(1);
    }
    
    // Initialize global variables
    completedSteps = new vector<int>();
    recipeSteps = new StepList(input_file);

    /* Set up signal handler for SIGRTMIN */
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timerHandler;
    sigemptyset(&sa.sa_mask);

    // Associate signals with handlers
    sigaction(SIGRTMIN, &sa, NULL);
    signal(SIGUSR1, RemoveDepHandler);

    // Continue until all steps are completed
    while (completeCount < recipeSteps->Count()) {
        // Get steps that are ready to run
        vector<Step*> readySteps = recipeSteps->GetReadySteps();
        // Start timers for ready steps that aren't already running
        for (Step* step : readySteps) {
            if (!step->running) {
                step->running = true;
                makeTimer(step, step->duration);
            }
        }
        usleep(100000); // Increased delay to 100ms for better synchronization
    }

    // Cleanup
    delete completedSteps;
    delete recipeSteps;
    return 0;
}