// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_NULL_WORKER_SCHEDULER_H_
#define COMPONENTS_SCHEDULER_CHILD_NULL_WORKER_SCHEDULER_H_

#include "components/scheduler/child/worker_scheduler.h"

namespace scheduler {

class NullWorkerScheduler : public WorkerScheduler {
 public:
  NullWorkerScheduler();
  ~NullWorkerScheduler() override;

  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;

  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  bool ShouldYieldForHighPriorityWork() override;
  void Init() override;
  void Shutdown() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(NullWorkerScheduler);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_NULL_WORKER_SCHEDULER_H_
