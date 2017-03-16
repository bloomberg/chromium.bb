// Copyright 2017 The Chromium Authors. All rights reserved.
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
// the new certificate. The reauthorization dylib remains in the framework to
// avoid duplication and to allow it to change over time without having to
// re-sign this executable with the old certificate. This uses dlopen and
// dlsym to avoid problems linking with a library whose path is not fixed and
// whose version changes with each release.
//
// In order to satisfy the requirements of items stored in the Keychain, this
// executable needs to be named "com.google.Chrome", or
// "com.google.Chrome.canary", because the original applications were signed
// with designated requirements requiring the identifier to be one of those
// names.

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CoreFoundation/CoreFoundation.h>

__attribute__((visibility("default"))) int main(int argc, char* argv[]) {
  const char* me = argv[0];

  if (argc != 2) {
    fprintf(stderr, "usage: %s <framework_dylib_path>\n", me);
    return 1;
  }

  const char* framework_dylib_path = argv[1];
  void* framework_dylib = dlopen(framework_dylib_path, RTLD_LAZY | RTLD_GLOBAL);
  if (!framework_dylib) {
    fprintf(stderr, "%s: dlopen: %s\n", me, dlerror());
    return 1;
  }

  CFStringRef keychain_reauthorize_pref =
      CFSTR("KeychainReauthorizeInAppSpring2017");
  const int keychain_reauthorize_max_tries = 3;

  typedef void (*KeychainReauthorizeIfNeeded)(CFStringRef, int);
  KeychainReauthorizeIfNeeded keychain_reauth =
      reinterpret_cast<KeychainReauthorizeIfNeeded>(
          dlsym(framework_dylib, "KeychainReauthorizeIfNeededAtUpdate"));
  if (!keychain_reauth) {
    fprintf(stderr, "%s: dlsym: %s\n", me, dlerror());
    return 1;
  }

  keychain_reauth(keychain_reauthorize_pref, keychain_reauthorize_max_tries);

  // As in chrome_exe_main_mac.cc: exit, don't return from main, to avoid the
  // apparent removal of main from stack backtraces under tail call
  // optimization.
  exit(0);
}
