// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/delayed_task_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task.h"
#include "base/task_runner.h"

namespace base {
namespace internal {

// The UMA histogram that logs a lower-bound number of cancelled tasks in the
// manager.
const char kNumCancelledDelayedTasksHistogram[] =
    "TaskScheduler.NumCancelledDelayedTasks";

// The UMA histogram that logs a lower-bound percentage of cancelled tasks in
// the manager.
const char kNumPercentCancelledDelayedTasksHistogram[] =
    "TaskScheduler.PercentCancelledDelayedTasks";

DelayedTaskManager::DelayedTask::DelayedTask() = default;

DelayedTaskManager::DelayedTask::DelayedTask(Task task,
                                             PostTaskNowCallback callback)
    : task(std::move(task)), callback(std::move(callback)) {}

DelayedTaskManager::DelayedTask::DelayedTask(
    DelayedTaskManager::DelayedTask&& other) = default;

DelayedTaskManager::DelayedTask::~DelayedTask() = default;

DelayedTaskManager::DelayedTask& DelayedTaskManager::DelayedTask::operator=(
    DelayedTaskManager::DelayedTask&& other) = default;

bool DelayedTaskManager::DelayedTask::operator<=(
    const DelayedTask& other) const {
  return task.delayed_run_time <= other.task.delayed_run_time;
}

bool DelayedTaskManager::DelayedTask::IsScheduled() const {
  return scheduled_;
}
void DelayedTaskManager::DelayedTask::SetScheduled() {
  DCHECK(!scheduled_);
  scheduled_ = true;
}

bool DelayedTaskManager::DelayedTask::IsStale() const {
  return !task.task.MaybeValid();
}

DelayedTaskManager::DelayedTaskManager(
    StringPiece histogram_label,
    std::unique_ptr<const TickClock> tick_clock)
    : process_ripe_tasks_closure_(
          BindRepeating(&DelayedTaskManager::ProcessRipeTasks,
                        Unretained(this))),
      histogram_label_(histogram_label),
      tick_clock_(std::move(tick_clock)) {
  DCHECK(tick_clock_);
}

DelayedTaskManager::~DelayedTaskManager() = default;

void DelayedTaskManager::Start(
    scoped_refptr<TaskRunner> service_thread_task_runner) {
  DCHECK(service_thread_task_runner);

  TimeTicks process_ripe_tasks_time;
  {
    AutoSchedulerLock auto_lock(queue_lock_);
    DCHECK(!service_thread_task_runner_);
    service_thread_task_runner_ = std::move(service_thread_task_runner);
    process_ripe_tasks_time = GetTimeToScheduleProcessRipeTasksLockRequired();
  }
  ScheduleProcessRipeTasksOnServiceThread(process_ripe_tasks_time);
}

void DelayedTaskManager::AddDelayedTask(
    Task task,
    PostTaskNowCallback post_task_now_callback) {
  DCHECK(task.task);
  DCHECK(!task.delayed_run_time.is_null());

  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.task);
  TimeTicks process_ripe_tasks_time;
  {
    AutoSchedulerLock auto_lock(queue_lock_);
    delayed_task_queue_.insert(
        DelayedTask(std::move(task), std::move(post_task_now_callback)));
    // Not started yet.
    if (service_thread_task_runner_ == nullptr)
      return;
    process_ripe_tasks_time = GetTimeToScheduleProcessRipeTasksLockRequired();
  }
  ScheduleProcessRipeTasksOnServiceThread(process_ripe_tasks_time);
}

void DelayedTaskManager::ReportHeartbeatMetrics() const {
  size_t num_cancelled_tasks = delayed_task_queue_.NumKnownStaleNodes();
  size_t num_tasks = delayed_task_queue_.size();
  int percent_cancelled_tasks =
      (num_tasks == 0)
          ? 0
          : static_cast<int>((100 * num_cancelled_tasks) / num_tasks);

  UmaHistogramCounts100(
      JoinString({kNumCancelledDelayedTasksHistogram, histogram_label_}, "."),
      static_cast<int>(num_cancelled_tasks));
  UmaHistogramPercentage(
      JoinString({kNumPercentCancelledDelayedTasksHistogram, histogram_label_},
                 "."),
      percent_cancelled_tasks);
}

void DelayedTaskManager::ProcessRipeTasks() {
  std::vector<DelayedTask> ripe_delayed_tasks;
  TimeTicks process_ripe_tasks_time;

  {
    AutoSchedulerLock auto_lock(queue_lock_);
    const TimeTicks now = tick_clock_->NowTicks();
    while (!delayed_task_queue_.empty() &&
           delayed_task_queue_.Min().task.delayed_run_time <= now) {
      // The const_cast on top is okay since the DelayedTask is
      // transactionally being popped from |delayed_task_queue_| right after
      // and the move doesn't alter the sort order.
      ripe_delayed_tasks.push_back(
          std::move(const_cast<DelayedTask&>(delayed_task_queue_.Min())));
      delayed_task_queue_.Pop();
    }
    process_ripe_tasks_time = GetTimeToScheduleProcessRipeTasksLockRequired();
  }
  ScheduleProcessRipeTasksOnServiceThread(process_ripe_tasks_time);

  for (auto& delayed_task : ripe_delayed_tasks) {
    std::move(delayed_task.callback).Run(std::move(delayed_task.task));
  }
}

TimeTicks DelayedTaskManager::GetTimeToScheduleProcessRipeTasksLockRequired() {
  queue_lock_.AssertAcquired();
  if (delayed_task_queue_.empty())
    return TimeTicks::Max();
  // The const_cast on top is okay since |IsScheduled()| and |SetScheduled()|
  // don't alter the sort order.
  DelayedTask& ripest_delayed_task =
      const_cast<DelayedTask&>(delayed_task_queue_.Min());
  if (ripest_delayed_task.IsScheduled())
    return TimeTicks::Max();
  ripest_delayed_task.SetScheduled();
  return ripest_delayed_task.task.delayed_run_time;
}

void DelayedTaskManager::ScheduleProcessRipeTasksOnServiceThread(
    TimeTicks next_delayed_task_run_time) {
  DCHECK(!next_delayed_task_run_time.is_null());
  if (next_delayed_task_run_time.is_max())
    return;
  const TimeTicks now = tick_clock_->NowTicks();
  TimeDelta delay = std::max(TimeDelta(), next_delayed_task_run_time - now);
  service_thread_task_runner_->PostDelayedTask(
      FROM_HERE, process_ripe_tasks_closure_, delay);
}

}  // namespace internal
}  // namespace base
