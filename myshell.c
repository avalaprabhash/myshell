#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_ARGS    100
#define MAX_INPUT   1024

//––– English→Unix Translator –––
typedef struct {
    const char *keywords[4];
    int         n_keywords;
    const char *template;      // e.g. "cd %s"
} EngMap;

EngMap mappings[] = {
    {{"list","files"},             2, "ls"},
    {{"show","current","directory"},3, "pwd"},
    {{"change","directory","to"},   3, "cd %s"},
    {{"make","directory"},         2, "mkdir %s"},
    {{"delete","file"},            2, "rm %s"},
};
int n_mappings = sizeof(mappings)/sizeof(*mappings);

static void str_tolower(char *s) {
    for (; *s; ++s) *s = tolower(*s);
}

int translate_english(char *in, char *out, size_t sz) {
    char tmp[MAX_INPUT];
    strncpy(tmp, in, sizeof(tmp));
    str_tolower(tmp);

    for (int i = 0; i < n_mappings; i++) {
        int ok = 1;
        for (int k = 0; k < mappings[i].n_keywords; k++) {
            if (!strstr(tmp, mappings[i].keywords[k])) {
                ok = 0;
                break;
            }
        }
        if (!ok) continue;

        // grab last word as argument
        char *last = strrchr(in, ' ');
        const char *arg = last ? last+1 : "";
        if (strstr(mappings[i].template, "%s"))
            snprintf(out, sz, mappings[i].template, arg);
        else
            strncpy(out, mappings[i].template, sz);
        return 1;
    }
    return 0;
}

//––– Your existing shell helpers (prompt, parser, executor) –––
void print_prompt() {
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    printf("\033[1;34m[PRABHAS@myshell]\033[0;32m %s\033[0m$ ", cwd);
}

int parse_input(char *input, char **args) {
    int i = 0;
    args[i] = strtok(input, " \t\n");
    while (args[i] && i < MAX_ARGS-1) {
        args[++i] = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    return i;
}

void execute_command(char **args) {
    if (!args[0]) return;
    if (strcmp(args[0], "exit")==0) exit(0);
    if (strcmp(args[0], "cd")==0) {
        if (args[1]) chdir(args[1]);
        return;
    }
    pid_t pid = fork();
    if (pid==0) {
        execvp(args[0], args);
        perror("exec");
        exit(1);
    }
    wait(NULL);
}

void execute_piped(char *left, char *right) {
    char *la[MAX_ARGS], *ra[MAX_ARGS];
    parse_input(left, la);
    parse_input(right, ra);

    int fd[2]; pipe(fd);
    if (fork()==0) {
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]); close(fd[1]);
        execvp(la[0], la);
        perror("left");
        exit(1);
    }
    if (fork()==0) {
        dup2(fd[0], STDIN_FILENO);
        close(fd[1]); close(fd[0]);
        execvp(ra[0], ra);
        perror("right");
        exit(1);
    }
    close(fd[0]); close(fd[1]);
    wait(NULL); wait(NULL);
}

//––– main loop with translation –––
int main() {
    char *raw, cmd[MAX_INPUT], *pargs[MAX_ARGS], *ppipe[MAX_ARGS];

    while (1) {
        print_prompt();
        raw = readline("");
        if (!raw || *raw=='\0') {
            free(raw);
            continue;
        }
        add_history(raw);

        // 1) Try English translation
        if (translate_english(raw, cmd, sizeof(cmd))) {
            printf("→ executing: %s\n", cmd);
        } else {
            strncpy(cmd, raw, sizeof(cmd));
        }
        free(raw);

        // 2) Handle pipe or simple
        if (strchr(cmd, '|')) {
            char *left = strtok(cmd, "|");
            char *right= strtok(NULL, "|");
            execute_piped(left, right);
        } else {
            parse_input(cmd, pargs);
            execute_command(pargs);
        }
    }
    return 0;
}
