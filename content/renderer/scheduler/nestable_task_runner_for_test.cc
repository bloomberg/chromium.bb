// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/nestable_task_runner_for_test.h"

namespace content {

// static
scoped_refptr<NestableTaskRunnerForTest> NestableTaskRunnerForTest::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return make_scoped_refptr(new NestableTaskRunnerForTest(task_runner));
}

NestableTaskRunnerForTest::NestableTaskRunnerForTest(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner), is_nested_(false) {
}

NestableTaskRunnerForTest::~NestableTaskRunnerForTest() {
}

bool NestableTaskRunnerForTest::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return task_runner_->PostDelayedTask(from_here, task, delay);
}

bool NestableTaskRunnerForTest::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return task_runner_->PostNonNestableDelayedTask(from_here, task, delay);
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

}  // namespace content
