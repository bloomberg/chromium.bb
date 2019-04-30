// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/test_utils.h"

#include <utility>

#include "base/bind.h"
#include "base/synchronization/condition_variable.h"
#include "base/task/thread_pool/scheduler_parallel_task_runner.h"
#include "base/task/thread_pool/scheduler_sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {
namespace test {

MockSchedulerWorkerObserver::MockSchedulerWorkerObserver()
    : on_main_exit_cv_(lock_.CreateConditionVariable()) {}

MockSchedulerWorkerObserver::~MockSchedulerWorkerObserver() {
  WaitCallsOnMainExit();
}

void MockSchedulerWorkerObserver::AllowCallsOnMainExit(int num_calls) {
  CheckedAutoLock auto_lock(lock_);
  EXPECT_EQ(0, allowed_calls_on_main_exit_);
  allowed_calls_on_main_exit_ = num_calls;
}

void MockSchedulerWorkerObserver::WaitCallsOnMainExit() {
  CheckedAutoLock auto_lock(lock_);
  while (allowed_calls_on_main_exit_ != 0)
    on_main_exit_cv_->Wait();
}

void MockSchedulerWorkerObserver::OnSchedulerWorkerMainExit() {
  CheckedAutoLock auto_lock(lock_);
  EXPECT_GE(allowed_calls_on_main_exit_, 0);
  --allowed_calls_on_main_exit_;
  if (allowed_calls_on_main_exit_ == 0)
    on_main_exit_cv_->Signal();
}

scoped_refptr<Sequence> CreateSequenceWithTask(
    Task task,
    const TaskTraits& traits,
    scoped_refptr<TaskRunner> task_runner,
    TaskSourceExecutionMode execution_mode) {
  scoped_refptr<Sequence> sequence =
      MakeRefCounted<Sequence>(traits, task_runner.get(), execution_mode);
  sequence->BeginTransaction().PushTask(std::move(task));
  return sequence;
}

scoped_refptr<TaskRunner> CreateTaskRunnerWithExecutionMode(
    test::ExecutionMode execution_mode,
    MockSchedulerTaskRunnerDelegate* mock_scheduler_task_runner_delegate,
    const TaskTraits& traits) {
  switch (execution_mode) {
    case test::ExecutionMode::PARALLEL:
      return CreateTaskRunnerWithTraits(traits,
                                        mock_scheduler_task_runner_delegate);
    case test::ExecutionMode::SEQUENCED:
      return CreateSequencedTaskRunnerWithTraits(
          traits, mock_scheduler_task_runner_delegate);
    default:
      // Fall through.
      break;
  }
  ADD_FAILURE() << "Unexpected ExecutionMode";
  return nullptr;
}

scoped_refptr<TaskRunner> CreateTaskRunnerWithTraits(
    const TaskTraits& traits,
    MockSchedulerTaskRunnerDelegate* mock_scheduler_task_runner_delegate) {
  return MakeRefCounted<SchedulerParallelTaskRunner>(
      traits, mock_scheduler_task_runner_delegate);
}

scoped_refptr<SequencedTaskRunner> CreateSequencedTaskRunnerWithTraits(
    const TaskTraits& traits,
    MockSchedulerTaskRunnerDelegate* mock_scheduler_task_runner_delegate) {
  return MakeRefCounted<SchedulerSequencedTaskRunner>(
      traits, mock_scheduler_task_runner_delegate);
}

// Waits on |event| in a scope where the blocking observer is null, to avoid
// affecting the max tasks in a worker pool.
void WaitWithoutBlockingObserver(WaitableEvent* event) {
  internal::ScopedClearBlockingObserverForTesting clear_blocking_observer;
  ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
  event->Wait();
}

MockSchedulerTaskRunnerDelegate::MockSchedulerTaskRunnerDelegate(
    TrackedRef<TaskTracker> task_tracker,
    DelayedTaskManager* delayed_task_manager)
    : task_tracker_(task_tracker),
      delayed_task_manager_(delayed_task_manager) {}

MockSchedulerTaskRunnerDelegate::~MockSchedulerTaskRunnerDelegate() = default;

bool MockSchedulerTaskRunnerDelegate::PostTaskWithSequence(
    Task task,
    scoped_refptr<Sequence> sequence) {
  // |worker_pool_| must be initialized with SetWorkerPool() before proceeding.
  DCHECK(worker_pool_);
  DCHECK(task.task);
  DCHECK(sequence);

  if (!task_tracker_->WillPostTask(&task, sequence->shutdown_behavior()))
    return false;

  if (task.delayed_run_time.is_null()) {
    worker_pool_->PostTaskWithSequenceNow(
        std::move(task),
        SequenceAndTransaction::FromSequence(std::move(sequence)));
  } else {
    // It's safe to take a ref on this pointer since the caller must have a ref
    // to the TaskRunner in order to post.
    scoped_refptr<TaskRunner> task_runner = sequence->task_runner();
    delayed_task_manager_->AddDelayedTask(
        std::move(task),
        BindOnce(
            [](scoped_refptr<Sequence> sequence,
               SchedulerWorkerPool* worker_pool, Task task) {
              worker_pool->PostTaskWithSequenceNow(
                  std::move(task),
                  SequenceAndTransaction::FromSequence(std::move(sequence)));
            },
            std::move(sequence), worker_pool_),
        std::move(task_runner));
  }

  return true;
}

bool MockSchedulerTaskRunnerDelegate::IsRunningPoolWithTraits(
    const TaskTraits& traits) const {
  // |worker_pool_| must be initialized with SetWorkerPool() before proceeding.
  DCHECK(worker_pool_);

  return worker_pool_->IsBoundToCurrentThread();
}

void MockSchedulerTaskRunnerDelegate::UpdatePriority(
    scoped_refptr<TaskSource> task_source,
    TaskPriority priority) {
  auto task_source_and_transaction =
      TaskSourceAndTransaction::FromTaskSource(std::move(task_source));
  task_source_and_transaction.transaction.UpdatePriority(priority);
  worker_pool_->UpdateSortKey(std::move(task_source_and_transaction));
}

void MockSchedulerTaskRunnerDelegate::SetWorkerPool(
    SchedulerWorkerPool* worker_pool) {
  worker_pool_ = worker_pool;
}

void ShutdownTaskTracker(TaskTracker* task_tracker) {
  task_tracker->StartShutdown();
  task_tracker->CompleteShutdown();
}

}  // namespace test
}  // namespace internal
}  // namespace base
