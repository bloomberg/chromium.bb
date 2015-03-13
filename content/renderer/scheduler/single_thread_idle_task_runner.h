// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_SINGLE_THREAD_IDLE_TASK_RUNNER_H_
#define CONTENT_RENDERER_SCHEDULER_SINGLE_THREAD_IDLE_TASK_RUNNER_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"

namespace content {

// A SingleThreadIdleTaskRunner is a task runner for running idle tasks. Idle
// tasks have an unbound argument which is bound to a deadline
// (in base::TimeTicks) when they are run. The idle task is expected to
// complete by this deadline.
class SingleThreadIdleTaskRunner
    : public base::RefCountedThreadSafe<SingleThreadIdleTaskRunner> {
 public:
  typedef base::Callback<void(base::TimeTicks)> IdleTask;

  SingleThreadIdleTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> idle_priority_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> after_wakeup_task_runner,
      base::Callback<void(base::TimeTicks*)> deadline_supplier);

  virtual void PostIdleTask(const tracked_objects::Location& from_here,
                            const IdleTask& idle_task);

  virtual void PostNonNestableIdleTask(
      const tracked_objects::Location& from_here,
      const IdleTask& idle_task);

  virtual void PostIdleTaskAfterWakeup(
      const tracked_objects::Location& from_here,
      const IdleTask& idle_task);

  bool RunsTasksOnCurrentThread() const;

 protected:
  virtual ~SingleThreadIdleTaskRunner();

 private:
  friend class base::RefCountedThreadSafe<SingleThreadIdleTaskRunner>;

  void RunTask(IdleTask idle_task);

  scoped_refptr<base::SingleThreadTaskRunner> idle_priority_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> after_wakeup_task_runner_;
  base::Callback<void(base::TimeTicks*)> deadline_supplier_;
  DISALLOW_COPY_AND_ASSIGN(SingleThreadIdleTaskRunner);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCHEDULER_SINGLE_THREAD_IDLE_TASK_RUNNER_H_
