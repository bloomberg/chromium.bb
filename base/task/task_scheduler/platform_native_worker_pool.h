// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_PLATFORM_NATIVE_WORKER_POOL_H_
#define BASE_TASK_TASK_SCHEDULER_PLATFORM_NATIVE_WORKER_POOL_H_

#include "base/base_export.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/task_scheduler/scheduler_worker_pool.h"

namespace base {
namespace internal {

class BASE_EXPORT PlatformNativeWorkerPool : public SchedulerWorkerPool {
 public:
  // Destroying a PlatformNativeWorkerPool is not allowed in
  // production; it is always leaked. In tests, it can only be destroyed after
  // JoinForTesting() has returned.
  ~PlatformNativeWorkerPool() override;

  // Starts the worker pool and allows tasks to begin running.
  void Start(WorkerEnvironment worker_environment = WorkerEnvironment::NONE);

  // SchedulerWorkerPool:
  void JoinForTesting() override;
  size_t GetMaxConcurrentNonBlockedTasksDeprecated() const override;
  void ReportHeartbeatMetrics() const override;

 protected:
  PlatformNativeWorkerPool(TrackedRef<TaskTracker> task_tracker,
                           TrackedRef<Delegate> delegate,
                           SchedulerWorkerPool* predecessor_pool);

  // Runs a task off the next sequence on the |priority_queue_|. Called by
  // callbacks posted to platform native thread pools.
  void RunNextSequenceImpl();

  virtual void JoinImpl() = 0;
  virtual void StartImpl() = 0;
  virtual void SubmitWork() = 0;

  // Used to control the worker environment. Supports COM MTA on Windows.
  WorkerEnvironment worker_environment_ = WorkerEnvironment::NONE;

 private:
  class ScopedWorkersExecutor;

  // SchedulerWorkerPool:
  void UpdateSortKey(SequenceAndTransaction sequence_and_transaction) override;
  void PushSequenceAndWakeUpWorkers(
      SequenceAndTransaction sequence_and_transaction) override;
  void EnsureEnoughWorkersLockRequired(BaseScopedWorkersExecutor* executor)
      override EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns the top Sequence off the |priority_queue_|. Returns nullptr
  // if the |priority_queue_| is empty.
  scoped_refptr<Sequence> GetWork();

  // Indicates whether the pool has been started yet.
  bool started_ GUARDED_BY(lock_) = false;

  // Number of threadpool work submitted to the pool which haven't popped a
  // Sequence from the PriorityQueue yet.
  size_t num_pending_threadpool_work_ GUARDED_BY(lock_) = 0;

#if DCHECK_IS_ON()
  // Set once JoinForTesting() has returned.
  bool join_for_testing_returned_ = false;
#endif

  DISALLOW_COPY_AND_ASSIGN(PlatformNativeWorkerPool);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_TASK_SCHEDULER_PLATFORM_NATIVE_WORKER_POOL_H_
