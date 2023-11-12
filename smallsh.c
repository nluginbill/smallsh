#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>

#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

char *words[MAX_WORDS];
pid_t last_background_pid = -5;
pid_t last_background_exit_status = 0;
pid_t last_foreground_exit_status = 0;
pid_t smallsh_pid = -5;
size_t wordsplit(char const *line);
char * expand(char const *word);

int main(int argc, char *argv[])
{
  smallsh_pid = getpid();
  FILE *input = stdin;
  char *input_fn = "(stdin)";
  if (argc == 2) {
    input_fn = argv[1];
    input = fopen(input_fn, "re");
    if (!input) err(1, "%s", input_fn);
  } else if (argc > 2) {
    errx(1, "too many arguments");
  }

  char *line = NULL;
  size_t n = 0;
  for (;;) {
//prompt:;
    /* TODO: Manage background processes */

    /* TODO: prompt */
    if (input == stdin) {

    }
    ssize_t line_len = getline(&line, &n, input);
    if (line_len < 0) {
      if (feof(input)) {
        exit(0);
      }
      err(1, "%s", input_fn);
    }
    
    size_t nwords = wordsplit(line);
    // null out the words that remain in the words array past nwords
    for (size_t i = nwords; i < MAX_WORDS; ++i) {
      words[i] = NULL;
    }
    for (size_t i = 0; i < nwords; ++i) {
      // fprintf(stderr, "Word %zu: %s\n", i, words[i]);
      char *exp_word = expand(words[i]);
      free(words[i]);
      words[i] = exp_word;
      // fprintf(stderr, "Expanded Word %zu: %s\n", i, words[i]);
    }

    char *command = words[0];

    if (strcmp(command, "exit") == 0) {
      if (nwords > 2) {
        fprintf(stderr, "exit: too many arguments\n");
      }
      else if (nwords == 2) {
        if (atoi(words[1]) == 0) {
          fprintf(stderr, "exit: argument must be a non-negative integer\n");
        }
        else {
          exit(atoi(words[1]));
        }
      }
      else {
        // TODO: after non-built-in commands
        // exit with value of $? (status of last foreground process)
        exit(last_foreground_exit_status);

      }
    }
    // if command is cd
    else if (strcmp(command, "cd") == 0) {
      if (nwords > 2) {
        fprintf(stderr, "cd: too many arguments\n");
      }
      else if (nwords == 2) {
        if (chdir(words[1]) == -1) {
          fprintf(stderr, "cd: %s: %s\n", words[1], strerror(errno));
        }
      }
      else {
        // change directory to HOME
        if (chdir(getenv("HOME")) == -1) {
          fprintf(stderr, "cd: %s\n", strerror(errno));
        }
      }
    }
    else {
      // non-built-in commands
      int background = 0;
      pid_t pid = -5;
      pid = fork();      
      // if there is a &, then we need to set the background to 1
      if (strcmp(words[nwords - 1], "&") == 0) {
        background = 1;
        // remove the & from the words array
        words[nwords - 1] = NULL;
        --nwords;
        // store the pid from fork, the child, as the last background pid
        last_background_pid = pid;
      }
      // for handling error, child, or parent process
      switch (pid) {
        case -1:
          fprintf(stderr, "fork: %s\n", strerror(errno));
          exit(1);
          break;
        case 0:
          // child process
          // TODO: All signals shall be reset to their original dispositions when smallsh was invoked
          int has_command_path = 0;          

          for (size_t i = 0; i < nwords; ++i) {
            // if there is a <, then we need to open the file for reading
            if (strcmp(words[i], "<") == 0) {
              FILE *input_file = fopen(words[i + 1], "r");
              if (!input_file) {
                fprintf(stderr, "%s: %s\n", words[i + 1], strerror(errno));
                exit(1);
              }
              // set the file to stdin
              if (dup2(fileno(input_file), STDIN_FILENO) == -1) {
                fprintf(stderr, "dup2: %s\n", strerror(errno));
                exit(1);
              }
              // remove the < and the file name from the words array
              words[i] = NULL;
              words[i + 1] = NULL;
              nwords -= 2;
              break;
            }
            // if there is a >, then we need to open the file for writing
            else if (strcmp(words[i], ">") == 0) {
              // open the specified file for writing on stdoout. If the file does not exist it will be
              // created. If the file exists, it will be truncated to 0 bytes. File permissions will
              // be set to 0777.
              int output_file = open(words[i + 1], O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0777);
              // FILE *output_file = fopen(words[i + 1], "w");

              if (!output_file) {
                fprintf(stderr, "%s: %s\n", words[i + 1], strerror(errno));
                exit(1);
              }

              if (dup2(output_file, STDOUT_FILENO) == -1) {
                fprintf(stderr, "dup2: %s\n", strerror(errno));
                exit(1);
              }
              // close the file descriptor?
              // remove the > and the file name from the words array
              words[i] = NULL;
              words[i + 1] = NULL;
              nwords -= 2;
              break;
            }
            // if there is a >>, then we need to open the file for appending
            else if (strcmp(words[i], ">>") == 0) {
              // open the specified file for appending on stdout. If the file does not exist it will be
              // created. File permissions will be set to 0777.
              int output_file = open(words[i + 1], O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0777);

              if (!output_file) {
                fprintf(stderr, "%s: %s\n", words[i + 1], strerror(errno));
                exit(1);
              }

              // dup2 the file descriptor to stdout
              if (dup2(output_file, STDOUT_FILENO) == -1) {
                fprintf(stderr, "dup2: %s\n", strerror(errno));
                exit(1);
              }
              // close the file descriptor?
              // remove the >> and the file name from the words array
              words[i] = NULL;
              words[i + 1] = NULL;
              nwords -= 2;
              break;
            }        
          }
          for (size_t i = 0; i < nwords; ++i) {
            // if <PID> is in the command, then replace it with the pid of smallsh
            if (strcmp(words[i], "<PID>") == 0) {
              char pid_str[10];
              sprintf(pid_str, "%d", smallsh_pid);
              words[i] = pid_str;
            }
            // if <STATUS> is in the command, then replace it with the status of the last foreground
            // process
            if (strcmp(words[i], "<STATUS>") == 0) {
              char status_str[10];
              sprintf(status_str, "%d", last_foreground_exit_status);
              words[i] = status_str;
            }
            // if <BGPID> is in the command, then replace it with the pid of the last background
            // process
            if (strcmp(words[i], "<BGPID>") == 0) {
              char bgpid_str[10];
              sprintf(bgpid_str, "%d", last_background_pid);
              words[i] = bgpid_str;
            }
            char *start = strstr(words[i], "<Parameter: ");
            if (start) {
              start += 12; // length of "<Parameter: "
              char *end = strchr(start, '>');
              int length = end - start;
              char *parameter = malloc(length + 1);
              strncpy(parameter, start, length);
              parameter[length] = '\0';

              char *env_var_value = getenv(parameter);
              if (env_var_value) {
                words[i] = env_var_value;
              }
              else {
                words[i] = "";
              }
            }
          }

          // if the command contains a /, then note that there is a command path
          if (strchr(command, '/') != NULL) {
            has_command_path = 1;
          }
          // if words is not empty, then we need to execute the command
          if (words[0] != NULL) {
            // if the command contains a /
            if (has_command_path == 1) {
              // execute the command with execv
              if (execv(command, words) == -1) {
                fprintf(stderr, "%s: %s\n", command, strerror(errno));
                exit(1);
              }
            } else {
              // execute the command with execvp
              if (execvp(command, words) == -1) {
                fprintf(stderr, "%s: %s\n", command, strerror(errno));
                exit(1);
              }
            }
          }
          default:
          // parent process
          int status;
          if (background == 1) {
            last_background_pid = waitpid(pid, &status, WNOHANG );

            if (WIFSIGNALED(status)) {
              last_background_exit_status = WTERMSIG(status) + 128;
            } else {
              last_background_exit_status = WEXITSTATUS(status);
              printf("Child process %d done. Exit status %d.\n", pid, last_background_exit_status);
            }
          if (last_background_pid == -1) {
            fprintf(stderr, "waitpid: %s\n", strerror(errno));
          }
          break;
          }
          // if the command is not a background process (foreground process)
          if (background == 0) {
            // wait for the child process to finish
            
            pid_t last_foreground_pid = waitpid(pid, &status, 0);
            if (last_foreground_pid == -1) {
              fprintf(stderr, "waitpid: %s\n", strerror(errno));
            }
            if (WIFSIGNALED(status)) {
              last_foreground_exit_status = WTERMSIG(status) + 128;
            } else {
              last_foreground_exit_status = WEXITSTATUS(status);
            }
          break;
            
          }

        }
        
      }
    }
  }

