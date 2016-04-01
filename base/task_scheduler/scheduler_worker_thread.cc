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

scoped_ptr<SchedulerWorkerThread>
SchedulerWorkerThread::CreateSchedulerWorkerThread(
    ThreadPriority thread_priority,
    const Closure& main_entry_callback,
    const GetWorkCallback& get_work_callback,
    const RanTaskFromSequenceCallback& ran_task_from_sequence_callback,
    TaskTracker* task_tracker) {
  scoped_ptr<SchedulerWorkerThread> worker_thread(new SchedulerWorkerThread(
      thread_priority, main_entry_callback, get_work_callback,
      ran_task_from_sequence_callback, task_tracker));

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

SchedulerWorkerThread::SchedulerWorkerThread(
    ThreadPriority thread_priority,
    const Closure& main_entry_callback,
    const GetWorkCallback& get_work_callback,
    const RanTaskFromSequenceCallback& ran_task_from_sequence_callback,
    TaskTracker* task_tracker)
    : wake_up_event_(false, false),
      main_entry_callback_(main_entry_callback),
      get_work_callback_(get_work_callback),
      ran_task_from_sequence_callback_(ran_task_from_sequence_callback),
      task_tracker_(task_tracker) {
  DCHECK(!main_entry_callback_.is_null());
  DCHECK(!get_work_callback_.is_null());
  DCHECK(!ran_task_from_sequence_callback_.is_null());
  DCHECK(task_tracker_);

  static const size_t kDefaultStackSize = 0;
  PlatformThread::CreateWithPriority(kDefaultStackSize, this, &thread_handle_,
                                     thread_priority);
}

void SchedulerWorkerThread::ThreadMain() {
  main_entry_callback_.Run();

  // A SchedulerWorkerThread starts out sleeping.
  wake_up_event_.Wait();

  while (!task_tracker_->shutdown_completed() && !ShouldExitForTesting()) {
    // Get the sequence containing the next task to execute.
    scoped_refptr<Sequence> sequence = get_work_callback_.Run(this);

    if (!sequence) {
      wake_up_event_.Wait();
      continue;
    }

    task_tracker_->RunTask(sequence->PeekTask());
    ran_task_from_sequence_callback_.Run(this, std::move(sequence));

    // Calling WakeUp() guarantees that this SchedulerWorkerThread will run
    // Tasks from Sequences returned by |get_work_callback_| until the callback
    // returns nullptr. Resetting |wake_up_event_| here doesn't break this
    // invariant and avoids a useless loop iteration before going to sleep if
    // WakeUp() is called while this SchedulerWorkerThread is awake.
    wake_up_event_.Reset();
  }
}

bool SchedulerWorkerThread::ShouldExitForTesting() const {
  AutoSchedulerLock auto_lock(should_exit_for_testing_lock_);
  return should_exit_for_testing_;
}

}  // namespace internal
}  // namespace base
