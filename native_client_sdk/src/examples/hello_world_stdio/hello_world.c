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


// Have the Module object provided by ppapi_main create a basic
// PPAPI instance with default arguments which mounts the dev
// file system providing /dev/null, /dev/tty, and /devl/console3
// for null STDIN, STDOUT directed to PostMessage and STDERR
// directed to the JavaScript Console with LogLevel 'ERROR'
PPAPI_MAIN_WITH_DEFAULT_ARGS

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
int ppapi_main(int argc, const char *argv[]) {
  int index = 1;

  // Use PostMessage to send "Hello World" to JavaScript.
  printf("Hello World STDIO.\n");
  fflush(stdout);

  // Use PPAPI Console interface to send "Hello World" to the
  // JavaScript Console.
  fprintf(stderr, "Hello World STDERR.\n");

  // Print the arguments we received from the web page
  printf("NAME: %s\n", argv[0]);
  while (index + 2 < argc) {
    printf("  ARGS: %s=%s\n", argv[index+0], argv[index+1]);
    index += 2;
  }
  fflush(stdout);
  return 0;
}
