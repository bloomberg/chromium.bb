/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <string.h>

#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_console.h"

#include "ppapi_main/ppapi_main.h"

// The default arguments to PPAPI_MAIN maps:
//   STDIN -> /dev/stdin
//   STDOUT -> /dev/stdout
//   STDERR -> /dev/console3
// We use our own args here so that stdout sends messages to JavaScript via
// PostMessage (/dev/tty).
PPAPI_MAIN_WITH_ARGS("pm_stdout", "/dev/tty", NULL, NULL)

//
// The "main" entry point called by PPAPIInstance once initialization
// takes place.  This is called off the main thread, which is hidden
// from the developer, making it safe to use blocking calls.
// The arguments are provided as:
//   argv[0] = "NEXE"
//   argv[1] = "--<KEY>"
//   argv[2] = "<VALUE>"
// Where the embed tag for this module uses KEY=VALUE
//
int ppapi_main(int argc, const char* argv[]) {
  int index = 1;

  // Use PostMessage to send "Hello World" to JavaScript.
  printf("Hello World STDOUT.\n");

  // Use PPAPI Console interface to send "Hello World" to the
  // JavaScript Console.
  fprintf(stderr, "Hello World STDERR.\n");

  // Print the arguments we received from the web page
  printf("NAME: %s\n", argv[0]);
  while (index + 2 < argc) {
    printf("  ARGS: %s=%s\n", argv[index + 0], argv[index + 1]);
    index += 2;
  }
  return 0;
}
