// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/scheduler_message_loop_delegate.h"

namespace scheduler {

// static
scoped_refptr<SchedulerMessageLoopDelegate>
SchedulerMessageLoopDelegate::Create(base::MessageLoop* message_loop) {
  return make_scoped_refptr(new SchedulerMessageLoopDelegate(message_loop));
}

SchedulerMessageLoopDelegate::SchedulerMessageLoopDelegate(
    base::MessageLoop* message_loop)
    : message_loop_(message_loop) {
}

SchedulerMessageLoopDelegate::~SchedulerMessageLoopDelegate() {
}

bool SchedulerMessageLoopDelegate::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return message_loop_->task_runner()->PostDelayedTask(from_here, task, delay);
}

bool SchedulerMessageLoopDelegate::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return message_loop_->task_runner()->PostNonNestableDelayedTask(from_here,
                                                                  task, delay);
}

bool SchedulerMessageLoopDelegate::RunsTasksOnCurrentThread() const {
  return message_loop_->task_runner()->RunsTasksOnCurrentThread();
}

bool SchedulerMessageLoopDelegate::IsNested() const {
  return message_loop_->IsNested();
}

void SchedulerMessageLoopDelegate::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  return message_loop_->AddTaskObserver(task_observer);
}

void SchedulerMessageLoopDelegate::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  return message_loop_->RemoveTaskObserver(task_observer);
}

}  // namespace scheduler
