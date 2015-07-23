// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_NULL_CHILD_TASK_QUEUE_H_
#define COMPONENTS_SCHEDULER_NULL_CHILD_TASK_QUEUE_H_

#include "components/scheduler/child/task_queue.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class SCHEDULER_EXPORT NullTaskQueue : public TaskQueue {
 public:
  NullTaskQueue(scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // TaskQueue implementation
  bool RunsTasksOnCurrentThread() const override;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;
  bool PostDelayedTaskAt(const tracked_objects::Location& from_here,
                         const base::Closure& task,
                         base::TimeTicks desired_run_time) override;

  bool IsQueueEnabled() const override;
  QueueState GetQueueState() const override;
  const char* GetName() const override;
  void SetQueuePriority(QueuePriority priority) override;
  void PumpQueue() override;
  void SetPumpPolicy(PumpPolicy pump_policy) override;

 protected:
  ~NullTaskQueue() override;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(NullTaskQueue);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_NULL_TASK_QUEUE_H_
