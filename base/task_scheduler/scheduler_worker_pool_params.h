// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_SCHEDULER_WORKER_POOL_PARAMS_H_
#define BASE_TASK_SCHEDULER_SCHEDULER_WORKER_POOL_PARAMS_H_

#include <string>

#include "base/macros.h"
#include "base/threading/platform_thread.h"

namespace base {
namespace internal {

class BASE_EXPORT SchedulerWorkerPoolParams final {
 public:
  enum class IORestriction {
    ALLOWED,
    DISALLOWED,
  };

  // Construct a scheduler worker pool parameter object that instructs a
  // scheduler worker pool to use the label |name| and create up to
  // |max_threads| threads of priority |thread_priority|. |io_restriction|
  // indicates whether Tasks on the scheduler worker pool are allowed to make
  // I/O calls.
  SchedulerWorkerPoolParams(
      const std::string& name,
      ThreadPriority thread_priority,
      IORestriction io_restriction,
      int max_threads);
  SchedulerWorkerPoolParams(SchedulerWorkerPoolParams&& other);
  SchedulerWorkerPoolParams& operator=(SchedulerWorkerPoolParams&& other);

  // Name of the pool. Used to label the pool's threads.
  const std::string& name() const { return name_; }

  // Priority of the pool's threads.
  ThreadPriority thread_priority() const { return thread_priority_; }

  // Whether I/O is allowed in the pool.
  IORestriction io_restriction() const { return io_restriction_; }

  // Maximum number of threads in the pool.
  size_t max_threads() const { return max_threads_; }

 private:
  std::string name_;
  ThreadPriority thread_priority_;
  IORestriction io_restriction_;
  size_t max_threads_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerPoolParams);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_SCHEDULER_WORKER_POOL_PARAMS_H_
