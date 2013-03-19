// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/first_run/upgrade_util.h"

// The entry point for all invocations of Chromium, browser and renderer. On
// windows, this does nothing but load chrome.dll and invoke its entry point in
// order to make it easy to update the app from GoogleUpdate. We don't need
// that extra layer with on linux.

#if defined(ADDRESS_SANITIZER) && defined(GOOGLE_CHROME_BUILD)
// Default AddressSanitizer options: limit the quarantine to 1Gb, disable the
// strict memcmp() checking (http://crbug.com/178677 and
// http://crbug.com/178404).
const char *kAsanDefaultOptions = "quarantine_size=1048576 strict_memcmp=0";

// Override the default ASan options for the Google Chrome executable.
// __asan_default_options should not be instrumented, because it is called
// before ASan is initialized.
extern "C"
__attribute__((no_address_safety_analysis))
const char *__asan_default_options() {
  return kAsanDefaultOptions;
}
#endif

extern "C" {
int ChromeMain(int argc, const char** argv);
}

int main(int argc, const char** argv) {
  int return_code = ChromeMain(argc, argv);

#if defined(OS_LINUX)
  // Launch a new instance if we're shutting down because we detected an
  // upgrade in the persistent mode.
  upgrade_util::RelaunchChromeBrowserWithNewCommandLineIfNeeded();
#endif

  return return_code;
}