char *words[MAX_WORDS] = {0};


/* Splits a string into words delimited by whitespace. Recognizes
 * comments as '#' at the beginning of a word, and backslash escapes.
 *
 * Returns number of words parsed, and updates the words[] array
 * with pointers to the words, each as an allocated string.
 */
size_t wordsplit(char const *line) {
  size_t wlen = 0;
  size_t wind = 0;

  char const *c = line;
  for (;*c && isspace(*c); ++c); /* discard leading space */

  for (; *c;) {
    if (wind == MAX_WORDS) break;
    /* read a word */
    if (*c == '#') break;
    for (;*c && !isspace(*c); ++c) {
      if (*c == '\\') ++c;
      void *tmp = realloc(words[wind], sizeof **words * (wlen + 2));
      if (!tmp) err(1, "realloc");
      words[wind] = tmp;
      words[wind][wlen++] = *c; 
      words[wind][wlen] = '\0';
    }
    ++wind;
    wlen = 0;
    for (;*c && isspace(*c); ++c);
  }
  return wind;
}


/* Find next instance of a parameter within a word. Sets
 * start and end pointers to the start and end of the parameter
 * token.
 */
char
param_scan(char const *word, char **start, char **end)
{
  static char *prev;
  if (!word) word = prev;
  
  char ret = 0;
  *start = NULL;
  *end = NULL;
  char *s = strchr(word, '$');
  if (s) {
    char *c = strchr("$!?", s[1]);
    if (c) {
      ret = *c;
      *start = s;
      *end = s + 2;
    }
    else if (s[1] == '{') {
      char *e = strchr(s + 2, '}');
      if (e) {
        ret = '{';
        *start = s;
        *end = e + 1;
      }
    }
  }
  prev = *end;
  return ret;
}

