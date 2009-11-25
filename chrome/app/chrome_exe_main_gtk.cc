// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/process_util.h"

// The entry point for all invocations of Chromium, browser and renderer. On
// windows, this does nothing but load chrome.dll and invoke its entry point in
// order to make it easy to update the app from GoogleUpdate. We don't need
// that extra layer with on linux.
//
// TODO(tc): This is similar to chrome_exe_main.mm.  After it's more clear what
// needs to go here, we should evaluate whether or not to merge this file with
// chrome_exe_main.mm.

extern "C" {
int ChromeMain(int argc, const char** argv);

#if defined(LINUX_USE_TCMALLOC)

int tc_set_new_mode(int mode);

#endif  // defined(LINUX_USE_TCMALLOC)

}

int main(int argc, const char** argv) {
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();

  // NOTE(willchan): One might ask why this call is done here rather than in
  // process_util_linux.cc with the definition of
  // EnableTerminationOnOutOfMemory().  That's because base shouldn't have a
  // dependency on TCMalloc.  Really, we ought to have our allocator shim code
  // implement this EnableTerminationOnOutOfMemory() function.  Whateverz.  This
  // works for now.
#if defined(LINUX_USE_TCMALLOC)
  // For tcmalloc, we need to tell it to behave like new.
  tc_set_new_mode(1);
#endif

  // The exit manager is in charge of calling the dtors of singletons.
  // Win has one here, but we assert with multiples from BrowserMain() if we
  // keep it.
  // base::AtExitManager exit_manager;

#if defined(GOOGLE_CHROME_BUILD)
  // TODO(tc): init crash reporter
#endif

  return ChromeMain(argc, argv);
}
