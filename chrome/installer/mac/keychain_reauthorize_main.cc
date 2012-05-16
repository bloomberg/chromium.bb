// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The entry point for the Mac Chrome Keychain Reauthorization process,
// which runs at update time. It needs to be signed by the old certificate
// in order to have access to the existing Keychain items, so it takes the
// form of this little stub that uses dlopen and dlsym to find a current
// Chrome framework, which can be signed by any certificate including the new
// one. This architecture allows the updater to peform keychain
// reauthorization by using an old copy of this executable signed with the old
// certificate even after the rest of Chrome has switched to being signed with
// the new certificate. The reauthorization code remains in the framework to
// avoid duplication and to allow it to change over time without having to
// re-sign this executable with the old certificate. This uses dlopen and
// dlsym to avoid problems linking with a library whose path is not fixed and
// whose version changes with each release.
//
// In order to satisfy the requirements of items stored in the Keychain, this
// executable needs to be named "com.google.Chrome" or
// "com.google.Chrome.canary", because the original applications were signed
// with deignated requirements requiring the identifier to be one of those
// names.

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__attribute__((visibility("default")))
int main(int argc, char* argv[]) {
  const char* me = argv[0];

  // Since |me| will be something like "com.google.Chrome", also use an
  // alternate name to avoid confusion.
  const char alt_me[] = "keychain_reauthorize";

  if (argc != 2) {
    fprintf(stderr, "usage: %s (%s) <framework_code_path>\n", me, alt_me);
    return 1;
  }

  const char* framework_code_path = argv[1];
  void* framework_code = dlopen(framework_code_path, RTLD_LAZY | RTLD_GLOBAL);
  if (!framework_code) {
    fprintf(stderr, "%s (%s): dlopen: %s\n", me, alt_me, dlerror());
    return 1;
  }

  typedef int(*ChromeMainType)(int, char**);
  ChromeMainType chrome_main =
      reinterpret_cast<ChromeMainType>(dlsym(framework_code, "ChromeMain"));
  if (!chrome_main) {
    fprintf(stderr, "%s (%s): dlsym: %s\n", me, alt_me, dlerror());
    return 1;
  }

  // Use strdup to get char* copies of the original const char* strings.
  // ChromeMain doesn't promise that it won't touch its argv.
  char* me_copy = strdup(me);
  char* keychain_reauthorize_argument = strdup("--keychain-reauthorize");
  char* chrome_main_argv[] = {
    me_copy,
    keychain_reauthorize_argument
  };

  int chrome_main_argc = sizeof(chrome_main_argv) / sizeof(chrome_main_argv[0]);

  // Not expected to return.
  int rv = chrome_main(chrome_main_argc, chrome_main_argv);

  fprintf(stderr, "%s (%s): NOTREACHED!\n", me, alt_me);

  free(keychain_reauthorize_argument);
  free(me_copy);

  // As in chrome_exe_main_mac.cc: exit, don't return from main, to avoid the
  // apparent removal of main from stack backtraces under tail call
  // optimization.
  exit(rv);
}
