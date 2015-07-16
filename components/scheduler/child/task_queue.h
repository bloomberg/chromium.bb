// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_TASK_QUEUE_H_
#define COMPONENTS_SCHEDULER_CHILD_TASK_QUEUE_H_

#include "base/single_thread_task_runner.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class SCHEDULER_EXPORT TaskQueue : public base::SingleThreadTaskRunner {
 public:
  TaskQueue() {}

  // Post a delayed task at an absolute desired run time instead of a time
  // delta from the current time.
  virtual bool PostDelayedTaskAt(const tracked_objects::Location& from_here,
                                 const base::Closure& task,
                                 base::TimeTicks desired_run_time) = 0;

 protected:
  ~TaskQueue() override {}

  DISALLOW_COPY_AND_ASSIGN(TaskQueue);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_TASK_QUEUE_H_
