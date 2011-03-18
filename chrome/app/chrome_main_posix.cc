// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main.h"

#include <signal.h>

#include "base/global_descriptors_posix.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/common/chrome_descriptors.h"

#if defined(OS_MACOSX)
#include "chrome/app/breakpad_mac.h"
#endif

namespace {

// Setup signal-handling state: resanitize most signals, ignore SIGPIPE.
void SetupSignalHandlers() {
  // Sanitise our signal handling state. Signals that were ignored by our
  // parent will also be ignored by us. We also inherit our parent's sigmask.
  sigset_t empty_signal_set;
  CHECK(0 == sigemptyset(&empty_signal_set));
  CHECK(0 == sigprocmask(SIG_SETMASK, &empty_signal_set, NULL));

  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIG_DFL;
  static const int signals_to_reset[] =
      {SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, SIGFPE, SIGSEGV,
       SIGALRM, SIGTERM, SIGCHLD, SIGBUS, SIGTRAP};  // SIGPIPE is set below.
  for (unsigned i = 0; i < arraysize(signals_to_reset); i++) {
    CHECK(0 == sigaction(signals_to_reset[i], &sigact, NULL));
  }

  // Always ignore SIGPIPE.  We check the return value of write().
  CHECK(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
}

}  // anonymous namespace

namespace chrome_main {

void LowLevelInit(void* instance) {
#if defined(OS_MACOSX)
  // TODO(mark): Some of these things ought to be handled in
  // chrome_exe_main_mac.mm.  Under the current architecture, nothing
  // in chrome_exe_main can rely directly on chrome_dll code on the
  // Mac, though, so until some of this code is refactored to avoid
  // such a dependency, it lives here.  See also the TODO(mark)
  // at InitCrashReporter() and DestructCrashReporter().
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
#endif  // OS_MACOSX

  // Set C library locale to make sure CommandLine can parse argument values
  // in correct encoding.
  setlocale(LC_ALL, "");

  SetupSignalHandlers();

  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  g_fds->Set(kPrimaryIPCChannel,
             kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor);
#if defined(OS_LINUX)
  g_fds->Set(kCrashDumpSignal,
             kCrashDumpSignal + base::GlobalDescriptors::kBaseDescriptor);
#endif
}

void LowLevelShutdown() {
#if defined(OS_MACOSX) && defined(GOOGLE_CHROME_BUILD)
  // TODO(mark): See the TODO(mark) at InitCrashReporter.
  DestructCrashReporter();
#endif  // OS_MACOSX && GOOGLE_CHROME_BUILD
}

#if !defined(OS_MACOSX)
// This policy is not implemented for Linux and ChromeOS. The win and mac ver-
// sions are implemented in the platform specific version of chrome_main.cc e.g.
// chrome_main_win.cc or chrome_main_mac.mm .
void CheckUserDataDirPolicy(FilePath* user_data_dir) {}
#endif

}  // namespace chrome_main
