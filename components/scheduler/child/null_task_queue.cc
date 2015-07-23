// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/null_task_queue.h"

namespace scheduler {

NullTaskQueue::NullTaskQueue(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner) {}

NullTaskQueue::~NullTaskQueue() {}

bool NullTaskQueue::RunsTasksOnCurrentThread() const {
  return task_runner_->RunsTasksOnCurrentThread();
}

bool NullTaskQueue::PostDelayedTask(const tracked_objects::Location& from_here,
                                    const base::Closure& task,
                                    base::TimeDelta delay) {
  return task_runner_->PostDelayedTask(from_here, task, delay);
}

bool NullTaskQueue::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return task_runner_->PostNonNestableDelayedTask(from_here, task, delay);
}

bool NullTaskQueue::PostDelayedTaskAt(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeTicks desired_run_time) {
  // Note: this loses the guarantee of monotonicity but we can't do any better
  // with base::TaskRunner.
  return task_runner_->PostDelayedTask(
      from_here, task, desired_run_time - base::TimeTicks::Now());
}

bool NullTaskQueue::IsQueueEnabled() const {
  return true;
}

TaskQueue::QueueState NullTaskQueue::GetQueueState() const {
  return QueueState::EMPTY;
}

const char* NullTaskQueue::GetName() const {
  return "null_tq";
}

void NullTaskQueue::SetQueuePriority(QueuePriority priority) {}

void NullTaskQueue::PumpQueue() {}

void NullTaskQueue::SetPumpPolicy(PumpPolicy pump_policy) {}

}  // namespace scheduler
