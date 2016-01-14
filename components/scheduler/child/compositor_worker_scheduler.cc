// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/compositor_worker_scheduler.h"

#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"

namespace scheduler {

CompositorWorkerScheduler::CompositorWorkerScheduler(base::Thread* thread)
    : thread_(thread) {}

CompositorWorkerScheduler::~CompositorWorkerScheduler() {}

void CompositorWorkerScheduler::Init() {}

scoped_refptr<base::SingleThreadTaskRunner>
CompositorWorkerScheduler::DefaultTaskRunner() {
  // TODO(sad): Implement a more robust scheduler that can do idle tasks for GC
  // without regressing performance of the rest of the system.
  return thread_->task_runner();
}

scoped_refptr<scheduler::SingleThreadIdleTaskRunner>
CompositorWorkerScheduler::IdleTaskRunner() {
  // TODO(sad): Not having a task-runner for idle tasks means v8 has to fallback
  // to inline GC, which might cause jank.
  return nullptr;
}

bool CompositorWorkerScheduler::CanExceedIdleDeadlineIfRequired() const {
  return false;
}

bool CompositorWorkerScheduler::ShouldYieldForHighPriorityWork() {
  return false;
}

void CompositorWorkerScheduler::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  thread_->message_loop()->AddTaskObserver(task_observer);
}

void CompositorWorkerScheduler::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  thread_->message_loop()->RemoveTaskObserver(task_observer);
}

void CompositorWorkerScheduler::Shutdown() {}

}  // namespace scheduler
