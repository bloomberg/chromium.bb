// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/scheduler_task_runner_delegate_impl.h"

namespace scheduler {

// static
scoped_refptr<SchedulerTaskRunnerDelegateImpl>
SchedulerTaskRunnerDelegateImpl::Create(base::MessageLoop* message_loop) {
  return make_scoped_refptr(new SchedulerTaskRunnerDelegateImpl(message_loop));
}

SchedulerTaskRunnerDelegateImpl::SchedulerTaskRunnerDelegateImpl(
    base::MessageLoop* message_loop)
    : message_loop_(message_loop),
      message_loop_task_runner_(message_loop->task_runner()) {}

SchedulerTaskRunnerDelegateImpl::~SchedulerTaskRunnerDelegateImpl() {
  RestoreDefaultTaskRunner();
}

void SchedulerTaskRunnerDelegateImpl::SetDefaultTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  message_loop_->SetTaskRunner(task_runner);
}

void SchedulerTaskRunnerDelegateImpl::RestoreDefaultTaskRunner() {
  if (base::MessageLoop::current() == message_loop_)
    message_loop_->SetTaskRunner(message_loop_task_runner_);
}

bool SchedulerTaskRunnerDelegateImpl::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return message_loop_task_runner_->PostDelayedTask(from_here, task, delay);
}

bool SchedulerTaskRunnerDelegateImpl::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return message_loop_task_runner_->PostNonNestableDelayedTask(from_here, task,
                                                               delay);
}

bool SchedulerTaskRunnerDelegateImpl::RunsTasksOnCurrentThread() const {
  return message_loop_task_runner_->RunsTasksOnCurrentThread();
}

bool SchedulerTaskRunnerDelegateImpl::IsNested() const {
  return message_loop_->IsNested();
}

}  // namespace scheduler
