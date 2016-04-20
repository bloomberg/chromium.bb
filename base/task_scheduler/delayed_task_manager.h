// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_DELAYED_TASK_MANAGER_H_
#define BASE_TASK_SCHEDULER_DELAYED_TASK_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <vector>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_scheduler/scheduler_lock.h"
#include "base/task_scheduler/sequence.h"
#include "base/task_scheduler/task.h"
#include "base/time/time.h"

namespace base {
namespace internal {

class SchedulerTaskExecutor;

// A DelayedTaskManager holds delayed Tasks until they become ripe for
// execution. When they become ripe for execution, it posts them to their
// associated Sequence and SchedulerTaskExecutor. This class is thread-safe.
class BASE_EXPORT DelayedTaskManager {
 public:
  // |on_delayed_run_time_updated| is invoked when the delayed run time is
  // updated as a result of adding a delayed task to the manager.
  explicit DelayedTaskManager(const Closure& on_delayed_run_time_updated);
  ~DelayedTaskManager();

  // Adds |task| to a queue of delayed tasks. The task will be posted to
  // |executor| as part of |sequence| the first time that PostReadyTasks() is
  // called while Now() is passed |task->delayed_run_time|.
  //
  // TODO(robliao): Find a concrete way to manage |executor|'s memory. It is
  // never deleted in production, but it is better not to spread this assumption
  // throughout the scheduler.
  void AddDelayedTask(std::unique_ptr<Task> task,
                      scoped_refptr<Sequence> sequence,
                      SchedulerTaskExecutor* executor);

  // Posts delayed tasks that are ripe for execution.
  // TODO(robliao): Call this from a service thread.
  void PostReadyTasks();

  // Returns the next time at which a delayed task will become ripe for
  // execution, or a null TimeTicks if there are no pending delayed tasks.
  TimeTicks GetDelayedRunTime() const;

 private:
  struct DelayedTask;
  struct DelayedTaskComparator {
    bool operator()(const DelayedTask& left, const DelayedTask& right) const;
  };

  // Returns the current time. Can be overridden for tests.
  virtual TimeTicks Now() const;

  const Closure on_delayed_run_time_updated_;

  // Synchronizes access to all members below.
  mutable SchedulerLock lock_;

  // Priority queue of delayed tasks. The delayed task with the smallest
  // |task->delayed_run_time| is in front of the priority queue.
  using DelayedTaskQueue = std::priority_queue<DelayedTask,
                                               std::vector<DelayedTask>,
                                               DelayedTaskComparator>;
  DelayedTaskQueue delayed_tasks_;

  // The index to assign to the next delayed task added to the manager.
  uint64_t delayed_task_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DelayedTaskManager);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_DELAYED_TASK_MANAGER_H_
