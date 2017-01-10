// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LIBC_CLOSE_TRACKING_H_
#define CHROME_BROWSER_CHROMEOS_LIBC_CLOSE_TRACKING_H_

// Debugging code for http://crbug.com/660960 where close() fails with EBADF.
// The primary suspicion is that some piece of code is closing a FD that does
// not belong to it. Unfortunately the PCHECK in ScopedFDCloseTraits::Free
// is too late and reports only victim rather than the culprit. Another
// possibility is memory corruption (or object holding fd released and its
// memory got over-written). In such case, LOG(FATAL) includes fd and errno
// and it should help us to identify the case.
//
// The code here interposes libc close() calls to track the last good calls
// on a fd and reports it when libc close() on it fails.
//
// Note:
// 1. The libc close() interposition happens at build time by exporting
//    "close" symbol and alias it to our CloseOverride function. All close()
//    calls will be redirected regardless whether stack tracking is enabled
//    or not.
// 2. Interposition captures libc close() calls but not direct syscalls.
//    To capture direct syscalls, ptrace is needed and it needs to run in a
//    master process with chrome browser process as its debuggee.  This would
//    need a complicated setup and introduce significant overhead as all close
//    calls are hooked to the master process to extract stacks etc.  Since
//    most (all?) chrome code calls libc close() instead of direct syscalls,
//    the interposition way is a better first step.
//
// TODO(xiyuan): Remove after http://crbug.com/660960.

namespace base {
namespace debug {
class StackTrace;
}  // namespace debug
}  // namespace base

namespace chromeos {

// Enables the stack tracking for the process where InitCloseTracking gets
// called (this should be the browser process) so that last good close stacks
// are collected and sent along with crash reports.
void InitCloseTracking();

// Cleans up and disables the stack tracking for close().
void ShutdownCloseTracking();

// Gets the last close stack trace for the given fd.
const base::debug::StackTrace* GetLastCloseStackForTest(int fd);

// Sets the boolean flag to ignore pid check for tracking in test.
void SetCloseTrackingIgnorePidForTest(bool ignore_pid);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LIBC_CLOSE_TRACKING_H_
