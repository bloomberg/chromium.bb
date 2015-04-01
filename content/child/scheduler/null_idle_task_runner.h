// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SCHEDULER_NULL_IDLE_TASK_RUNNER_H_
#define CONTENT_CHILD_SCHEDULER_NULL_IDLE_TASK_RUNNER_H_

#include "content/child/scheduler/single_thread_idle_task_runner.h"

namespace content {

class NullIdleTaskRunner : public SingleThreadIdleTaskRunner {
 public:
  NullIdleTaskRunner();
  void PostIdleTask(const tracked_objects::Location& from_here,
                    const IdleTask& idle_task) override;

  void PostNonNestableIdleTask(
      const tracked_objects::Location& from_here,
      const IdleTask& idle_task) override;

  void PostIdleTaskAfterWakeup(
      const tracked_objects::Location& from_here,
      const IdleTask& idle_task) override;

 protected:
  ~NullIdleTaskRunner() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NullIdleTaskRunner);
};

}  // namespace content

#endif  // CONTENT_CHILD_SCHEDULER_NULL_IDLE_TASK_RUNNER_H_
