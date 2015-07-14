// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_NESTABLE_TASK_RUNNER_FOR_TEST_H_
#define CONTENT_RENDERER_SCHEDULER_NESTABLE_TASK_RUNNER_FOR_TEST_H_

#include "components/scheduler/child/nestable_single_thread_task_runner.h"

namespace scheduler {

class NestableTaskRunnerForTest : public NestableSingleThreadTaskRunner {
 public:
  static scoped_refptr<NestableTaskRunnerForTest> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void SetNested(bool is_nested);

  // NestableSingleThreadTaskRunner implementation
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;
  bool RunsTasksOnCurrentThread() const override;
  bool IsNested() const override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;

 protected:
  ~NestableTaskRunnerForTest() override;

 private:
  NestableTaskRunnerForTest(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void WrapTask(const base::PendingTask* wrapped_task);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool is_nested_;

  base::ObserverList<base::MessageLoop::TaskObserver> task_observers_;

  base::WeakPtr<NestableTaskRunnerForTest> weak_nestable_task_runner_ptr_;
  base::WeakPtrFactory<NestableTaskRunnerForTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NestableTaskRunnerForTest);
};

}  // namespace scheduler

#endif  // CONTENT_RENDERER_SCHEDULER_NESTABLE_TASK_RUNNER_FOR_TEST_H_
