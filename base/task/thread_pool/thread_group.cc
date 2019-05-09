// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_group.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/threading/thread_local.h"

namespace base {
namespace internal {

namespace {

// ThreadGroup that owns the current thread, if any.
LazyInstance<ThreadLocalPointer<const ThreadGroup>>::Leaky
    tls_current_thread_group = LAZY_INSTANCE_INITIALIZER;

const ThreadGroup* GetCurrentThreadGroup() {
  return tls_current_thread_group.Get().Get();
}

}  // namespace

ThreadGroup::ScopedReenqueueExecutor::ScopedReenqueueExecutor() = default;

ThreadGroup::ScopedReenqueueExecutor::~ScopedReenqueueExecutor() {
  if (destination_thread_group_) {
    destination_thread_group_->PushTaskSourceAndWakeUpWorkers(
        std::move(task_source_and_transaction_.value()));
  }
}

void ThreadGroup::ScopedReenqueueExecutor::
    SchedulePushTaskSourceAndWakeUpWorkers(
        TaskSourceAndTransaction task_source_and_transaction,
        ThreadGroup* destination_thread_group) {
  DCHECK(destination_thread_group);
  DCHECK(!destination_thread_group_);
  DCHECK(!task_source_and_transaction_);
  task_source_and_transaction_.emplace(std::move(task_source_and_transaction));
  destination_thread_group_ = destination_thread_group;
}

ThreadGroup::ThreadGroup(TrackedRef<TaskTracker> task_tracker,
                         TrackedRef<Delegate> delegate,
                         ThreadGroup* predecessor_thread_group)
    : task_tracker_(std::move(task_tracker)),
      delegate_(std::move(delegate)),
      lock_(predecessor_thread_group ? &predecessor_thread_group->lock_
                                     : nullptr) {
  DCHECK(task_tracker_);
}

ThreadGroup::~ThreadGroup() = default;

void ThreadGroup::BindToCurrentThread() {
  DCHECK(!GetCurrentThreadGroup());
  tls_current_thread_group.Get().Set(this);
}

void ThreadGroup::UnbindFromCurrentThread() {
  DCHECK(GetCurrentThreadGroup());
  tls_current_thread_group.Get().Set(nullptr);
}

bool ThreadGroup::IsBoundToCurrentThread() const {
  return GetCurrentThreadGroup() == this;
}

size_t ThreadGroup::GetNumQueuedCanRunBestEffortTaskSources() const {
  const size_t num_queued =
      priority_queue_.GetNumTaskSourcesWithPriority(TaskPriority::BEST_EFFORT);
  if (num_queued == 0 ||
      !task_tracker_->CanRunPriority(TaskPriority::BEST_EFFORT)) {
    return 0U;
  }
  return num_queued;
}

size_t ThreadGroup::GetNumQueuedCanRunForegroundTaskSources() const {
  const size_t num_queued = priority_queue_.GetNumTaskSourcesWithPriority(
                                TaskPriority::USER_VISIBLE) +
                            priority_queue_.GetNumTaskSourcesWithPriority(
                                TaskPriority::USER_BLOCKING);
  if (num_queued == 0 ||
      !task_tracker_->CanRunPriority(TaskPriority::HIGHEST)) {
    return 0U;
  }
  return num_queued;
}

bool ThreadGroup::RemoveTaskSource(scoped_refptr<TaskSource> task_source) {
  CheckedAutoLock auto_lock(lock_);
  return priority_queue_.RemoveTaskSource(std::move(task_source));
}

void ThreadGroup::ReEnqueueTaskSourceLockRequired(
    BaseScopedWorkersExecutor* workers_executor,
    ScopedReenqueueExecutor* reenqueue_executor,
    TaskSourceAndTransaction task_source_and_transaction) {
  // Decide in which thread group the TaskSource should be reenqueued.
  ThreadGroup* destination_thread_group = delegate_->GetThreadGroupForTraits(
      task_source_and_transaction.transaction.traits());

  if (destination_thread_group == this) {
    // If the TaskSource should be reenqueued in the current thread group,
    // reenqueue it inside the scope of the lock.
    priority_queue_.Push(std::move(task_source_and_transaction.task_source),
                         task_source_and_transaction.transaction.GetSortKey());
    EnsureEnoughWorkersLockRequired(workers_executor);
  } else {
    // Otherwise, schedule a reenqueue after releasing the lock.
    reenqueue_executor->SchedulePushTaskSourceAndWakeUpWorkers(
        std::move(task_source_and_transaction), destination_thread_group);
  }
}

void ThreadGroup::UpdateSortKeyImpl(
    BaseScopedWorkersExecutor* executor,
    TaskSourceAndTransaction task_source_and_transaction) {
  CheckedAutoLock auto_lock(lock_);
  priority_queue_.UpdateSortKey(std::move(task_source_and_transaction));
  EnsureEnoughWorkersLockRequired(executor);
}

void ThreadGroup::PushTaskSourceAndWakeUpWorkersImpl(
    BaseScopedWorkersExecutor* executor,
    TaskSourceAndTransaction task_source_and_transaction) {
  CheckedAutoLock auto_lock(lock_);
  DCHECK(!replacement_thread_group_);
  priority_queue_.Push(std::move(task_source_and_transaction.task_source),
                       task_source_and_transaction.transaction.GetSortKey());
  EnsureEnoughWorkersLockRequired(executor);
}

void ThreadGroup::InvalidateAndHandoffAllTaskSourcesToOtherThreadGroup(
    ThreadGroup* destination_thread_group) {
  CheckedAutoLock current_thread_group_lock(lock_);
  CheckedAutoLock destination_thread_group_lock(
      destination_thread_group->lock_);
  destination_thread_group->priority_queue_ = std::move(priority_queue_);
  replacement_thread_group_ = destination_thread_group;
}

}  // namespace internal
}  // namespace base
