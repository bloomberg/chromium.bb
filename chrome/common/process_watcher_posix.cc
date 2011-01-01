// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/process_watcher.h"

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"

// Return true if the given child is dead. This will also reap the process.
// Doesn't block.
static bool IsChildDead(pid_t child) {
  const pid_t result = HANDLE_EINTR(waitpid(child, NULL, WNOHANG));
  if (result == -1) {
    PLOG(ERROR) << "waitpid(" << child << ")";
    NOTREACHED();
  } else if (result > 0) {
    // The child has died.
    return true;
  }

  return false;
}

// A thread class which waits for the given child to exit and reaps it.
// If the child doesn't exit within a couple of seconds, kill it.
class BackgroundReaper : public base::PlatformThread::Delegate {
 public:
  explicit BackgroundReaper(pid_t child, unsigned timeout)
      : child_(child),
        timeout_(timeout) {
  }

  void ThreadMain() {
    WaitForChildToDie();
    delete this;
  }

  void WaitForChildToDie() {
    // Wait forever case.
    if (timeout_ == 0) {
      pid_t r = HANDLE_EINTR(waitpid(child_, NULL, 0));
      if (r != child_) {
        PLOG(ERROR) << "While waiting for " << child_
                    << " to terminate, we got the following result: " << r;
      }
      return;
    }

    // There's no good way to wait for a specific child to exit in a timed
    // fashion. (No kqueue on Linux), so we just loop and sleep.

    // Wait for 2 * timeout_ 500 milliseconds intervals.
    for (unsigned i = 0; i < 2 * timeout_; ++i) {
      base::PlatformThread::Sleep(500);  // 0.5 seconds
      if (IsChildDead(child_))
        return;
    }

    if (kill(child_, SIGKILL) == 0) {
      // SIGKILL is uncatchable. Since the signal was delivered, we can
      // just wait for the process to die now in a blocking manner.
      if (HANDLE_EINTR(waitpid(child_, NULL, 0)) < 0)
        PLOG(WARNING) << "waitpid";
    } else {
      LOG(ERROR) << "While waiting for " << child_ << " to terminate we"
                 << " failed to deliver a SIGKILL signal (" << errno << ").";
    }
  }

 private:
  const pid_t child_;
  // Number of seconds to wait, if 0 then wait forever and do not attempt to
  // kill |child_|.
  const unsigned timeout_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundReaper);
};

// static
void ProcessWatcher::EnsureProcessTerminated(base::ProcessHandle process) {
  // If the child is already dead, then there's nothing to do.
  if (IsChildDead(process))
    return;

  const unsigned timeout = 2;  // seconds
  BackgroundReaper* reaper = new BackgroundReaper(process, timeout);
  base::PlatformThread::CreateNonJoinable(0, reaper);
}

// static
void ProcessWatcher::EnsureProcessGetsReaped(base::ProcessHandle process) {
  // If the child is already dead, then there's nothing to do.
  if (IsChildDead(process))
    return;

  BackgroundReaper* reaper = new BackgroundReaper(process, 0);
  base::PlatformThread::CreateNonJoinable(0, reaper);
}
