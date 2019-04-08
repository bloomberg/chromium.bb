// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_SCHEDULER_WORKER_POOL_H_
#define BASE_TASK_TASK_SCHEDULER_SCHEDULER_WORKER_POOL_H_

#include "base/base_export.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_scheduler/can_schedule_sequence_observer.h"
#include "base/task/task_scheduler/priority_queue.h"
#include "base/task/task_scheduler/scheduler_lock.h"
#include "base/task/task_scheduler/sequence.h"
#include "base/task/task_scheduler/task.h"
#include "base/task/task_scheduler/tracked_ref.h"
#include "build/build_config.h"

namespace base {
namespace internal {

class TaskTracker;

// Interface for a worker pool.
class BASE_EXPORT SchedulerWorkerPool : public CanScheduleSequenceObserver {
 public:
  // Delegate interface for SchedulerWorkerPool.
  class BASE_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked when the Sequence in |sequence_and_transaction| is non-empty
    // after the SchedulerWorkerPool has run a task from it. The implementation
    // must return the pool in which the Sequence should be reenqueued.
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

  ~SchedulerWorkerPool() override;

  // CanScheduleSequenceObserver:
  void OnCanScheduleSequence(scoped_refptr<Sequence> sequence) final;

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

  // Removes |sequence| from |priority_queue_|. Returns true if successful, or
  // false if |sequence| is not currently in |priority_queue_|, such as when a
  // worker is running a task from it.
  bool RemoveSequence(scoped_refptr<Sequence> sequence);

  // Updates the position of the Sequence in |sequence_and_transaction| in
  // this pool's PriorityQueue based on the Sequence's current traits.
  //
  // Implementations should instantiate a concrete ScopedWorkersExecutor and
  // invoke UpdateSortKeyImpl().
  virtual void UpdateSortKey(
      SequenceAndTransaction sequence_and_transaction) = 0;

  // Pushes the Sequence in |sequence_and_transaction| into this pool's
  // PriorityQueue and wakes up workers as appropriate.
  //
  // Implementations should instantiate a concrete ScopedWorkersExecutor and
  // invoke PushSequenceAndWakeUpWorkersImpl().
  virtual void PushSequenceAndWakeUpWorkers(
      SequenceAndTransaction sequence_and_transaction) = 0;

  // Removes all sequences from this pool's PriorityQueue and enqueues them in
  // another |destination_pool|. After this method is called, any sequences
  // posted to this pool will be forwarded to |destination_pool|.
  //
  // TODO(crbug.com/756547): Remove this method once the UseNativeThreadPool
  // experiment is complete.
  void InvalidateAndHandoffAllSequencesToOtherPool(
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

  // Allows a sequence to be pushed to a pool's PriorityQueue at the end of a
  // scope, when all locks have been released.
  class ScopedReenqueueExecutor {
   public:
    ScopedReenqueueExecutor();
    ~ScopedReenqueueExecutor();

    void SchedulePushSequenceAndWakeUpWorkers(
        SequenceAndTransaction sequence_and_transaction,
        SchedulerWorkerPool* destination_pool);

   private:
    // A SequenceAndTransaction and the pool in which it should be enqueued.
    Optional<SequenceAndTransaction> sequence_and_transaction_;
    SchedulerWorkerPool* destination_pool_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ScopedReenqueueExecutor);
  };

  // |predecessor_pool| is a pool whose lock can be acquired before the
  // constructed pool's lock. This is necessary to move all sequences from
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

  // Ensures that there are enough workers to run queued sequences. |executor|
  // is forwarded from the one received in PushSequenceAndWakeUpWorkersImpl()
  virtual void EnsureEnoughWorkersLockRequired(
      BaseScopedWorkersExecutor* executor) EXCLUSIVE_LOCKS_REQUIRED(lock_) = 0;

  // Reenqueues a |sequence_and_transaction| from which a Task just ran in the
  // current pool into the appropriate pool.
  void ReEnqueueSequenceLockRequired(
      BaseScopedWorkersExecutor* workers_executor,
      ScopedReenqueueExecutor* reenqueue_executor,
      SequenceAndTransaction sequence_and_transaction)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Must be invoked by implementations of the corresponding non-Impl() methods.
  void UpdateSortKeyImpl(BaseScopedWorkersExecutor* executor,
                         SequenceAndTransaction sequence_and_transaction);
  void PushSequenceAndWakeUpWorkersImpl(
      BaseScopedWorkersExecutor* executor,
      SequenceAndTransaction sequence_and_transaction);

  // Synchronizes accesses to all members of this class which are neither const,
  // atomic, nor immutable after start. Since this lock is a bottleneck to post
  // and schedule work, only simple data structure manipulations are allowed
  // within its scope (no thread creation or wake up).
  mutable SchedulerLock lock_;

  // PriorityQueue from which all threads of this worker pool get work.
  PriorityQueue priority_queue_ GUARDED_BY(lock_);

  // If |replacement_pool_| is non-null, this pool is invalid and all sequences
  // should be scheduled on |replacement_pool_|. Used to support the
  // UseNativeThreadPool experiment.
  SchedulerWorkerPool* replacement_pool_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerPool);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_TASK_SCHEDULER_SCHEDULER_WORKER_POOL_H_
