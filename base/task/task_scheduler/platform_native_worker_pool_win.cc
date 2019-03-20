// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/platform_native_worker_pool_win.h"

#include <algorithm>
#include <utility>

#include "base/system/sys_info.h"
#include "base/task/task_scheduler/task_tracker.h"

namespace base {
namespace internal {

class PlatformNativeWorkerPoolWin::ScopedWorkersExecutor
    : public SchedulerWorkerPool::BaseScopedWorkersExecutor {
 public:
  ScopedWorkersExecutor(PlatformNativeWorkerPoolWin* outer) : outer_(outer) {}
  ~ScopedWorkersExecutor() {
    SchedulerLock::AssertNoLockHeldOnCurrentThread();

    // TODO(fdoray): Handle priorities by having different work objects and
    // using SetThreadpoolCallbackPriority() and
    // SetThreadpoolCallbackRunsLong().
    for (size_t i = 0; i < num_threadpool_work_to_submit_; ++i)
      ::SubmitThreadpoolWork(outer_->work_);
  }

  // Sets the number of threadpool work to submit upon destruction.
  void set_num_threadpool_work_to_submit(size_t num) {
    DCHECK_EQ(num_threadpool_work_to_submit_, 0U);
    num_threadpool_work_to_submit_ = num;
  }

 private:
  PlatformNativeWorkerPoolWin* const outer_;
  size_t num_threadpool_work_to_submit_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ScopedWorkersExecutor);
};

PlatformNativeWorkerPoolWin::PlatformNativeWorkerPoolWin(
    TrackedRef<TaskTracker> task_tracker,
    TrackedRef<Delegate> delegate)
    : SchedulerWorkerPool(std::move(task_tracker), std::move(delegate)) {}

PlatformNativeWorkerPoolWin::~PlatformNativeWorkerPoolWin() {
#if DCHECK_IS_ON()
  // Verify join_for_testing has been called to ensure that there is no more
  // outstanding work. Otherwise, work may try to de-reference an invalid
  // pointer to this class.
  DCHECK(join_for_testing_returned_.IsSet());
#endif
  ::DestroyThreadpoolEnvironment(&environment_);
  ::CloseThreadpoolWork(work_);
  ::CloseThreadpool(pool_);
}

void PlatformNativeWorkerPoolWin::Start() {
  ::InitializeThreadpoolEnvironment(&environment_);

  pool_ = ::CreateThreadpool(nullptr);
  DCHECK(pool_) << "LastError: " << ::GetLastError();
  ::SetThreadpoolThreadMinimum(pool_, 1);
  ::SetThreadpoolThreadMaximum(pool_, 256);

  work_ = ::CreateThreadpoolWork(&RunNextSequence, this, &environment_);
  DCHECK(work_) << "LastError: " << GetLastError();
  ::SetThreadpoolCallbackPool(&environment_, pool_);

  ScopedWorkersExecutor executor(this);
  AutoSchedulerLock auto_lock(lock_);
  DCHECK(!started_);
  started_ = true;
  EnsureEnoughWorkersLockRequired(&executor);
}

void PlatformNativeWorkerPoolWin::JoinForTesting() {
  ::WaitForThreadpoolWorkCallbacks(work_, true);
#if DCHECK_IS_ON()
  DCHECK(!join_for_testing_returned_.IsSet());
  join_for_testing_returned_.Set();
#endif
}

// static
void CALLBACK PlatformNativeWorkerPoolWin::RunNextSequence(
    PTP_CALLBACK_INSTANCE,
    void* scheduler_worker_pool_windows_impl,
    PTP_WORK) {
  auto* worker_pool = static_cast<PlatformNativeWorkerPoolWin*>(
      scheduler_worker_pool_windows_impl);

  worker_pool->BindToCurrentThread();

  scoped_refptr<Sequence> sequence = worker_pool->GetWork();
  DCHECK(sequence);

  sequence = worker_pool->task_tracker_->RunAndPopNextTask(std::move(sequence),
                                                           worker_pool);

  if (sequence) {
    ScopedWorkersExecutor workers_executor(worker_pool);
    ScopedReenqueueExecutor reenqueue_executor;
    auto sequence_and_transaction =
        SequenceAndTransaction::FromSequence(std::move(sequence));
    AutoSchedulerLock auto_lock(worker_pool->lock_);
    worker_pool->ReEnqueueSequenceLockRequired(
        &workers_executor, &reenqueue_executor,
        std::move(sequence_and_transaction));
  }

  worker_pool->UnbindFromCurrentThread();
}

scoped_refptr<Sequence> PlatformNativeWorkerPoolWin::GetWork() {
  AutoSchedulerLock auto_lock(lock_);
  DCHECK_GT(num_pending_threadpool_work_, 0U);
  --num_pending_threadpool_work_;
  // There can be more pending threadpool work than Sequences in the
  // PriorityQueue after RemoveSequence().
  if (priority_queue_.IsEmpty())
    return nullptr;
  return priority_queue_.PopSequence();
}

void PlatformNativeWorkerPoolWin::UpdateSortKey(
    SequenceAndTransaction sequence_and_transaction) {
  ScopedWorkersExecutor executor(this);
  UpdateSortKeyImpl(&executor, std::move(sequence_and_transaction));
}

void PlatformNativeWorkerPoolWin::PushSequenceAndWakeUpWorkers(
    SequenceAndTransaction sequence_and_transaction) {
  ScopedWorkersExecutor executor(this);
  PushSequenceAndWakeUpWorkersImpl(&executor,
                                   std::move(sequence_and_transaction));
}

void PlatformNativeWorkerPoolWin::EnsureEnoughWorkersLockRequired(
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

size_t PlatformNativeWorkerPoolWin::GetMaxConcurrentNonBlockedTasksDeprecated()
    const {
  // The Windows Thread Pool API gives us no control over the number of workers
  // that are active at one time. Consequently, we cannot report a true value
  // here. Instead, the values were chosen to match
  // TaskScheduler::StartWithDefaultParams.
  const int num_cores = SysInfo::NumberOfProcessors();
  return std::max(3, num_cores - 1);
}

void PlatformNativeWorkerPoolWin::ReportHeartbeatMetrics() const {
  // Windows Thread Pool API does not provide the capability to determine the
  // number of worker threads created.
}

}  // namespace internal
}  // namespace base
