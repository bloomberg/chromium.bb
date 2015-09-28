// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/scheduler_task_runner_delegate_for_test.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/scheduler/base/nestable_task_runner_for_test.h"

namespace scheduler {

// static
scoped_refptr<SchedulerTaskRunnerDelegateForTest>
SchedulerTaskRunnerDelegateForTest::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return make_scoped_refptr(
      new SchedulerTaskRunnerDelegateForTest(task_runner));
}

SchedulerTaskRunnerDelegateForTest::SchedulerTaskRunnerDelegateForTest(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(NestableTaskRunnerForTest::Create(task_runner)) {}

SchedulerTaskRunnerDelegateForTest::~SchedulerTaskRunnerDelegateForTest() {}

void SchedulerTaskRunnerDelegateForTest::SetDefaultTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  default_task_runner_ = task_runner.Pass();
}

void SchedulerTaskRunnerDelegateForTest::RestoreDefaultTaskRunner() {
  default_task_runner_ = nullptr;
}

bool SchedulerTaskRunnerDelegateForTest::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return task_runner_->PostDelayedTask(from_here, task, delay);
}

bool SchedulerTaskRunnerDelegateForTest::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return task_runner_->PostNonNestableDelayedTask(from_here, task, delay);
}

bool SchedulerTaskRunnerDelegateForTest::RunsTasksOnCurrentThread() const {
  return task_runner_->RunsTasksOnCurrentThread();
}

bool SchedulerTaskRunnerDelegateForTest::IsNested() const {
  return task_runner_->IsNested();
}

}  // namespace scheduler
