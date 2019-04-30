// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_SCHEDULER_WORKER_POOL_H_
#define BASE_TASK_THREAD_POOL_SCHEDULER_WORKER_POOL_H_

#include "base/base_export.h"
#include "base/memory/ref_counted.h"
#include "base/task/common/checked_lock.h"
#include "base/task/thread_pool/priority_queue.h"
#include "base/task/thread_pool/sequence.h"
#include "base/task/thread_pool/task.h"
#include "base/task/thread_pool/task_source.h"
#include "base/task/thread_pool/tracked_ref.h"
#include "build/build_config.h"

namespace base {
namespace internal {

class TaskTracker;

// Interface and base implementation for a worker pool.
class BASE_EXPORT SchedulerWorkerPool {
 public:
  // Delegate interface for SchedulerWorkerPool.
  class BASE_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked when the TaskSource in |task_source_and_transaction| is non-empty
    // after the SchedulerWorkerPool has run a task from it. The implementation
    // must return the pool in which the TaskSource should be reenqueued.
    virtual SchedulerWorkerPool* GetWorkerPoolForTraits(
        const TaskTraits& traits) = 0;
  };

  enum class WorkerEnvironment {
    // No special worker environment required.
    NONE,
#if defined(OS_WIN)
    // Initialize a COM MTA on the worker.
    COM_MTA,
#endif  // defined(OS_WIN)
  };

  virtual ~SchedulerWorkerPool();

  // Posts |task| to be executed by this SchedulerWorkerPool as part of
  // the Sequence in |sequence_and_transaction|. This must only be called after
  // |task| has gone through TaskTracker::WillPostTask() and after |task|'s
  // delayed run time.
  void PostTaskWithSequenceNow(Task task,
                               SequenceAndTransaction sequence_and_transaction);

  // Registers the worker pool in TLS.
  void BindToCurrentThread();

  // Resets the worker pool in TLS.
  void UnbindFromCurrentThread();

  // Returns true if the worker pool is registered in TLS.
  bool IsBoundToCurrentThread() const;

  // Removes |task_source| from |priority_queue_|. Returns true if successful,
  // or false if |task_source| is not currently in |priority_queue_|, such as
  // when a worker is running a task from it.
  bool RemoveTaskSource(scoped_refptr<TaskSource> task_source);

  // Updates the position of the TaskSource in |task_source_and_transaction| in
  // this pool's PriorityQueue based on the TaskSource's current traits.
  //
  // Implementations should instantiate a concrete ScopedWorkersExecutor and
  // invoke UpdateSortKeyImpl().
  virtual void UpdateSortKey(
      TaskSourceAndTransaction task_source_and_transaction) = 0;

  // Pushes the TaskSource in |task_source_and_transaction| into this pool's
  // PriorityQueue and wakes up workers as appropriate.
  //
  // Implementations should instantiate a concrete ScopedWorkersExecutor and
  // invoke PushTaskSourceAndWakeUpWorkersImpl().
  virtual void PushTaskSourceAndWakeUpWorkers(
      TaskSourceAndTransaction task_source_and_transaction) = 0;

  // Removes all task sources from this pool's PriorityQueue and enqueues them
  // in another |destination_pool|. After this method is called, any task
  // sources posted to this pool will be forwarded to |destination_pool|.
  //
  // TODO(crbug.com/756547): Remove this method once the UseNativeThreadPool
  // experiment is complete.
  void InvalidateAndHandoffAllTaskSourcesToOtherPool(
      SchedulerWorkerPool* destination_pool);

  // Prevents new tasks from starting to run and waits for currently running
  // tasks to complete their execution. It is guaranteed that no thread will do
  // work on behalf of this SchedulerWorkerPool after this returns. It is
  // invalid to post a task once this is called. TaskTracker::Flush() can be
  // called before this to complete existing tasks, which might otherwise post a
  // task during JoinForTesting(). This can only be called once.
  virtual void JoinForTesting() = 0;

  // Returns the maximum number of non-blocked tasks that can run concurrently
  // in this pool.
  //
  // TODO(fdoray): Remove this method. https://crbug.com/687264
  virtual size_t GetMaxConcurrentNonBlockedTasksDeprecated() const = 0;

