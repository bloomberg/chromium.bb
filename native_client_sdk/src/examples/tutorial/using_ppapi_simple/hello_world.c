/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <string.h>

#include "ppapi_simple/ps_main.h"

int example_main(int argc, char* argv[]) {
  /* Use ppb_messaging to send "Hello World" to JavaScript. */
  printf("Hello World STDOUT.\n");

  /* Use ppb_console send "Hello World" to the JavaScript Console. */
  fprintf(stderr, "Hello World STDERR.\n");
  return 0;
}

/*
 * Register the function to call once the Instance Object is initialized.
 * see: pappi_simple/ps_main.h
 *
 * This is not needed when building the sel_ldr version of this example
 * which does not link against ppapi_simple.
 */
PPAPI_SIMPLE_REGISTER_MAIN(example_main)
