// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#import <Foundation/Foundation.h>
#include <stdio.h>
#include <sys/resource.h>

#include "base/logging.h"

// This is just enough of a shim to let the support needed by test_support
// link.  In general, process_util isn't valid on iOS.

namespace base {

ProcessId GetCurrentProcId() {
  return getpid();
}

ProcessHandle GetCurrentProcessHandle() {
  return GetCurrentProcId();
}

void EnableTerminationOnHeapCorruption() {
  // On iOS, there nothing to do AFAIK.
}

void EnableTerminationOnOutOfMemory() {
  // iOS provides this for free!
}

void RaiseProcessToHighPriority() {
  // Impossible on iOS. Do nothing.
}

size_t GetMaxFds() {
  static const rlim_t kSystemDefaultMaxFds = 256;
  rlim_t max_fds;
  struct rlimit nofile;
  if (getrlimit(RLIMIT_NOFILE, &nofile)) {
    // Error case: Take a best guess.
    max_fds = kSystemDefaultMaxFds;
  } else {
    max_fds = nofile.rlim_cur;
  }

  if (max_fds > INT_MAX)
    max_fds = INT_MAX;

  return static_cast<size_t>(max_fds);
}

}  // namespace base
