// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/process_watcher.h"

#include <errno.h>
#include <signal.h>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/time.h"

namespace {

// Reap |child| process.
// This call blocks until completion.
void BlockingReap(pid_t child) {
  const pid_t result = HANDLE_EINTR(waitpid(child, NULL, 0));
  if (result == -1) {
    PLOG(ERROR) << "waitpid(" << child << ")";
    NOTREACHED();
  }
}

// Waits for |timeout| seconds for the given |child| to exit and reap it.
// If the child doesn't exit within a couple of seconds, kill it.
void WaitForChildToDie(pid_t child, unsigned timeout) {
  int kq = HANDLE_EINTR(kqueue());
  file_util::ScopedFD auto_close(&kq);
  if (kq == -1) {
    PLOG(ERROR) << "Failed to create kqueue";
    return;
  }

  struct kevent event_to_add = {0};
  EV_SET(&event_to_add, child, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
  // Register interest with kqueue.
  int result = HANDLE_EINTR(kevent(kq, &event_to_add, 1, NULL, 0, NULL));
  if (result == -1 && errno == ESRCH) {
    // A "No Such Process" error is fine, the process may have died already
    // and been reaped by someone else. But make sure that it was/is reaped.
    // Don't report an error in case it was already reaped.
    HANDLE_EINTR(waitpid(child, NULL, WNOHANG));
    return;
  }

  if (result == -1) {
    PLOG(ERROR) << "Failed to register event to listen for death of pid "
                << child;
    return;
  }

  struct kevent event = {0};

  DCHECK(timeout != 0);

  int num_processes_that_died = -1;
  using base::Time;
  using base::TimeDelta;
  // We need to keep track of the elapsed time - if kevent() returns
  // EINTR in the middle of blocking call we want to make up what's left
  // of the timeout.
  TimeDelta time_left = TimeDelta::FromSeconds(timeout);
  Time wakeup = Time::Now() + time_left;
  while(time_left.InMilliseconds() > 0) {
    const struct timespec timeout = time_left.ToTimeSpec();
    num_processes_that_died = kevent(kq, NULL, 0, &event, 1, &timeout);
    if (num_processes_that_died >= 0)
      break;
    if (num_processes_that_died == -1 && errno == EINTR) {
      time_left = wakeup - Time::Now();
      continue;
    }

    // If we got here, kevent() must have returned -1.
    PLOG(ERROR) << "kevent() failed";
    break;
  }

  if (num_processes_that_died == -1) {
    PLOG(ERROR) << "kevent failed";
    return;
  }
  if (num_processes_that_died == 1) {
    if (event.fflags & NOTE_EXIT &&
        event.ident == static_cast<uintptr_t>(child)) {
      // Process died, it's safe to make a blocking call here since the
      // kqueue() notification occurs when the process is already zombified.
      BlockingReap(child);
      return;
    } else {
      PLOG(ERROR) << "kevent() returned unexpected result - ke.fflags ="
                  << event.fflags
                  << " ke.ident ="
                  << event.ident
                  << " while listening for pid="
                  << child;
    }
  }

  // If we got here the child is still alive so kill it...
  if (kill(child, SIGKILL) == 0) {
    // SIGKILL is uncatchable. Since the signal was delivered, we can
    // just wait for the process to die now in a blocking manner.
    BlockingReap(child);
  } else {
    PLOG(ERROR) << "While waiting for " << child << " to terminate we"
               << " failed to deliver a SIGKILL signal";
  }
}

}  // namespace

void ProcessWatcher::EnsureProcessTerminated(base::ProcessHandle process) {
  WaitForChildToDie(process, 2);
}
