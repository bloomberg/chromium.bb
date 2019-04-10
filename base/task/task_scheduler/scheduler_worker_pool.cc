// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/scheduler_worker_pool.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/task/task_scheduler/task_tracker.h"
#include "base/threading/thread_local.h"

namespace base {
namespace internal {

namespace {

// SchedulerWorkerPool that owns the current thread, if any.
LazyInstance<ThreadLocalPointer<const SchedulerWorkerPool>>::Leaky
    tls_current_worker_pool = LAZY_INSTANCE_INITIALIZER;

const SchedulerWorkerPool* GetCurrentWorkerPool() {
  return tls_current_worker_pool.Get().Get();
}

}  // namespace

SchedulerWorkerPool::ScopedReenqueueExecutor::ScopedReenqueueExecutor() =
    default;

SchedulerWorkerPool::ScopedReenqueueExecutor::~ScopedReenqueueExecutor() {
  if (destination_pool_) {
    destination_pool_->PushSequenceAndWakeUpWorkers(
        std::move(sequence_and_transaction_.value()));
  }
}

void SchedulerWorkerPool::ScopedReenqueueExecutor::
    SchedulePushSequenceAndWakeUpWorkers(
        SequenceAndTransaction sequence_and_transaction,
        SchedulerWorkerPool* destination_pool) {
  DCHECK(destination_pool);
  DCHECK(!destination_pool_);
  DCHECK(!sequence_and_transaction_);
  sequence_and_transaction_.emplace(std::move(sequence_and_transaction));
  destination_pool_ = destination_pool;
}

SchedulerWorkerPool::SchedulerWorkerPool(TrackedRef<TaskTracker> task_tracker,
                                         TrackedRef<Delegate> delegate,
                                         SchedulerWorkerPool* predecessor_pool)
    : task_tracker_(std::move(task_tracker)),
      delegate_(std::move(delegate)),
      lock_(predecessor_pool ? &predecessor_pool->lock_ : nullptr) {
  DCHECK(task_tracker_);
}

SchedulerWorkerPool::~SchedulerWorkerPool() = default;

void SchedulerWorkerPool::BindToCurrentThread() {
  DCHECK(!GetCurrentWorkerPool());
  tls_current_worker_pool.Get().Set(this);
}

void SchedulerWorkerPool::UnbindFromCurrentThread() {
  DCHECK(GetCurrentWorkerPool());
  tls_current_worker_pool.Get().Set(nullptr);
}

bool SchedulerWorkerPool::IsBoundToCurrentThread() const {
  return GetCurrentWorkerPool() == this;
}

void SchedulerWorkerPool::OnCanScheduleSequence(
    scoped_refptr<Sequence> sequence) {
  if (replacement_pool_) {
    replacement_pool_->OnCanScheduleSequence(std::move(sequence));
    return;
  }

  PushSequenceAndWakeUpWorkers(
      SequenceAndTransaction::FromSequence(std::move(sequence)));
}

void SchedulerWorkerPool::PostTaskWithSequenceNow(
    Task task,
    SequenceAndTransaction sequence_and_transaction) {
  DCHECK(task.task);

  // Confirm that |task| is ready to run (its delayed run time is either null or
  // in the past).
  DCHECK_LE(task.delayed_run_time, TimeTicks::Now());

  const bool task_source_should_be_queued =
      sequence_and_transaction.transaction.PushTask(std::move(task));
  if (task_source_should_be_queued) {
    // Try to schedule the Sequence locked by |sequence_transaction|.
    if (task_tracker_->WillScheduleSequence(
            sequence_and_transaction.transaction, this)) {
      PushSequenceAndWakeUpWorkers(std::move(sequence_and_transaction));
    }
  }
}

bool SchedulerWorkerPool::RemoveSequence(scoped_refptr<Sequence> sequence) {
  AutoSchedulerLock auto_lock(lock_);
  return priority_queue_.RemoveSequence(std::move(sequence));
}

void SchedulerWorkerPool::ReEnqueueSequenceLockRequired(
    BaseScopedWorkersExecutor* workers_executor,
    ScopedReenqueueExecutor* reenqueue_executor,
    SequenceAndTransaction sequence_and_transaction) {
  // Decide in which pool the Sequence should be reenqueued.
  SchedulerWorkerPool* destination_pool = delegate_->GetWorkerPoolForTraits(
      sequence_and_transaction.transaction.traits());

  if (destination_pool == this) {
    // If the Sequence should be reenqueued in the current pool, reenqueue it
    // inside the scope of the lock.
    priority_queue_.Push(std::move(sequence_and_transaction.sequence),
                         sequence_and_transaction.transaction.GetSortKey());
    EnsureEnoughWorkersLockRequired(workers_executor);
  } else {
    // Otherwise, schedule a reenqueue after releasing the lock.
    reenqueue_executor->SchedulePushSequenceAndWakeUpWorkers(
        std::move(sequence_and_transaction), destination_pool);
  }
}

void SchedulerWorkerPool::UpdateSortKeyImpl(
    BaseScopedWorkersExecutor* executor,
    SequenceAndTransaction sequence_and_transaction) {
  AutoSchedulerLock auto_lock(lock_);
  priority_queue_.UpdateSortKey(std::move(sequence_and_transaction));
  EnsureEnoughWorkersLockRequired(executor);
}

void SchedulerWorkerPool::PushSequenceAndWakeUpWorkersImpl(
    BaseScopedWorkersExecutor* executor,
    SequenceAndTransaction sequence_and_transaction) {
  AutoSchedulerLock auto_lock(lock_);
  DCHECK(!replacement_pool_);
  priority_queue_.Push(std::move(sequence_and_transaction.sequence),
                       sequence_and_transaction.transaction.GetSortKey());
  EnsureEnoughWorkersLockRequired(executor);
}

void SchedulerWorkerPool::InvalidateAndHandoffAllSequencesToOtherPool(
    SchedulerWorkerPool* destination_pool) {
  AutoSchedulerLock current_pool_lock(lock_);
  AutoSchedulerLock destination_pool_lock(destination_pool->lock_);
  destination_pool->priority_queue_ = std::move(priority_queue_);
  replacement_pool_ = destination_pool;
}

}  // namespace internal
}  // namespace base
