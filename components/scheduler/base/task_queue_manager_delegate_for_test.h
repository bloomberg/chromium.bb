// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_FOR_TEST_H_
#define COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_FOR_TEST_H_

#include "base/macros.h"
#include "base/time/tick_clock.h"
#include "components/scheduler/base/task_queue_manager_delegate.h"

namespace scheduler {

class TaskQueueManagerDelegateForTest : public TaskQueueManagerDelegate {
 public:
  static scoped_refptr<TaskQueueManagerDelegateForTest> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_ptr<base::TickClock> time_source);

  // NestableSingleThreadTaskRunner implementation
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;
  bool RunsTasksOnCurrentThread() const override;
  bool IsNested() const override;
  base::TimeTicks NowTicks() override;
  void OnNoMoreImmediateWork() override;
  double CurrentTimeSeconds() const override;

 protected:
  ~TaskQueueManagerDelegateForTest() override;
  TaskQueueManagerDelegateForTest(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_ptr<base::TickClock> time_source);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_ptr<base::TickClock> time_source_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueManagerDelegateForTest);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_BASE_TASK_QUEUE_MANAGER_DELEGATE_FOR_TEST_H_
