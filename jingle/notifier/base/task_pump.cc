// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "jingle/notifier/base/task_pump.h"

namespace notifier {

TaskPump::TaskPump()
    : scoped_runnable_method_factory_(
        ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      posted_wake_(false) {}

TaskPump::~TaskPump() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

void TaskPump::WakeTasks() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!posted_wake_) {
    MessageLoop* current_message_loop = MessageLoop::current();
    CHECK(current_message_loop);
    // Do the requested wake up.
    current_message_loop->PostTask(
        FROM_HERE,
        scoped_runnable_method_factory_.NewRunnableMethod(
            &TaskPump::CheckAndRunTasks));
    posted_wake_ = true;
  }
}

int64 TaskPump::CurrentTime() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  // Only timeout tasks rely on this function.  Since we're not using
  // libjingle tasks for timeout, it's safe to return 0 here.
  return 0;
}

void TaskPump::CheckAndRunTasks() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  posted_wake_ = false;
  // We shouldn't be using libjingle for timeout tasks, so we should
  // have no timeout tasks at all.

  // TODO(akalin): Add HasTimeoutTask() back in TaskRunner class and
  // uncomment this check.
  // DCHECK(!HasTimeoutTask())
  RunTasks();
}

}  // namespace notifier
