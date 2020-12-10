/*
 * montac
 *  MONitoring of Temperatures And Clock speeds
 *
 * Compile with:
 * $ gcc -pthread monitorTempsAndClock.c -o montac
 *
 * And you need to install the sensors package:
 * $ sudo apt-get install lm-sensors
 * $ sudo sensors-detect
 * $ sudo service kmod start
 *
 * Usage is like:
 * $ montac -c "./run.sh" -o data
 * which will output to
 * > data.temps
 * > data.clocks
 *
 * by Jamie Border
 * 2020-12-10
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

static void showUsage(char *name)
{
    printf("Usage: %s -c CMD -o SAVEFILE\n", name);
    printf("\ni.e. %s -c \"make >> log\" -o data\n", name);
    printf("outputs to:\n  data.temps\n  data.clock\n");
    printf("\nOptions\n");
    printf(" -h     this message is printed\n");
    printf(" -c     command to be run by bash (surround with \" for spaces)\n");
    printf(" -o     prefix of output files (prefix.{temps,clock})\n");
    printf(" -f     frequency to log data points (usecs)\n");
    printf(" -v     how much noise to make [0,1]\n");
}

struct Args
{
    char *saveFilename;
    int logFreq_us;
    int verb;
};

void* logClockSpeeds(void *arguments)
{
    struct Args *args =  (struct Args *)arguments;
    int status = 0;

    char *name;
    name = args->saveFilename;
    if (args->verb)
        printf("saving clock speeds (MHz) every %f s to file... %s\n",
                (float)(args->logFreq_us)/1000./1000., name);

    FILE *file;

    if (name == NULL || name[0] == '\0') {
        printf("failed to open file: %s\n", name);
        pthread_exit(&status);
    }
    else {
        file = fopen(name, "w");
    }

    if (file == NULL) {
        printf("failed to open file: %s\n", name);
        pthread_exit(&status);
    }

    char *logStr;
    logStr = (char *)malloc(150 * sizeof(char));
    // this str is 82 characters long
    strcpy(logStr, "cat /proc/cpuinfo | grep \"^[c]pu MHz\" | "\
            "awk '{print($4)}' | paste -sd \" \" >> ");
    strcat(logStr, name);

    int maxLogs = 100000;
    while (maxLogs--) {
        system(logStr);
        usleep(args->logFreq_us);
    }

    if (args->verb)
        printf("reached maximum number of logs in %s\n", name);
    fclose(file);

    status = 1;
    pthread_exit(&status);
}

void* logTemperatures(void *arguments)
{
    struct Args *args =  (struct Args *)arguments;
    int status = 0;

    char *name;
    name = args->saveFilename;
    if (args->verb)
        printf("saving temps (degrees C) every %f s to file... %s\n",
            (float)(args->logFreq_us)/1000./1000., name);

    FILE *file;

    if (name == NULL || name[0] == '\0') {
        printf("failed to open file: %s\n", name);
        pthread_exit(&status);
    }
    else {
        file = fopen(name, "w");
    }

    if (file == NULL) {
        printf("failed to open file: %s\n", name);
        pthread_exit(&status);
    }

    char *logStr;
    logStr = (char *)malloc(200 * sizeof(char));
    // this str is 119 characters long
    strcpy(logStr, "sensors | awk '{ if($1==\"Core\") "\
            "print substr($3,0,length($3)-3) }' | awk '{gsub(/\\+/,\"\")}1' "\
            "| paste -sd \" \" >> ");
    strcat(logStr, name);

    int maxLogs = 100000;
    while (maxLogs--) {
        system(logStr);
        usleep(args->logFreq_us);
    }

    if (args->verb)
        printf("reached maximum number of logs in %s\n", name);
    fclose(file);

    status = 1;
    pthread_exit(&status);
}

int main (int argc, char **argv)
{
    char *cmdStr = NULL;
    struct Args args;
    args.saveFilename = NULL;
    args.logFreq_us = 500000; // default: 0.5 s
    args.verb = 1;

    if (argc == 1) {
        showUsage(argv[0]);
        exit(EXIT_FAILURE);
    }

    int c;
    opterr = 0;

    while ((c = getopt (argc, argv, "c:o:fv")) != -1) {
        switch (c)
        {
            case 'c':
                cmdStr = optarg;
                break;
            case 'o':
                args.saveFilename = optarg;
                break;
            case 'f':
                args.logFreq_us = atoi(optarg);
                if (args.logFreq_us < 1) {
                    printf("bad logFrequency supplied\n");
                    showUsage(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'v':
                args.verb = atoi(optarg);
                if (args.verb < 0) {
                    printf("bad verbosity supplied\n");
                    showUsage(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case '?':
                if (optopt == 'c'
                 || optopt == 'o'
                 || optopt == 'f'
                 || optopt == 'v') {
                    fprintf(stderr, "Option -%c requires an argument.\n",
                            optopt);
                }
                else if (isprint(optopt)) {
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                }
                else {
                    fprintf (stderr, "Unknown option character `\\x%x'.\n",
                           optopt);
                }
                return 1;
            default:
                abort();
        }
    }

    if (optind != argc) {
        for (int index = optind; index < argc; index++) {
            printf ("Non-option argument %s\n", argv[index]);
        }
        showUsage(argv[0]);
        exit(EXIT_FAILURE);
    }

    char saveFilenameTemps[60];
    char saveFilenameClock[60];
    if (strlen(args.saveFilename) > 50) {
        printf("saveFilename too long: 50 character limit\n");
        exit(EXIT_FAILURE);
    }
    strcpy(saveFilenameTemps, args.saveFilename);
    strcat(saveFilenameTemps, ".temps");
    strcpy(saveFilenameClock, args.saveFilename);
    strcat(saveFilenameClock, ".clock");

    pthread_t idTemps;
    pthread_t idClock;

    pthread_create(&idTemps, NULL, logTemperatures, (void *)&args);
    pthread_create(&idClock, NULL, logClockSpeeds, (void *)&args);
    
    if (args.verb > 0) printf("running cmd: %s\n", cmdStr);
    if (system(cmdStr)) {
        printf("failed to run cmd: %s\n", cmdStr);
    }

    pthread_cancel(idTemps);
    pthread_cancel(idClock);

    exit(EXIT_SUCCESS);
}
