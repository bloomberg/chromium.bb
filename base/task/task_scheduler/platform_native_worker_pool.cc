// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/platform_native_worker_pool.h"

#include <algorithm>
#include <utility>

#include "base/system/sys_info.h"
#include "base/task/task_scheduler/task_tracker.h"

namespace base {
namespace internal {

class PlatformNativeWorkerPool::ScopedWorkersExecutor
    : public SchedulerWorkerPool::BaseScopedWorkersExecutor {
 public:
  ScopedWorkersExecutor(PlatformNativeWorkerPool* outer) : outer_(outer) {}
  ~ScopedWorkersExecutor() {
    SchedulerLock::AssertNoLockHeldOnCurrentThread();

    for (size_t i = 0; i < num_threadpool_work_to_submit_; ++i)
      outer_->SubmitWork();
  }

  // Sets the number of threadpool work to submit upon destruction.
  void set_num_threadpool_work_to_submit(size_t num) {
    DCHECK_EQ(num_threadpool_work_to_submit_, 0U);
    num_threadpool_work_to_submit_ = num;
  }

 private:
  PlatformNativeWorkerPool* const outer_;
  size_t num_threadpool_work_to_submit_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ScopedWorkersExecutor);
};

PlatformNativeWorkerPool::PlatformNativeWorkerPool(
    TrackedRef<TaskTracker> task_tracker,
    TrackedRef<Delegate> delegate,
    SchedulerWorkerPool* predecessor_pool)
    : SchedulerWorkerPool(std::move(task_tracker),
                          std::move(delegate),
                          predecessor_pool) {}

PlatformNativeWorkerPool::~PlatformNativeWorkerPool() {
#if DCHECK_IS_ON()
  // Verify join_for_testing has been called to ensure that there is no more
  // outstanding work. Otherwise, work may try to de-reference an invalid
  // pointer to this class.
  DCHECK(join_for_testing_returned_);
#endif
}

void PlatformNativeWorkerPool::Start(WorkerEnvironment worker_environment) {
  worker_environment_ = worker_environment;

  StartImpl();

  ScopedWorkersExecutor executor(this);
  AutoSchedulerLock auto_lock(lock_);
  DCHECK(!started_);
  started_ = true;
  EnsureEnoughWorkersLockRequired(&executor);
}

void PlatformNativeWorkerPool::JoinForTesting() {
  JoinImpl();
#if DCHECK_IS_ON()
  DCHECK(!join_for_testing_returned_);
  join_for_testing_returned_ = true;
#endif
}

void PlatformNativeWorkerPool::RunNextSequenceImpl() {
  BindToCurrentThread();

  scoped_refptr<Sequence> sequence = GetWork();
  DCHECK(sequence);

  sequence = task_tracker_->RunAndPopNextTask(std::move(sequence), this);

  if (sequence) {
    ScopedWorkersExecutor workers_executor(this);
    ScopedReenqueueExecutor reenqueue_executor;
    auto sequence_and_transaction =
        SequenceAndTransaction::FromSequence(std::move(sequence));
    AutoSchedulerLock auto_lock(lock_);
    ReEnqueueSequenceLockRequired(&workers_executor, &reenqueue_executor,
                                  std::move(sequence_and_transaction));
  }

  UnbindFromCurrentThread();
}

scoped_refptr<Sequence> PlatformNativeWorkerPool::GetWork() {
  AutoSchedulerLock auto_lock(lock_);
  DCHECK_GT(num_pending_threadpool_work_, 0U);
  --num_pending_threadpool_work_;
  // There can be more pending threadpool work than Sequences in the
  // PriorityQueue after RemoveSequence().
  if (priority_queue_.IsEmpty())
    return nullptr;
  return priority_queue_.PopSequence();
}

void PlatformNativeWorkerPool::UpdateSortKey(
    SequenceAndTransaction sequence_and_transaction) {
  ScopedWorkersExecutor executor(this);
  UpdateSortKeyImpl(&executor, std::move(sequence_and_transaction));
}

void PlatformNativeWorkerPool::PushSequenceAndWakeUpWorkers(
    SequenceAndTransaction sequence_and_transaction) {
  ScopedWorkersExecutor executor(this);
  PushSequenceAndWakeUpWorkersImpl(&executor,
                                   std::move(sequence_and_transaction));
}

void PlatformNativeWorkerPool::EnsureEnoughWorkersLockRequired(
    BaseScopedWorkersExecutor* executor) {
  if (!started_)
    return;
  // Ensure that there is at least one pending threadpool work per Sequence in
  // the PriorityQueue.
  const size_t desired_num_pending_threadpool_work = priority_queue_.Size();
  if (desired_num_pending_threadpool_work > num_pending_threadpool_work_) {
    static_cast<ScopedWorkersExecutor*>(executor)
        ->set_num_threadpool_work_to_submit(
            desired_num_pending_threadpool_work - num_pending_threadpool_work_);
    num_pending_threadpool_work_ = desired_num_pending_threadpool_work;
  }
}

size_t PlatformNativeWorkerPool::GetMaxConcurrentNonBlockedTasksDeprecated()
    const {
  // Native thread pools give us no control over the number of workers that are
  // active at one time. Consequently, we cannot report a true value here.
  // Instead, the values were chosen to match
  // TaskScheduler::StartWithDefaultParams.
  const int num_cores = SysInfo::NumberOfProcessors();
  return std::max(3, num_cores - 1);
}

void PlatformNativeWorkerPool::ReportHeartbeatMetrics() const {
  // Native thread pools do not provide the capability to determine the
  // number of worker threads created.
}

}  // namespace internal
}  // namespace base
