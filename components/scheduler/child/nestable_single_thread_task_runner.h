// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_NESTABLE_SINGLE_THREAD_TASK_RUNNER_H_
#define COMPONENTS_SCHEDULER_CHILD_NESTABLE_SINGLE_THREAD_TASK_RUNNER_H_

#include "base/single_thread_task_runner.h"
#include "base/message_loop/message_loop.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

// A single thread task runner which exposes whether it is running nested.
class SCHEDULER_EXPORT NestableSingleThreadTaskRunner
    : public base::SingleThreadTaskRunner {
 public:
  NestableSingleThreadTaskRunner() {}

  // Returns true if the task runner is nested (i.e., running a run loop within
  // a nested task).
  virtual bool IsNested() const = 0;

 protected:
  ~NestableSingleThreadTaskRunner() override {}

  DISALLOW_COPY_AND_ASSIGN(NestableSingleThreadTaskRunner);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_NESTABLE_SINGLE_THREAD_TASK_RUNNER_H_
