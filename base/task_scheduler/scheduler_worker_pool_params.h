// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_SCHEDULER_WORKER_POOL_PARAMS_H_
#define BASE_TASK_SCHEDULER_SCHEDULER_WORKER_POOL_PARAMS_H_

#include <string>

#include "base/macros.h"
#include "base/threading/platform_thread.h"

namespace base {

class TimeDelta;

class BASE_EXPORT SchedulerWorkerPoolParams final {
 public:
  enum class IORestriction {
    ALLOWED,
    DISALLOWED,
  };

  // Construct a scheduler worker pool parameter object that instructs a
  // scheduler worker pool to use the label |name| and create up to
  // |max_threads| threads. |priority_hint| is the preferred thread priority;
  // the actual thread priority depends on shutdown state and platform
  // capabilities. |io_restriction| indicates whether Tasks on the scheduler
  // worker pool are allowed to make I/O calls. |suggested_reclaim_time| sets a
  // suggestion on when to reclaim idle threads. The worker pool is free to
  // ignore this value for performance or correctness reasons.
  SchedulerWorkerPoolParams(const std::string& name,
                            ThreadPriority priority_hint,
                            IORestriction io_restriction,
                            int max_threads,
                            const TimeDelta& suggested_reclaim_time);
  SchedulerWorkerPoolParams(SchedulerWorkerPoolParams&& other);
  SchedulerWorkerPoolParams& operator=(SchedulerWorkerPoolParams&& other);

  // Name of the pool. Used to label the pool's threads.
  const std::string& name() const { return name_; }

  // Preferred priority for the pool's threads.
  ThreadPriority priority_hint() const { return priority_hint_; }

  // Whether I/O is allowed in the pool.
  IORestriction io_restriction() const { return io_restriction_; }

  // Maximum number of threads in the pool.
  size_t max_threads() const { return max_threads_; }

  // Suggested reclaim time for threads in the worker pool.
  const TimeDelta& suggested_reclaim_time() const {
    return suggested_reclaim_time_;
  }

 private:
  std::string name_;
  ThreadPriority priority_hint_;
  IORestriction io_restriction_;
  size_t max_threads_;
  TimeDelta suggested_reclaim_time_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerPoolParams);
};

}  // namespace base

#endif  // BASE_TASK_SCHEDULER_SCHEDULER_WORKER_POOL_PARAMS_H_
