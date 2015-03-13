// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_DEADLINE_TASK_RUNNER_H_
#define CONTENT_RENDERER_SCHEDULER_DEADLINE_TASK_RUNNER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/renderer/scheduler/cancelable_closure_holder.h"

namespace content {

// Runs a posted task at latest by a given deadline, but possibly sooner.
class CONTENT_EXPORT DeadlineTaskRunner {
 public:
  DeadlineTaskRunner(const base::Closure& callback,
                     scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~DeadlineTaskRunner();

  // If there is no outstanding task then a task is posted to run after |delay|.
  // If there is an outstanding task which is scheduled to run:
  //   a) sooner - then this is a NOP.
  //   b) later - then the outstanding task is cancelled and a new task is
  //              posted to run after |delay|.
  //
  // Once the deadline task has run, we reset.
  void SetDeadline(const tracked_objects::Location& from_here,
                   base::TimeDelta delay,
                   base::TimeTicks now);

 private:
  void RunInternal();

  CancelableClosureHolder cancelable_run_internal_;
  base::Closure callback_;
  base::TimeTicks deadline_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DeadlineTaskRunner);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCHEDULER_DEADLINE_TASK_RUNNER_H_
