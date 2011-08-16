/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Bootstraping the nacl_helper. This executable reserves the bottom 1G
 * of the address space, then invokes nacl_helper_init. Note that,
 * as the text of this executable will eventually be overwritten by the
 * native_client module, nacl_helper_init must not attempt to return.
 */

#include <stdlib.h>

/* reserve 1GB of space */
#define ONEGIG (1 << 30)
char nacl_reserved_space[ONEGIG];

void nacl_helper_init(int argc, char *argv[],
                      const char *nacl_reserved_space);

int main(int argc, char *argv[]) {
  nacl_helper_init(argc, argv, nacl_reserved_space);
  abort();
  return 0;  // convince the tools I'm sane.
}
