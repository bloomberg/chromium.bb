// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The entry point for all Mac Chromium processes, including the outer app
// bundle (browser) and helper app (renderer, plugin, and friends).

#if defined(ADDRESS_SANITIZER)
#include <crt_externs.h>  // for _NSGetArgc, _NSGetArgv
#include <string.h>
#endif  // ADDRESS_SANITIZER
#include <stdlib.h>

#if defined(ADDRESS_SANITIZER)
// NaCl requires its own SEGV handler, so we need to add handle_segv=0 to
// ASAN_OPTIONS. This is done by injecting __asan_default_options into the
// executable.
// Because there's no distinct NaCl executable on OSX, we have to look at the
// command line arguments to understand whether the process is a NaCl loader.

static const char kNaClDefaultOptions[] = "handle_segv=0";
static const char kNaClFlag[] = "--type=nacl-loader";

extern "C"
// __asan_default_options() is called at ASan initialization, so it must
// not be instrumented with ASan -- thus the "no_sanitize_address" attribute.
__attribute__((no_sanitize_address))
// The function isn't referenced from the executable itself. Make sure it isn't
// stripped by the linker.
__attribute__((used))
__attribute__((visibility("default")))
const char* __asan_default_options()  {
  char*** argvp = _NSGetArgv();
  int* argcp = _NSGetArgc();
  if (!argvp || !argcp) return NULL;
  char** argv = *argvp;
  int argc = *argcp;
  for (int i = 0; i < argc; ++i) {
    if (strcmp(argv[i], kNaClFlag) == 0) {
      return kNaClDefaultOptions;
    }
  }
  return NULL;
}
#endif  // ADDRESS_SANITIZER

extern "C" {
int ChromeMain(int argc, char** argv);
}  // extern "C"

__attribute__((visibility("default")))
int main(int argc, char* argv[]) {
  int rv = ChromeMain(argc, argv);

  // exit, don't return from main, to avoid the apparent removal of main from
  // stack backtraces under tail call optimization.
  exit(rv);
}
