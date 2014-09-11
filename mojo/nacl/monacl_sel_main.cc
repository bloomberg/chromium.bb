// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/nacl/monacl_sel_main.h"

#include <stdio.h>

#include "mojo/nacl/mojo_syscall.h"
#include "native_client/src/public/chrome_main.h"
#include "native_client/src/public/nacl_app.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"

namespace mojo {

void LaunchNaCl(const char* nexe_file, const char* irt_file,
                int app_argc, char* app_argv[]) {
  NaClChromeMainInit();

  // Open the IRT.
  struct NaClDesc* irt_desc = (struct NaClDesc*) NaClDescIoDescOpen(
      irt_file, NACL_ABI_O_RDONLY, 0);
  if (NULL == irt_desc) {
    perror(irt_file);
    exit(1);
  }

  // Open the main executable.
  struct NaClDesc* nexe_desc = (struct NaClDesc*) NaClDescIoDescOpen(
      nexe_file, NACL_ABI_O_RDONLY, 0);
  if (NULL == nexe_desc) {
    perror(nexe_file);
    exit(1);
  }

  struct NaClChromeMainArgs* args = NaClChromeMainArgsCreate();
  args->nexe_desc = nexe_desc;
  args->irt_desc = irt_desc;

  args->argc = app_argc;
  args->argv = app_argv;

  struct NaClApp* nap = NaClAppCreate();
  InjectMojo(nap);

  int exit_status = 1;
  NaClChromeMainStart(nap, args, &exit_status);
  NaClExit(exit_status);
}

} // namespace mojo
