// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/delayed_task_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task.h"
#include "base/task_runner.h"

namespace base {
namespace internal {

DelayedTaskManager::DelayedTaskManager(
    std::unique_ptr<const TickClock> tick_clock)
    : process_ripe_tasks_closure_(
          BindRepeating(&DelayedTaskManager::ProcessRipeTasks,
                        Unretained(this))),
      tick_clock_(std::move(tick_clock)) {
  DCHECK(tick_clock_);
}

DelayedTaskManager::~DelayedTaskManager() = default;

void DelayedTaskManager::Start(
    scoped_refptr<TaskRunner> service_thread_task_runner) {
  DCHECK(service_thread_task_runner);

  TimeTicks next_delayed_task_run_time;
  {
    AutoSchedulerLock auto_lock(queue_lock_);
    DCHECK(!service_thread_task_runner_);
    service_thread_task_runner_ = std::move(service_thread_task_runner);
    if (delayed_task_queue_.empty()) {
      return;
    }
    next_delayed_task_run_time = GetNextDelayedTaskRunTimeLockRequired();
    process_ripe_tasks_time_queue_.push(next_delayed_task_run_time);
  }
  ScheduleProcessRipeTasksOnServiceThread(next_delayed_task_run_time);
}

void DelayedTaskManager::AddDelayedTask(
    Task task,
    PostTaskNowCallback post_task_now_callback) {
  DCHECK(task.task);

  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.task);

  TimeTicks next_delayed_task_run_time;
  {
    AutoSchedulerLock auto_lock(queue_lock_);
    TimeTicks previous_next_delayed_task_run_time =
        GetNextDelayedTaskRunTimeLockRequired();
    delayed_task_queue_.push(
        {std::move(task), std::move(post_task_now_callback)});
    next_delayed_task_run_time = GetNextDelayedTaskRunTimeLockRequired();
    // Not started yet.
    if (service_thread_task_runner_ == nullptr) {
      return;
    }
    // ProcessRipeTasks() callback already scheduled before or at |task|'s
    // scheduled run time.
    if (GetNextProcessRipeTaskTimeLockRequired() <=
        next_delayed_task_run_time) {
      return;
    }
    DCHECK_LT(next_delayed_task_run_time, previous_next_delayed_task_run_time);
    process_ripe_tasks_time_queue_.push(next_delayed_task_run_time);
  }
  ScheduleProcessRipeTasksOnServiceThread(next_delayed_task_run_time);
}

void DelayedTaskManager::ProcessRipeTasks() {
  std::vector<DelayedTask> ripe_delayed_tasks;
  TimeTicks next_delayed_task_run_time;
  bool schedule_process_ripe_tasks = false;

  {
    AutoSchedulerLock auto_lock(queue_lock_);
    const TimeTicks now = tick_clock_->NowTicks();
    while (GetNextDelayedTaskRunTimeLockRequired() <= now) {
      // The const_cast on top is okay since the DelayedTask is
      // transactionnaly being popped from |delayed_task_queue_| right after
      // and the move doesn't alter the sort order (a requirement for the
      // Windows STL's consistency debug-checks for
      // std::priority_queue::top()).
      ripe_delayed_tasks.push_back(
          std::move(const_cast<DelayedTask&>(delayed_task_queue_.top())));
      delayed_task_queue_.pop();
    }
    while (GetNextProcessRipeTaskTimeLockRequired() <= now) {
      process_ripe_tasks_time_queue_.pop();
    }
    if (!delayed_task_queue_.empty()) {
      next_delayed_task_run_time = GetNextDelayedTaskRunTimeLockRequired();
      // ProcessRipeTasks() callback already scheduled before or at the next
      // task's scheduled run time.
      if (next_delayed_task_run_time <
          GetNextProcessRipeTaskTimeLockRequired()) {
        schedule_process_ripe_tasks = true;
        process_ripe_tasks_time_queue_.push(next_delayed_task_run_time);
      }
    }
  }

  if (schedule_process_ripe_tasks) {
    ScheduleProcessRipeTasksOnServiceThread(next_delayed_task_run_time);
  }

  for (auto& delayed_task : ripe_delayed_tasks) {
    std::move(delayed_task.second).Run(std::move(delayed_task.first));
  }
}

const TimeTicks DelayedTaskManager::GetNextDelayedTaskRunTimeLockRequired() {
  queue_lock_.AssertAcquired();
  return delayed_task_queue_.empty()
             ? TimeTicks::Max()
             : delayed_task_queue_.top().first.delayed_run_time;
}

const TimeTicks DelayedTaskManager::GetNextProcessRipeTaskTimeLockRequired() {
  queue_lock_.AssertAcquired();
  return process_ripe_tasks_time_queue_.empty()
             ? TimeTicks::Max()
             : process_ripe_tasks_time_queue_.top();
}

void DelayedTaskManager::ScheduleProcessRipeTasksOnServiceThread(
    TimeTicks next_delayed_task_run_time) {
  DCHECK(!next_delayed_task_run_time.is_null());
  const TimeTicks now = tick_clock_->NowTicks();
  TimeDelta delay = std::max(TimeDelta(), next_delayed_task_run_time - now);
  service_thread_task_runner_->PostDelayedTask(
      FROM_HERE, process_ripe_tasks_closure_, delay);
}

}  // namespace internal
}  // namespace base
