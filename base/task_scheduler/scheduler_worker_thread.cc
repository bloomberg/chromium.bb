// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/scheduler_worker_thread.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/task_scheduler/task_tracker.h"

namespace base {
namespace internal {

std::unique_ptr<SchedulerWorkerThread>
SchedulerWorkerThread::CreateSchedulerWorkerThread(
    ThreadPriority thread_priority,
    Delegate* delegate,
    TaskTracker* task_tracker) {
  std::unique_ptr<SchedulerWorkerThread> worker_thread(
      new SchedulerWorkerThread(thread_priority, delegate, task_tracker));

  if (worker_thread->thread_handle_.is_null())
    return nullptr;
  return worker_thread;
}

SchedulerWorkerThread::~SchedulerWorkerThread() {
  DCHECK(ShouldExitForTesting());
}

void SchedulerWorkerThread::WakeUp() {
  wake_up_event_.Signal();
}

void SchedulerWorkerThread::JoinForTesting() {
  {
    AutoSchedulerLock auto_lock(should_exit_for_testing_lock_);
    should_exit_for_testing_ = true;
  }
  WakeUp();
  PlatformThread::Join(thread_handle_);
}

SchedulerWorkerThread::SchedulerWorkerThread(ThreadPriority thread_priority,
                                             Delegate* delegate,
                                             TaskTracker* task_tracker)
    : wake_up_event_(false, false),
      delegate_(delegate),
      task_tracker_(task_tracker) {
  DCHECK(delegate_);
  DCHECK(task_tracker_);

  const size_t kDefaultStackSize = 0;
  PlatformThread::CreateWithPriority(kDefaultStackSize, this, &thread_handle_,
                                     thread_priority);
}

void SchedulerWorkerThread::ThreadMain() {
  delegate_->OnMainEntry();

  // A SchedulerWorkerThread starts out sleeping.
  wake_up_event_.Wait();

  while (!task_tracker_->shutdown_completed() && !ShouldExitForTesting()) {
    // Get the sequence containing the next task to execute.
    scoped_refptr<Sequence> sequence = delegate_->GetWork(this);

    if (!sequence) {
      wake_up_event_.Wait();
      continue;
    }

    task_tracker_->RunTask(sequence->PeekTask());

    const bool sequence_became_empty = sequence->PopTask();

    // If |sequence| isn't empty immediately after the pop, re-enqueue it to
    // maintain the invariant that a non-empty Sequence is always referenced by
    // either a PriorityQueue or a SchedulerWorkerThread. If it is empty and
    // there are live references to it, it will be enqueued when a Task is added
    // to it. Otherwise, it will be destroyed at the end of this scope.
    if (!sequence_became_empty)
      delegate_->ReEnqueueSequence(std::move(sequence));

    // Calling WakeUp() guarantees that this SchedulerWorkerThread will run
    // Tasks from Sequences returned by the GetWork() method of |delegate_|
    // until it returns nullptr. Resetting |wake_up_event_| here doesn't break
    // this invariant and avoids a useless loop iteration before going to sleep
    // if WakeUp() is called while this SchedulerWorkerThread is awake.
    wake_up_event_.Reset();
  }
}

bool SchedulerWorkerThread::ShouldExitForTesting() const {
  AutoSchedulerLock auto_lock(should_exit_for_testing_lock_);
  return should_exit_for_testing_;
}

}  // namespace internal
}  // namespace base
