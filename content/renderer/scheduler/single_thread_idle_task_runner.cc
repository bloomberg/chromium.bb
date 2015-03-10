// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/single_thread_idle_task_runner.h"

#include "base/location.h"
#include "base/trace_event/trace_event.h"

namespace content {

SingleThreadIdleTaskRunner::SingleThreadIdleTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> idle_priority_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> after_wakeup_task_runner,
    base::Callback<void(base::TimeTicks*)> deadline_supplier)
    : idle_priority_task_runner_(idle_priority_task_runner),
      after_wakeup_task_runner_(after_wakeup_task_runner),
      deadline_supplier_(deadline_supplier) {
  DCHECK(!idle_priority_task_runner_ ||
         idle_priority_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!after_wakeup_task_runner_ ||
         after_wakeup_task_runner_->RunsTasksOnCurrentThread());
}

SingleThreadIdleTaskRunner::~SingleThreadIdleTaskRunner() {
}

bool SingleThreadIdleTaskRunner::RunsTasksOnCurrentThread() const {
  return idle_priority_task_runner_->RunsTasksOnCurrentThread();
}

void SingleThreadIdleTaskRunner::PostIdleTask(
    const tracked_objects::Location& from_here,
    const IdleTask& idle_task) {
  idle_priority_task_runner_->PostTask(
      from_here,
      base::Bind(&SingleThreadIdleTaskRunner::RunTask, this, idle_task));
}

void SingleThreadIdleTaskRunner::PostIdleTaskAfterWakeup(
    const tracked_objects::Location& from_here,
    const IdleTask& idle_task) {
  after_wakeup_task_runner_->PostTask(
      FROM_HERE, base::Bind(&SingleThreadIdleTaskRunner::PostIdleTask, this,
                            from_here, idle_task));
}

void SingleThreadIdleTaskRunner::RunTask(IdleTask idle_task) {
  base::TimeTicks deadline;
  deadline_supplier_.Run(&deadline);
  TRACE_EVENT1("renderer.scheduler",
               "SingleThreadIdleTaskRunner::RunTask", "allotted_time_ms",
               (deadline - base::TimeTicks::Now()).InMillisecondsF());
  idle_task.Run(deadline);
  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("renderer.scheduler", &is_tracing);
  if (is_tracing && base::TimeTicks::Now() > deadline) {
    TRACE_EVENT_INSTANT0("renderer.scheduler",
                         "SingleThreadIdleTaskRunner::DidOverrunDeadline",
                         TRACE_EVENT_SCOPE_THREAD);
  }
}

}  // namespace content
