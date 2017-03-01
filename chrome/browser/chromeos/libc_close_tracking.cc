// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <sstream>
#include <string>
#include <unordered_map>

#include "base/debug/crash_logging.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "chrome/common/crash_keys.h"

namespace {

// Captures errno on construction and restores it on destruction.
class ErrnoCapture {
 public:
  ErrnoCapture() : captured_errno_(errno) {}
  ~ErrnoCapture() { errno = captured_errno_; }

  int captured_errno() const { return captured_errno_; }

 private:
  const int captured_errno_;

  DISALLOW_COPY_AND_ASSIGN(ErrnoCapture);
};

// Tracks per-FD stack traces on successful close() call in the process where
// it gets created.
class StackTracker {
 public:
  StackTracker() : creation_process_id_(base::GetCurrentProcId()) {}
  ~StackTracker() = default;

  // Whether the current process should be tracked.
  bool ShouldTrack() const {
    return ignore_pid_ || base::GetCurrentProcId() == creation_process_id_;
  }

  // Stores the stack associated with |fd|.
  void SetStack(int fd, const base::debug::StackTrace& stack) {
    base::AutoLock lock(lock_);
    stacks_[fd] = stack;
  }

  // Retrieves the stack associated with |fd|. Returns nullptr if no stack
  // is associated with it.
  const base::debug::StackTrace* GetStack(int fd) {
    base::AutoLock lock(lock_);

    auto it = stacks_.find(fd);
    return it != stacks_.end() ? &(it->second) : nullptr;
  }

  void set_ignore_pid(bool ignore_pid) { ignore_pid_ = ignore_pid; }

 private:
  // Process id of the creation process.
  const base::ProcessId creation_process_id_;

  bool ignore_pid_ = false;

  // Guard access to |stacks_|.
  base::Lock lock_;

  // Tracks per-FD call stacks of the last successful close() calls.
  std::unordered_map<int, base::debug::StackTrace> stacks_;

  DISALLOW_COPY_AND_ASSIGN(StackTracker);
};

// This could not be a LazyInstance because it is used in CloseOverride called
// during early process start before base code is initialized and LazyInstance
// also depends on AtExitManager, which needs to call close() during clean up.
StackTracker* g_stack_tracker = nullptr;

// Creates a stack string without symbolizing since the code is targeted
// for debugging release code that has no symbols. StackTrace::ToString()
// could not be used because it calls close() and CloseOverride currently
// does not handle recursive close() calls.
std::string GetStackString(const base::debug::StackTrace& stack) {
  size_t count = 0;
  const void* const* addresses = stack.Addresses(&count);
  if (!count || !addresses)
    return "<null>";

  std::stringstream stream;
  for (size_t i = 0; i < count; ++i)
    stream << base::StringPrintf("%p ", addresses[i]);

  return stream.str();
}

}  // namespace

extern "C" {

// Implementation of the overridden libc close() call.
int CloseOverride(int fd) {
  // Call real close via syscall.
  const int ret = IGNORE_EINTR(syscall(SYS_close, fd));
  // Captures errno and restores it later in case it is changed.
  const ErrnoCapture close_errno;

  // Return if tracking is not enabled for the current process.
  if (!g_stack_tracker || !g_stack_tracker->ShouldTrack())
    return ret;

  // Capture stack for successful close.
  if (ret == 0) {
#if HAVE_TRACE_STACK_FRAME_POINTERS && !defined(MEMORY_SANITIZER)
    // Use TraceStackFramePointers because the backtrack() based default
    // capturing gets only the last stack frame and is not useful.
    // With the exception of when MSAN is enabled. See comments for why
    // StackTraceTest.TraceStackFramePointers is disabled in MSAN builds.
    const void* frames[64];
    const size_t frame_count =
        base::debug::TraceStackFramePointers(frames, arraysize(frames), 0);
    g_stack_tracker->SetStack(fd, base::debug::StackTrace(frames, frame_count));
#else
    // Use default StackTrace when TraceStackFramePointers is not available.
    // Hopefully it will capture something useful.
    g_stack_tracker->SetStack(fd, base::debug::StackTrace());
#endif
    return ret;
  }

  // Only capture EBADF. Let caller handle other errors.
  if (close_errno.captured_errno() != EBADF)
    return ret;

  const base::debug::StackTrace* const last_close_stack =
      g_stack_tracker->GetStack(fd);
  if (!last_close_stack) {
    // No previous stack. Return and let existing code to catch it.
    return ret;
  }

  // Crash with the stack of the last success close on the fd.
  base::debug::SetCrashKeyToStackTrace(crash_keys::kLastGoodCloseStack,
                                       *last_close_stack);
  LOG(FATAL) << "Failed to close a fd, attach info to http://crbug.com/660960"
             << ", fd=" << fd << ", errno=" << close_errno.captured_errno()
             << ", stack=" << GetStackString(*last_close_stack);
  return ret;
}

// Export "close" symbol and make it an alias of "CloseOverride" to interpose
// libc close().
int close(int fd)
    __attribute__((alias("CloseOverride"), visibility("default")));

}  // extern "C"

namespace chromeos {

void InitCloseTracking() {
  DCHECK(!g_stack_tracker);
  g_stack_tracker = new StackTracker;
}

void ShutdownCloseTracking() {
  delete g_stack_tracker;
  g_stack_tracker = nullptr;
}

const base::debug::StackTrace* GetLastCloseStackForTest(int fd) {
  return g_stack_tracker ? g_stack_tracker->GetStack(fd) : nullptr;
}

void SetCloseTrackingIgnorePidForTest(bool ignore_pid) {
  DCHECK(g_stack_tracker);
  g_stack_tracker->set_ignore_pid(ignore_pid);
}

}  // namespace chromeos
