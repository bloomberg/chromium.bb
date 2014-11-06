// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/single_thread_idle_task_runner.h"

#include "base/location.h"

namespace content {

SingleThreadIdleTaskRunner::SingleThreadIdleTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::Callback<void(base::TimeTicks*)> deadline_supplier)
    : task_runner_(task_runner), deadline_supplier_(deadline_supplier) {
}

SingleThreadIdleTaskRunner::~SingleThreadIdleTaskRunner() {
}

bool SingleThreadIdleTaskRunner::RunsTasksOnCurrentThread() const {
  return task_runner_->RunsTasksOnCurrentThread();
}

void SingleThreadIdleTaskRunner::PostIdleTask(
    const tracked_objects::Location& from_here,
    const IdleTask& idle_task) {
  task_runner_->PostTask(
      from_here,
      base::Bind(&SingleThreadIdleTaskRunner::RunTask, this, idle_task));
}

void SingleThreadIdleTaskRunner::RunTask(IdleTask idle_task) {
  base::TimeTicks deadline;
  deadline_supplier_.Run(&deadline);
  idle_task.Run(deadline);
}

}  // namespace content
