// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/process_watcher.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/win/object_watcher.h"
#include "content/common/result_codes.h"

// Maximum amount of time (in milliseconds) to wait for the process to exit.
static const int kWaitInterval = 2000;

namespace {

class TimerExpiredTask : public Task,
                         public base::win::ObjectWatcher::Delegate {
 public:
  explicit TimerExpiredTask(base::ProcessHandle process) : process_(process) {
    watcher_.StartWatching(process_, this);
  }

  virtual ~TimerExpiredTask() {
    if (process_) {
      KillProcess();
      DCHECK(!process_) << "Make sure to close the handle.";
    }
  }

  // Task ---------------------------------------------------------------------

  virtual void Run() {
    if (process_)
      KillProcess();
  }

  // MessageLoop::Watcher -----------------------------------------------------

  virtual void OnObjectSignaled(HANDLE object) {
    // When we're called from KillProcess, the ObjectWatcher may still be
    // watching.  the process handle, so make sure it has stopped.
    watcher_.StopWatching();

    CloseHandle(process_);
    process_ = NULL;
  }

 private:
  void KillProcess() {
    // OK, time to get frisky.  We don't actually care when the process
    // terminates.  We just care that it eventually terminates, and that's what
    // TerminateProcess should do for us. Don't check for the result code since
    // it fails quite often. This should be investigated eventually.
    base::KillProcess(process_, content::RESULT_CODE_HUNG, false);

    // Now, just cleanup as if the process exited normally.
    OnObjectSignaled(process_);
  }

  // The process that we are watching.
  base::ProcessHandle process_;

  base::win::ObjectWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(TimerExpiredTask);
};

}  // namespace

// static
void ProcessWatcher::EnsureProcessTerminated(base::ProcessHandle process) {
  DCHECK(process != GetCurrentProcess());

  // If already signaled, then we are done!
  if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
    CloseHandle(process);
    return;
  }

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new TimerExpiredTask(process),
                                          kWaitInterval);
}
