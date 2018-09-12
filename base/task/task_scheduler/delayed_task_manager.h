// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_DELAYED_TASK_MANAGER_H_
#define BASE_TASK_TASK_SCHEDULER_DELAYED_TASK_MANAGER_H_

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/task_scheduler/scheduler_lock.h"
#include "base/task/task_scheduler/task.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"

namespace base {

class TaskRunner;

namespace internal {

struct Task;

// The DelayedTaskManager forwards tasks to post task callbacks when they become
// ripe for execution. Tasks are not forwarded before Start() is called. This
// class is thread-safe.
class BASE_EXPORT DelayedTaskManager {
 public:
  // Posts |task| for execution immediately.
  using PostTaskNowCallback = OnceCallback<void(Task task)>;

  // |tick_clock| can be specified for testing.
  DelayedTaskManager(std::unique_ptr<const TickClock> tick_clock =
                         std::make_unique<DefaultTickClock>());
  ~DelayedTaskManager();

  // Starts the delayed task manager, allowing past and future tasks to be
  // forwarded to their callbacks as they become ripe for execution.
  // |service_thread_task_runner| posts tasks to the TaskScheduler service
  // thread.
  void Start(scoped_refptr<TaskRunner> service_thread_task_runner);

  // Schedules a call to |post_task_now_callback| with |task| as argument when
  // |task| is ripe for execution.
  void AddDelayedTask(Task task, PostTaskNowCallback post_task_now_callback);

 private:
  using DelayedTask = std::pair<Task, PostTaskNowCallback>;

  // Pop and post all the ripe tasks in the delayed task queue.
  void ProcessRipeTasks();

  // Return the run time of the delayed task that needs to be processed the
  // soonest.
  const TimeTicks GetNextDelayedTaskRunTimeLockRequired();

  // Return the run time of the soonest scheduled `ProcessRipeTasks` call.
  const TimeTicks GetNextProcessRipeTaskTimeLockRequired();

  // Schedule the ProcessRipeTasks method on the service thread to be executed
  // in the given |next_delayed_task_run_time|.
  void ScheduleProcessRipeTasksOnServiceThread(
      TimeTicks next_delayed_task_run_time);

  const RepeatingClosure process_ripe_tasks_closure_;

  const std::unique_ptr<const TickClock> tick_clock_;

  scoped_refptr<TaskRunner> service_thread_task_runner_;

  struct TaskDelayedRuntimeComparator {
    inline bool operator()(const DelayedTask& lhs,
                           const DelayedTask& rhs) const {
      return lhs.first.delayed_run_time > rhs.first.delayed_run_time;
    }
  };

  std::priority_queue<DelayedTask,
                      std::vector<DelayedTask>,
                      TaskDelayedRuntimeComparator>
      delayed_task_queue_;

  std::
      priority_queue<TimeTicks, std::vector<TimeTicks>, std::greater<TimeTicks>>
          process_ripe_tasks_time_queue_;

  // Synchronizes access to |delayed_task_queue_|,
  // |process_ripe_task_time_queue_| and the setting of
  // |service_thread_task_runner|. Once |service_thread_task_runner_| is set,
  // it is never modified. It is therefore safe to access
  // |service_thread_task_runner_| without synchronization once it is observed
  // that it is non-null.
  SchedulerLock queue_lock_;

  DISALLOW_COPY_AND_ASSIGN(DelayedTaskManager);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_TASK_SCHEDULER_DELAYED_TASK_MANAGER_H_