  // Reports relevant metrics per implementation.
  virtual void ReportHeartbeatMetrics() const = 0;

  // Wakes up workers as appropriate for the new CanRunPolicy policy. Must be
  // called after an update to CanRunPolicy in TaskTracker.
  virtual void DidUpdateCanRunPolicy() = 0;

 protected:
  // Derived classes must implement a ScopedWorkersExecutor that derives from
  // this to perform operations on workers at the end of a scope, when all locks
  // have been released.
  class BaseScopedWorkersExecutor {
   protected:
    BaseScopedWorkersExecutor() = default;
    ~BaseScopedWorkersExecutor() = default;
    DISALLOW_COPY_AND_ASSIGN(BaseScopedWorkersExecutor);
  };

  // Allows a task source to be pushed to a pool's PriorityQueue at the end of a
  // scope, when all locks have been released.
  class ScopedReenqueueExecutor {
   public:
    ScopedReenqueueExecutor();
    ~ScopedReenqueueExecutor();

    void SchedulePushTaskSourceAndWakeUpWorkers(
        TaskSourceAndTransaction task_source_and_transaction,
        SchedulerWorkerPool* destination_pool);

   private:
    // A TaskSourceAndTransaction and the pool in which it should be enqueued.
    Optional<TaskSourceAndTransaction> task_source_and_transaction_;
    SchedulerWorkerPool* destination_pool_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ScopedReenqueueExecutor);
  };

  // |predecessor_pool| is a pool whose lock can be acquired before the
  // constructed pool's lock. This is necessary to move all task sources from
  // |predecessor_pool| to the constructed pool and support the
  // UseNativeThreadPool experiment.
  //
  // TODO(crbug.com/756547): Remove |predecessor_pool| once the experiment is
  // complete.
  SchedulerWorkerPool(TrackedRef<TaskTracker> task_tracker,
                      TrackedRef<Delegate> delegate,
                      SchedulerWorkerPool* predecessor_pool = nullptr);

  const TrackedRef<TaskTracker> task_tracker_;
  const TrackedRef<Delegate> delegate_;

  // Returns the number of queued BEST_EFFORT task sources allowed to run by the
  // current CanRunPolicy.
  size_t GetNumQueuedCanRunBestEffortTaskSources() const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns the number of queued USER_VISIBLE/USER_BLOCKING task sources
  // allowed to run by the current CanRunPolicy.
  size_t GetNumQueuedCanRunForegroundTaskSources() const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Ensures that there are enough workers to run queued task sources.
  // |executor| is forwarded from the one received in
  // PushTaskSourceAndWakeUpWorkersImpl()
  virtual void EnsureEnoughWorkersLockRequired(
      BaseScopedWorkersExecutor* executor) EXCLUSIVE_LOCKS_REQUIRED(lock_) = 0;

  // Reenqueues a |task_source_and_transaction| from which a Task just ran in
  // the current pool into the appropriate pool.
  void ReEnqueueTaskSourceLockRequired(
      BaseScopedWorkersExecutor* workers_executor,
      ScopedReenqueueExecutor* reenqueue_executor,
      TaskSourceAndTransaction task_source_and_transaction)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Must be invoked by implementations of the corresponding non-Impl() methods.
  void UpdateSortKeyImpl(BaseScopedWorkersExecutor* executor,
                         TaskSourceAndTransaction task_source_and_transaction);
  void PushTaskSourceAndWakeUpWorkersImpl(
      BaseScopedWorkersExecutor* executor,
      TaskSourceAndTransaction task_source_and_transaction);

  // Synchronizes accesses to all members of this class which are neither const,
  // atomic, nor immutable after start. Since this lock is a bottleneck to post
  // and schedule work, only simple data structure manipulations are allowed
  // within its scope (no thread creation or wake up).
  mutable CheckedLock lock_;

  // PriorityQueue from which all threads of this worker pool get work.
  PriorityQueue priority_queue_ GUARDED_BY(lock_);

  // If |replacement_pool_| is non-null, this pool is invalid and all task
  // sources should be scheduled on |replacement_pool_|. Used to support the
  // UseNativeThreadPool experiment.
  SchedulerWorkerPool* replacement_pool_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerPool);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_SCHEDULER_WORKER_POOL_H_
