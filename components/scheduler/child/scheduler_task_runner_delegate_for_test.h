// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_TASK_RUNNER_DELEGATE_FOR_TEST_H_
#define CONTENT_RENDERER_SCHEDULER_TASK_RUNNER_DELEGATE_FOR_TEST_H_

#include "components/scheduler/child/scheduler_task_runner_delegate.h"

namespace scheduler {

class NestableTaskRunnerForTest;

class SchedulerTaskRunnerDelegateForTest : public SchedulerTaskRunnerDelegate {
 public:
  static scoped_refptr<SchedulerTaskRunnerDelegateForTest> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // SchedulerTaskRunnerDelegate implementation
  void SetDefaultTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void RestoreDefaultTaskRunner() override;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;
  bool RunsTasksOnCurrentThread() const override;
  bool IsNested() const override;

  base::SingleThreadTaskRunner* default_task_runner() const {
    return default_task_runner_.get();
  }

 protected:
  ~SchedulerTaskRunnerDelegateForTest() override;

 private:
  explicit SchedulerTaskRunnerDelegateForTest(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;

  scoped_refptr<NestableTaskRunnerForTest> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerTaskRunnerDelegateForTest);
};

}  // namespace scheduler

#endif  // CONTENT_RENDERER_SCHEDULER_SCHEDULER_TASK_RUNNER_DELEGATE_FOR_TEST_H_
