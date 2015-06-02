// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/nestable_task_runner_for_test.h"

#include "base/bind.h"
#include "base/bind_helpers.h"

namespace scheduler {

// static
scoped_refptr<NestableTaskRunnerForTest> NestableTaskRunnerForTest::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return make_scoped_refptr(new NestableTaskRunnerForTest(task_runner));
}

NestableTaskRunnerForTest::NestableTaskRunnerForTest(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner), is_nested_(false), weak_factory_(this) {
  weak_nestable_task_runner_ptr_ = weak_factory_.GetWeakPtr();
}

NestableTaskRunnerForTest::~NestableTaskRunnerForTest() {
}

void NestableTaskRunnerForTest::WrapTask(
    const base::PendingTask* wrapped_task) {
  scoped_refptr<NestableTaskRunnerForTest> protect(this);
  FOR_EACH_OBSERVER(base::MessageLoop::TaskObserver, task_observers_,
                    DidProcessTask(*wrapped_task));
  wrapped_task->task.Run();
  FOR_EACH_OBSERVER(base::MessageLoop::TaskObserver, task_observers_,
                    WillProcessTask(*wrapped_task));
}

bool NestableTaskRunnerForTest::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  base::PendingTask* wrapped_task = new base::PendingTask(from_here, task);
  return task_runner_->PostDelayedTask(
      from_here,
      base::Bind(&NestableTaskRunnerForTest::WrapTask,
                 weak_nestable_task_runner_ptr_, base::Owned(wrapped_task)),
      delay);
}

bool NestableTaskRunnerForTest::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  base::PendingTask* wrapped_task = new base::PendingTask(from_here, task);
  return task_runner_->PostNonNestableDelayedTask(
      from_here,
      base::Bind(&NestableTaskRunnerForTest::WrapTask,
                 weak_nestable_task_runner_ptr_, base::Owned(wrapped_task)),
      delay);
}

bool NestableTaskRunnerForTest::RunsTasksOnCurrentThread() const {
  return task_runner_->RunsTasksOnCurrentThread();
}

bool NestableTaskRunnerForTest::IsNested() const {
  return is_nested_;
}

void NestableTaskRunnerForTest::SetNested(bool is_nested) {
  is_nested_ = is_nested;
}

void NestableTaskRunnerForTest::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  task_observers_.AddObserver(task_observer);
}

void NestableTaskRunnerForTest::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  task_observers_.RemoveObserver(task_observer);
}

}  // namespace scheduler
