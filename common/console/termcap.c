/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include "native_client/common/console/term.h"
#include "native_client/common/console/console_c.h"

/* exports for termcap */
char PC = 0;
char *UP = "";
char *BC = "";
short ospeed = 0;  /* short is used to match termcap definition, ignore lint */

int tgetent(char *bp, const char *name) {
  /* Always succeed and ignore inputs. */
  return 1;
}

int tgetflag(char *id) {
  return 0;
}

int tgetnum(char *id) {
  if (strcmp(id, "co") == 0) return ConsoleGetWidth();
  if (strcmp(id, "li") == 0) return ConsoleGetHeight();
  return -1;
}

char *tgetstr(char *id, char **area) {
  return 0;
}

char *tgoto(const char *cap, int col, int row) {
  #define FORMAT_STRING_LIMIT 100
  static char dst_str[FORMAT_STRING_LIMIT];
  char *dst = dst_str;
  char ch;
  int values[2];
  int position;
  int tmp;

  /* NOT THREAD SAFE!!! */
  values[0] = row;
  values[1] = col;
  position = 0;
  while (*cap) {
    ch = *cap;
    cap++;
    if (ch == '%') {
      ch = *cap;
      cap++;
      if (ch == '%') {
        *dst = '%';
        dst++;
      } else if (ch == 'd') {
        snprintf(dst, FORMAT_STRING_LIMIT, "%d", values[position]);
        dst += strlen(dst);
        position++;
      } else if (ch == '2') {
        snprintf(dst, FORMAT_STRING_LIMIT, "%02d", values[position]);
        dst += strlen(dst);
        position++;
      } else if (ch == '3') {
        snprintf(dst, FORMAT_STRING_LIMIT, "%03d", values[position]);
        dst += strlen(dst);
        position++;
      } else if (ch == '.') {
        snprintf(dst, FORMAT_STRING_LIMIT, "%c", values[position]);
        dst += strlen(dst);
        position++;
      } else if (ch == 'r') {
        tmp = values[0];
        values[0] = values[1];
        values[1] = tmp;
      } else if (ch == 'i') {
        values[0]++;
        values[1]++;
      }
    } else {
      *dst = ch;
      dst++;
    }
  }
  *dst = 0;
  return dst_str;
}

int tputs(const char *str, int affcnt, int (*putc_)(int ch)) {
  while (*str) {
    putc_(*str);
    str++;
  }
  return 0;
}


/* TODO(bradnelson): These do nothing stubs are currently needed to cover the
 * range of calls that tests/vim needs to build. They probably don't belong
 * here, but should instead be in some sort of mock filesystem library.
 * For that matter they should actually do something...
 */


int chdir(const char *pathname) {
  /* always fail for now. */
  return -1;
}

int rmdir(const char *pathname) {
  /* always fail for now. */
  return -1;
}

uid_t getuid(void) {
  return 5001;
}

uid_t getgid(void) {
  return 5001;
}

int mkdir(const char *pathname, mode_t mode) {
  /* always fail for now. */
  return -1;
}

int chmod(const char *pathname, mode_t mode) {
  /* always fail for now. */
  return -1;
}

int access(const char *pathname, mode_t mode) {
  /* always fail for now. */
  return -1;
}

char *getwd(char *buf) {
  return "./";
}

mode_t umask(mode_t mask) {
  return mask;
}

int gethostname(char *name, size_t len) {
  snprintf(name, len, "nacl-host");
  return 0;
}

struct pollfd;
int poll(struct pollfd *fds, int nfds, int timeout) {
  return -1;
}

void sync(void) {
}

struct __sysctl_args;
int sysctl(struct __sysctl_args *args) {
  return -1;
}

struct sysinfo;
int sysinfo(struct sysinfo *info) {
  return -1;
}