/* Simple string-builder function. Builds up a base
 * string by appending supplied strings/character ranges
 * to it.
 */
char *
build_str(char const *start, char const *end)
{
  static size_t base_len = 0;
  static char *base = 0;

  if (!start) {
    /* Reset; new base string, return old one */
    char *ret = base;
    base = NULL;
    base_len = 0;
    return ret;
  }
  /* Append [start, end) to base string 
   * If end is NULL, append whole start string to base string.
   * Returns a newly allocated string that the caller must free.
   */
  size_t n = end ? end - start : strlen(start);
  size_t newsize = sizeof *base *(base_len + n + 1);
  void *tmp = realloc(base, newsize);
  if (!tmp) err(1, "realloc");
  base = tmp;
  memcpy(base + base_len, start, n);
  base_len += n;
  base[base_len] = '\0';

  return base;
}

/* Expands all instances of $! $$ $? and ${param} in a string 
 * Returns a newly allocated string that the caller must free
 */
char *
expand(char const *word)
{
  char const *pos = word;
  char *start, *end;
  char c = param_scan(pos, &start, &end);
  build_str(NULL, NULL);
  build_str(pos, start);
  while (c) {
    if (c == '!') build_str("<BGPID>", NULL);
    else if (c == '$') build_str("<PID>", NULL);
    else if (c == '?') build_str("<STATUS>", NULL);
    else if (c == '{') {
      build_str("<Parameter: ", NULL);
      build_str(start + 2, end - 1);
      build_str(">", NULL);
    }
    pos = end;
    c = param_scan(pos, &start, &end);
    build_str(pos, start);
  }
  return build_str(start, NULL);
}
