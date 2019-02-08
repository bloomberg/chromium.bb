// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/scheduler_worker_pool_impl.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/atomicops.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/sequence_token.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_features.h"
#include "base/task/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task/task_scheduler/task_tracker.h"
#include "base/task/task_traits.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_windows_thread_environment.h"
#include "base/win/scoped_winrt_initializer.h"
#include "base/win/windows_version.h"
#endif  // defined(OS_WIN)

namespace base {
namespace internal {

namespace {

constexpr char kPoolNameSuffix[] = "Pool";
constexpr char kDetachDurationHistogramPrefix[] =
    "TaskScheduler.DetachDuration.";
constexpr char kNumTasksBeforeDetachHistogramPrefix[] =
    "TaskScheduler.NumTasksBeforeDetach.";
constexpr char kNumTasksBetweenWaitsHistogramPrefix[] =
    "TaskScheduler.NumTasksBetweenWaits.";
constexpr char kNumWorkersHistogramPrefix[] = "TaskScheduler.NumWorkers.";
constexpr char kNumActiveWorkersHistogramPrefix[] =
    "TaskScheduler.NumActiveWorkers.";
constexpr size_t kMaxNumberOfWorkers = 256;

// In a background pool:
// - Blocking calls take more time than in a foreground pool.
// - We want to minimize impact on foreground work, not maximize execution
//   throughput.
// For these reasons, the timeout to increase the maximum number of concurrent
// tasks when there is a MAY_BLOCK ScopedBlockingCall is *long*. It is not
// infinite because execution throughput should not be reduced forever if a task
// blocks forever.
//
// TODO(fdoray): On platforms without background pools, blocking in a
// BEST_EFFORT task should:
// 1. Increment the maximum number of concurrent tasks after a *short* timeout,
//    to allow scheduling of USER_VISIBLE/USER_BLOCKING tasks.
// 2. Increment the maximum number of concurrent BEST_EFFORT tasks after a
//    *long* timeout, because we only want to allow more BEST_EFFORT tasks to be
//    be scheduled concurrently when we believe that a BEST_EFFORT task is
//    blocked forever.
// Currently, only 1. is true as the configuration is per pool.
// TODO(https://crbug.com/927755): Fix racy condition when MayBlockThreshold ==
// BlockedWorkersPoll.
constexpr TimeDelta kForegroundMayBlockThreshold =
    TimeDelta::FromMilliseconds(1000);
constexpr TimeDelta kForegroundBlockedWorkersPoll =
    TimeDelta::FromMilliseconds(1200);
constexpr TimeDelta kBackgroundMayBlockThreshold = TimeDelta::FromSeconds(10);
constexpr TimeDelta kBackgroundBlockedWorkersPoll = TimeDelta::FromSeconds(12);

// Only used in DCHECKs.
bool ContainsWorker(const std::vector<scoped_refptr<SchedulerWorker>>& workers,
                    const SchedulerWorker* worker) {
  auto it = std::find_if(workers.begin(), workers.end(),
                         [worker](const scoped_refptr<SchedulerWorker>& i) {
                           return i.get() == worker;
                         });
  return it != workers.end();
}

}  // namespace

// Accumulates workers and executes actions on them upon destruction. Useful
// to satisfy locking requirements of worker actions.
class SchedulerWorkerPoolImpl::SchedulerWorkerActionExecutor {
 public:
  SchedulerWorkerActionExecutor(TrackedRef<SchedulerWorkerPoolImpl> outer)
      : outer_(outer) {}

  ~SchedulerWorkerActionExecutor() {
    SchedulerLock::AssertNoLockHeldOnCurrentThread();

    // Wake up workers.
    workers_to_wake_up_.ForEachWorker(
        [](SchedulerWorker* worker) { worker->WakeUp(); });

    // Start workers. Happens after wake ups to prevent the case where a worker
    // enters its main function, is descheduled because it wasn't woken up yet,
    // and is woken up immediately after.
    workers_to_start_.ForEachWorker([&](SchedulerWorker* worker) {
      worker->Start(outer_->after_start().scheduler_worker_observer);
    });
  }

  void ScheduleWakeUp(scoped_refptr<SchedulerWorker> worker) {
    workers_to_wake_up_.AddWorker(std::move(worker));
  }

  void ScheduleStart(scoped_refptr<SchedulerWorker> worker) {
    workers_to_start_.AddWorker(std::move(worker));
  }

 private:
  class WorkerContainer {
   public:
    WorkerContainer() = default;

    void AddWorker(scoped_refptr<SchedulerWorker> worker) {
      if (!worker)
        return;
      if (!first_worker_)
        first_worker_ = std::move(worker);
      else
        additional_workers_.push_back(std::move(worker));
    }

    template <typename Action>
    void ForEachWorker(Action action) {
      if (first_worker_) {
        action(first_worker_.get());
        for (scoped_refptr<SchedulerWorker> worker : additional_workers_)
          action(worker.get());
      } else {
        DCHECK(additional_workers_.empty());
      }
    }

   private:
    // The purpose of |first_worker| is to avoid a heap allocation by the vector
    // in the case where there is only one worker in the container.
    scoped_refptr<SchedulerWorker> first_worker_;
    std::vector<scoped_refptr<SchedulerWorker>> additional_workers_;

    DISALLOW_COPY_AND_ASSIGN(WorkerContainer);
  };

  const TrackedRef<SchedulerWorkerPoolImpl> outer_;

  WorkerContainer workers_to_wake_up_;
  WorkerContainer workers_to_start_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerActionExecutor);
};

class SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl
    : public SchedulerWorker::Delegate,
      public BlockingObserver {
 public:
  // |outer| owns the worker for which this delegate is constructed.
  SchedulerWorkerDelegateImpl(TrackedRef<SchedulerWorkerPoolImpl> outer);
  ~SchedulerWorkerDelegateImpl() override;

  // SchedulerWorker::Delegate:
  void OnCanScheduleSequence(scoped_refptr<Sequence> sequence) override;
  SchedulerWorker::ThreadLabel GetThreadLabel() const override;
  void OnMainEntry(const SchedulerWorker* worker) override;
  scoped_refptr<Sequence> GetWork(SchedulerWorker* worker) override;
  void DidRunTask(scoped_refptr<Sequence> sequence) override;
  TimeDelta GetSleepTimeout() override;
  void OnMainExit(SchedulerWorker* worker) override;

  // BlockingObserver:
  void BlockingStarted(BlockingType blocking_type) override;
  void BlockingTypeUpgraded() override;
  void BlockingEnded() override;

  void MayBlockEntered();
  void WillBlockEntered();

  // Returns true iff the worker can get work. Cleans up the worker or puts it
  // on the idle stack if it can't get work.
  bool CanGetWorkLockRequired(SchedulerWorker* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Returns true iff this worker has been within a MAY_BLOCK ScopedBlockingCall
  // for more than |may_block_threshold|. The max tasks must be
  // incremented if this returns true.
  bool MustIncrementMaxTasksLockRequired()
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  bool is_running_best_effort_task_lock_required() const
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) {
    return read_any().is_running_best_effort_task;
  }

  // Exposed for AnnotateSchedulerLockAcquired in
  // SchedulerWorkerPoolImpl::AdjustMaxTasks()
  const SchedulerLock& lock() const LOCK_RETURNED(outer_->lock_) {
    return outer_->lock_;
  }

 private:
  // Returns true if |worker| is allowed to cleanup and remove itself from the
  // pool. Called from GetWork() when no work is available.
  bool CanCleanupLockRequired(const SchedulerWorker* worker) const
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Calls cleanup on |worker| and removes it from the pool. Called from
  // GetWork() when no work is available and CanCleanupLockRequired() returns
  // true.
  void CleanupLockRequired(SchedulerWorker* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Called in GetWork() when a worker becomes idle.
  void OnWorkerBecomesIdleLockRequired(SchedulerWorker* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Accessed only from the worker thread.
  struct WorkerOnly {
    // Number of tasks executed since the last time the
    // TaskScheduler.NumTasksBetweenWaits histogram was recorded.
    size_t num_tasks_since_last_wait = 0;

    // Number of tasks executed since the last time the
    // TaskScheduler.NumTasksBeforeDetach histogram was recorded.
    size_t num_tasks_since_last_detach = 0;

    // Whether the worker is currently running a task (i.e. GetWork() has
    // returned a non-empty sequence and DidRunTask() hasn't been called yet).
    bool is_running_task = false;

#if defined(OS_WIN)
    std::unique_ptr<win::ScopedWindowsThreadEnvironment> win_thread_environment;
#endif  // defined(OS_WIN)
  } worker_only_;

  // Writes from the worker thread protected by |outer_->lock_|. Reads from any
  // thread, protected by |outer_->lock_| when not on the worker thread.
  struct WriteWorkerReadAny {
    // Whether the worker is currently running a TaskPriority::BEST_EFFORT task.
    bool is_running_best_effort_task = false;

    // Time when MayBlockScopeEntered() was last called. Reset when
    // BlockingScopeExited() is called.
    TimeTicks may_block_start_time;
  } write_worker_read_any_;

  WorkerOnly& worker_only() {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    return worker_only_;
  }

  WriteWorkerReadAny& write_worker() EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    return write_worker_read_any_;
  }

  const WriteWorkerReadAny& read_any() const
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) {
    return write_worker_read_any_;
  }

  const WriteWorkerReadAny& read_worker() const {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    return write_worker_read_any_;
  }

  const TrackedRef<SchedulerWorkerPoolImpl> outer_;

  // Whether |outer_->max_tasks_| was incremented due to a ScopedBlockingCall on
  // the thread.
  bool incremented_max_tasks_since_blocked_ GUARDED_BY(outer_->lock_) = false;

  // Verifies that specific calls are always made from the worker thread.
  THREAD_CHECKER(worker_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerDelegateImpl);
};

SchedulerWorkerPoolImpl::SchedulerWorkerPoolImpl(
    StringPiece histogram_label,
    StringPiece pool_label,
    ThreadPriority priority_hint,
    TrackedRef<TaskTracker> task_tracker,
    TrackedRef<Delegate> delegate)
    : SchedulerWorkerPool(std::move(task_tracker), std::move(delegate)),
      pool_label_(pool_label.as_string()),
      priority_hint_(priority_hint),
      idle_workers_stack_cv_for_testing_(lock_.CreateConditionVariable()),
      // Mimics the UMA_HISTOGRAM_LONG_TIMES macro.
      detach_duration_histogram_(Histogram::FactoryTimeGet(
          JoinString({kDetachDurationHistogramPrefix, histogram_label,
                      kPoolNameSuffix},
                     ""),
          TimeDelta::FromMilliseconds(1),
          TimeDelta::FromHours(1),
          50,
          HistogramBase::kUmaTargetedHistogramFlag)),
      // Mimics the UMA_HISTOGRAM_COUNTS_1000 macro. When a worker runs more
      // than 1000 tasks before detaching, there is no need to know the exact
      // number of tasks that ran.
      num_tasks_before_detach_histogram_(Histogram::FactoryGet(
          JoinString({kNumTasksBeforeDetachHistogramPrefix, histogram_label,
                      kPoolNameSuffix},
                     ""),
          1,
          1000,
          50,
          HistogramBase::kUmaTargetedHistogramFlag)),
      // Mimics the UMA_HISTOGRAM_COUNTS_100 macro. A SchedulerWorker is
      // expected to run between zero and a few tens of tasks between waits.
      // When it runs more than 100 tasks, there is no need to know the exact
      // number of tasks that ran.
      num_tasks_between_waits_histogram_(Histogram::FactoryGet(
          JoinString({kNumTasksBetweenWaitsHistogramPrefix, histogram_label,
                      kPoolNameSuffix},
                     ""),
          1,
          100,
          50,
          HistogramBase::kUmaTargetedHistogramFlag)),
      // Mimics the UMA_HISTOGRAM_COUNTS_100 macro. A SchedulerWorkerPool is
      // expected to run between zero and a few tens of workers.
      // When it runs more than 100 worker, there is no need to know the exact
      // number of workers that ran.
      num_workers_histogram_(Histogram::FactoryGet(
          JoinString(
              {kNumWorkersHistogramPrefix, histogram_label, kPoolNameSuffix},
              ""),
          1,
          100,
          50,
          HistogramBase::kUmaTargetedHistogramFlag)),
      num_active_workers_histogram_(
          Histogram::FactoryGet(JoinString({kNumActiveWorkersHistogramPrefix,
                                            histogram_label, kPoolNameSuffix},
                                           ""),
                                1,
                                100,
                                50,
                                HistogramBase::kUmaTargetedHistogramFlag)),
      tracked_ref_factory_(this) {
  DCHECK(!histogram_label.empty());
  DCHECK(!pool_label_.empty());
}

void SchedulerWorkerPoolImpl::Start(
    const SchedulerWorkerPoolParams& params,
    int max_best_effort_tasks,
    scoped_refptr<TaskRunner> service_thread_task_runner,
    SchedulerWorkerObserver* scheduler_worker_observer,
    WorkerEnvironment worker_environment,
    Optional<TimeDelta> may_block_threshold) {
  SchedulerWorkerActionExecutor executor(tracked_ref_factory_.GetTrackedRef());

  AutoSchedulerLock auto_lock(lock_);

  DCHECK(workers_.empty());

  in_start().may_block_without_delay_ =
      FeatureList::IsEnabled(kMayBlockWithoutDelay);
  in_start().may_block_threshold =
      may_block_threshold ? may_block_threshold.value()
                          : (priority_hint_ == ThreadPriority::NORMAL
                                 ? kForegroundMayBlockThreshold
                                 : kBackgroundMayBlockThreshold);
  in_start().blocked_workers_poll_period =
      priority_hint_ == ThreadPriority::NORMAL ? kForegroundBlockedWorkersPoll
                                               : kBackgroundBlockedWorkersPoll;

  max_tasks_ = params.max_tasks();
  DCHECK_GE(max_tasks_, 1U);
  in_start().initial_max_tasks = max_tasks_;
  DCHECK_LE(in_start().initial_max_tasks, kMaxNumberOfWorkers);
  max_best_effort_tasks_ = max_best_effort_tasks;
  in_start().suggested_reclaim_time = params.suggested_reclaim_time();
  in_start().backward_compatibility = params.backward_compatibility();
  in_start().worker_environment = worker_environment;
  in_start().service_thread_task_runner = std::move(service_thread_task_runner);
  in_start().scheduler_worker_observer = scheduler_worker_observer;

#if DCHECK_IS_ON()
  in_start().initialized = true;
#endif

  // The initial number of workers is |num_wake_ups_before_start_| + 1 to try to
  // keep one at least one standby thread at all times (capacity permitting).
  const int num_initial_workers =
      std::min(num_wake_ups_before_start_ + 1, static_cast<int>(max_tasks_));
  workers_.reserve(num_initial_workers);

  for (int index = 0; index < num_initial_workers; ++index) {
    auto worker = CreateAndRegisterWorkerLockRequired(&executor);

    if (index < num_wake_ups_before_start_)
      executor.ScheduleWakeUp(std::move(worker));
    else
      idle_workers_stack_.Push(worker.get());
  }
}

SchedulerWorkerPoolImpl::~SchedulerWorkerPoolImpl() {
  // SchedulerWorkerPool should only ever be deleted:
  //  1) In tests, after JoinForTesting().
  //  2) In production, iff initialization failed.
  // In both cases |workers_| should be empty.
  DCHECK(workers_.empty());
}

void SchedulerWorkerPoolImpl::OnCanScheduleSequence(
    scoped_refptr<Sequence> sequence) {
  OnCanScheduleSequence(
      SequenceAndTransaction::FromSequence(std::move(sequence)));
}

void SchedulerWorkerPoolImpl::OnCanScheduleSequence(
    SequenceAndTransaction sequence_and_transaction) {
  PushSequenceToPriorityQueue(std::move(sequence_and_transaction));
  WakeUpOneWorker();
}

void SchedulerWorkerPoolImpl::PushSequenceToPriorityQueue(
    SequenceAndTransaction sequence_and_transaction) {
  DCHECK(sequence_and_transaction.sequence);
  AutoSchedulerLock auto_lock(lock_);
  priority_queue_.Push(std::move(sequence_and_transaction.sequence),
                       sequence_and_transaction.transaction.GetSortKey());
}

void SchedulerWorkerPoolImpl::GetHistograms(
    std::vector<const HistogramBase*>* histograms) const {
  histograms->push_back(detach_duration_histogram_);
  histograms->push_back(num_tasks_between_waits_histogram_);
  histograms->push_back(num_workers_histogram_);
  histograms->push_back(num_active_workers_histogram_);
}

size_t SchedulerWorkerPoolImpl::GetMaxConcurrentNonBlockedTasksDeprecated()
    const {
#if DCHECK_IS_ON()
  AutoSchedulerLock auto_lock(lock_);
  DCHECK_NE(after_start().initial_max_tasks, 0U)
      << "GetMaxConcurrentTasksDeprecated() should only be called after the "
      << "worker pool has started.";
#endif
  return after_start().initial_max_tasks;
}

void SchedulerWorkerPoolImpl::WaitForWorkersIdleForTesting(size_t n) {
  AutoSchedulerLock auto_lock(lock_);

#if DCHECK_IS_ON()
  DCHECK(!some_workers_cleaned_up_for_testing_)
      << "Workers detached prior to waiting for a specific number of idle "
         "workers. Doing the wait under such conditions is flaky. Consider "
         "setting the suggested reclaim time to TimeDelta::Max() in Start().";
#endif

  WaitForWorkersIdleLockRequiredForTesting(n);
}

void SchedulerWorkerPoolImpl::WaitForAllWorkersIdleForTesting() {
  AutoSchedulerLock auto_lock(lock_);
  WaitForWorkersIdleLockRequiredForTesting(workers_.size());
}

void SchedulerWorkerPoolImpl::WaitForWorkersCleanedUpForTesting(size_t n) {
  AutoSchedulerLock auto_lock(lock_);

  if (!num_workers_cleaned_up_for_testing_cv_)
    num_workers_cleaned_up_for_testing_cv_ = lock_.CreateConditionVariable();

  while (num_workers_cleaned_up_for_testing_ < n)
    num_workers_cleaned_up_for_testing_cv_->Wait();

  num_workers_cleaned_up_for_testing_ = 0;
}

void SchedulerWorkerPoolImpl::JoinForTesting() {
#if DCHECK_IS_ON()
  join_for_testing_started_.Set();
#endif

  decltype(workers_) workers_copy;
  {
    AutoSchedulerLock auto_lock(lock_);
    priority_queue_.EnableFlushSequencesOnDestroyForTesting();

    DCHECK_GT(workers_.size(), size_t(0)) << "Joined an unstarted worker pool.";

    // Ensure SchedulerWorkers in |workers_| do not attempt to cleanup while
    // being joined.
    worker_cleanup_disallowed_for_testing_ = true;

    // Make a copy of the SchedulerWorkers so that we can call
    // SchedulerWorker::JoinForTesting() without holding |lock_| since
    // SchedulerWorkers may need to access |workers_|.
    workers_copy = workers_;
  }
  for (const auto& worker : workers_copy)
    worker->JoinForTesting();

  AutoSchedulerLock auto_lock(lock_);
  DCHECK(workers_ == workers_copy);
  // Release |workers_| to clear their TrackedRef against |this|.
  workers_.clear();
}

void SchedulerWorkerPoolImpl::ReEnqueueSequenceChangingPool(
    SequenceAndTransaction sequence_and_transaction) {
  PushSequenceToPriorityQueue(std::move(sequence_and_transaction));
  WakeUpOneWorker();
}

size_t SchedulerWorkerPoolImpl::NumberOfWorkersForTesting() const {
  AutoSchedulerLock auto_lock(lock_);
  return workers_.size();
}

size_t SchedulerWorkerPoolImpl::GetMaxTasksForTesting() const {
  AutoSchedulerLock auto_lock(lock_);
  return max_tasks_;
}

size_t SchedulerWorkerPoolImpl::NumberOfIdleWorkersForTesting() const {
  AutoSchedulerLock auto_lock(lock_);
  return idle_workers_stack_.Size();
}

void SchedulerWorkerPoolImpl::ReportHeartbeatMetrics() const {
  AutoSchedulerLock auto_lock(lock_);
  num_workers_histogram_->Add(workers_.size());

  num_active_workers_histogram_->Add(workers_.size() -
                                     idle_workers_stack_.Size());
}

void SchedulerWorkerPoolImpl::UpdateSortKey(
    SequenceAndTransaction sequence_and_transaction) {
  // TODO(fdoray): A worker should be woken up when the priority of a
  // BEST_EFFORT task is increased and |num_running_best_effort_tasks_| is
  // equal to |max_best_effort_tasks_|.
  AutoSchedulerLock auto_lock(lock_);
  priority_queue_.UpdateSortKey(std::move(sequence_and_transaction));
}

bool SchedulerWorkerPoolImpl::RemoveSequence(scoped_refptr<Sequence> sequence) {
  AutoSchedulerLock auto_lock(lock_);
  return priority_queue_.RemoveSequence(std::move(sequence));
}

SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    SchedulerWorkerDelegateImpl(TrackedRef<SchedulerWorkerPoolImpl> outer)
    : outer_(std::move(outer)) {
  // Bound in OnMainEntry().
  DETACH_FROM_THREAD(worker_thread_checker_);
}

// OnMainExit() handles the thread-affine cleanup; SchedulerWorkerDelegateImpl
// can thereafter safely be deleted from any thread.
SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    ~SchedulerWorkerDelegateImpl() = default;

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    OnCanScheduleSequence(scoped_refptr<Sequence> sequence) {
  outer_->OnCanScheduleSequence(std::move(sequence));
}

SchedulerWorker::ThreadLabel
SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::GetThreadLabel() const {
  return SchedulerWorker::ThreadLabel::POOLED;
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::OnMainEntry(
    const SchedulerWorker* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  {
#if DCHECK_IS_ON()
    AutoSchedulerLock auto_lock(outer_->lock_);
    DCHECK(ContainsWorker(outer_->workers_, worker));
#endif
  }

#if defined(OS_WIN)
  if (outer_->after_start().worker_environment == WorkerEnvironment::COM_MTA) {
    if (win::GetVersion() >= win::VERSION_WIN8) {
      worker_only().win_thread_environment =
          std::make_unique<win::ScopedWinrtInitializer>();
    } else {
      worker_only().win_thread_environment =
          std::make_unique<win::ScopedCOMInitializer>(
              win::ScopedCOMInitializer::kMTA);
    }
    DCHECK(worker_only().win_thread_environment->Succeeded());
  }
#endif  // defined(OS_WIN)

  DCHECK_EQ(worker_only().num_tasks_since_last_wait, 0U);

  PlatformThread::SetName(
      StringPrintf("TaskScheduler%sWorker", outer_->pool_label_.c_str()));

  outer_->BindToCurrentThread();
  SetBlockingObserverForCurrentThread(this);
}

scoped_refptr<Sequence>
SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::GetWork(
    SchedulerWorker* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(!worker_only().is_running_task);
  DCHECK(!read_worker().is_running_best_effort_task);

  SchedulerWorkerActionExecutor executor(outer_);
  AutoSchedulerLock auto_lock(outer_->lock_);

  DCHECK(ContainsWorker(outer_->workers_, worker));

  if (!CanGetWorkLockRequired(worker))
    return nullptr;

  if (outer_->priority_queue_.IsEmpty()) {
    OnWorkerBecomesIdleLockRequired(worker);
    return nullptr;
  }

  // Enforce that no more than |max_best_effort_tasks_| run concurrently.
  const TaskPriority priority =
      outer_->priority_queue_.PeekSortKey().priority();
  if (priority == TaskPriority::BEST_EFFORT) {
    if (outer_->num_running_best_effort_tasks_ <
        outer_->max_best_effort_tasks_) {
      ++outer_->num_running_best_effort_tasks_;
      write_worker().is_running_best_effort_task = true;
    } else {
      OnWorkerBecomesIdleLockRequired(worker);
      return nullptr;
    }
  }

  // Replace this worker if it was the last one, capacity permitting.
  outer_->MaintainAtLeastOneIdleWorkerLockRequired(&executor);

  DCHECK(!outer_->idle_workers_stack_.Contains(worker));
  worker_only().is_running_task = true;

  return outer_->priority_queue_.PopSequence();
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::DidRunTask(
    scoped_refptr<Sequence> sequence) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(worker_only().is_running_task);
  DCHECK(read_worker().may_block_start_time.is_null());
  // |may_block_start_time| being null unracily implies
  // |!incremented_max_tasks_since_blocked_|. Skip check on NaCl to avoid unsafe
  // reference acquisition warning.
#if !defined(OS_NACL)
  DCHECK(!TS_UNCHECKED_READ(incremented_max_tasks_since_blocked_));
#endif

  // Bookkeeping.
  worker_only().is_running_task = false;

  if (read_worker().is_running_best_effort_task) {
    AutoSchedulerLock auto_lock(outer_->lock_);
    --outer_->num_running_best_effort_tasks_;
    write_worker().is_running_best_effort_task = false;
  }

  ++worker_only().num_tasks_since_last_wait;
  ++worker_only().num_tasks_since_last_detach;

  if (!sequence)
    return;

  // A transaction to the Sequence to reenqueue.
  SequenceAndTransaction sequence_to_reenqueue_and_transaction(
      SequenceAndTransaction::FromSequence(std::move(sequence)));

  // Decide in which pool the Sequence in
  // |sequence_to_reenqueue_and_transaction| should be reenqueued.
  SchedulerWorkerPool* const destination_pool =
      outer_->delegate_->GetWorkerPoolForTraits(
          sequence_to_reenqueue_and_transaction.transaction.traits());

  // Reenqueue the Sequence.
  if (outer_ == destination_pool) {
    outer_->PushSequenceToPriorityQueue(
        std::move(sequence_to_reenqueue_and_transaction));
  } else {
    destination_pool->ReEnqueueSequenceChangingPool(
        std::move(sequence_to_reenqueue_and_transaction));
  }
}

TimeDelta
SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::GetSleepTimeout() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  // Sleep for an extra 10% to avoid the following pathological case:

  //   0) A task is running on a timer which matches
  //      |after_start().suggested_reclaim_time|.
  //   1) The timer fires and this worker is created by
  //      MaintainAtLeastOneIdleWorkerLockRequired() because the last idle
  //      worker was assigned the task.
  //   2) This worker begins sleeping |after_start().suggested_reclaim_time| (on
  //      top of the idle stack).
  //   3) The task assigned to the other worker completes and the worker goes
  //      back on the idle stack (this worker is now second on the idle stack;
  //      its GetLastUsedTime() is set to Now()).
  //   4) The sleep in (2) expires. Since (3) was fast this worker is likely to
  //      have been second on the idle stack long enough for
  //      CanCleanupLockRequired() to be satisfied in which case this worker is
  //      cleaned up.
  //   5) The timer fires at roughly the same time and we're back to (1) if (4)
  //      resulted in a clean up; causing thread churn.
  //
  //   Sleeping 10% longer in (2) makes it much less likely that (4) occurs
  //   before (5). In that case (5) will cause (3) and refresh this worker's
  //   GetLastUsedTime(), making CanCleanupLockRequired() return false in (4)
  //   and avoiding churn.
  //
  //   Of course the same problem arises if in (0) the timer matches
  //   |after_start().suggested_reclaim_time * 1.1| but it's expected that any
  //   timer slower than |after_start().suggested_reclaim_time| will cause such
  //   churn during long idle periods. If this is a problem in practice, the
  //   standby thread configuration and algorithm should be revisited.
  return outer_->after_start().suggested_reclaim_time * 1.1;
}

bool SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    CanCleanupLockRequired(const SchedulerWorker* worker) const {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  const TimeTicks last_used_time = worker->GetLastUsedTime();
  return !last_used_time.is_null() &&
         TimeTicks::Now() - last_used_time >=
             outer_->after_start().suggested_reclaim_time &&
         (outer_->workers_.size() > outer_->after_start().initial_max_tasks ||
          !FeatureList::IsEnabled(kNoDetachBelowInitialCapacity)) &&
         LIKELY(!outer_->worker_cleanup_disallowed_for_testing_);
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::CleanupLockRequired(
    SchedulerWorker* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  outer_->num_tasks_before_detach_histogram_->Add(
      worker_only().num_tasks_since_last_detach);
  outer_->cleanup_timestamps_.push(TimeTicks::Now());
  worker->Cleanup();
  outer_->idle_workers_stack_.Remove(worker);

  // Remove the worker from |workers_|.
  auto worker_iter =
      std::find(outer_->workers_.begin(), outer_->workers_.end(), worker);
  DCHECK(worker_iter != outer_->workers_.end());
  outer_->workers_.erase(worker_iter);

  ++outer_->num_workers_cleaned_up_for_testing_;
#if DCHECK_IS_ON()
  outer_->some_workers_cleaned_up_for_testing_ = true;
#endif
  if (outer_->num_workers_cleaned_up_for_testing_cv_)
    outer_->num_workers_cleaned_up_for_testing_cv_->Signal();
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    OnWorkerBecomesIdleLockRequired(SchedulerWorker* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  // Record the TaskScheduler.NumTasksBetweenWaits histogram. After GetWork()
  // returns nullptr, the SchedulerWorker will perform a wait on its
  // WaitableEvent, so we record how many tasks were ran since the last wait
  // here.
  outer_->num_tasks_between_waits_histogram_->Add(
      worker_only().num_tasks_since_last_wait);
  worker_only().num_tasks_since_last_wait = 0;

  // Add the worker to the idle stack.
  DCHECK(!outer_->idle_workers_stack_.Contains(worker));
  outer_->idle_workers_stack_.Push(worker);
  DCHECK_LE(outer_->idle_workers_stack_.Size(), outer_->workers_.size());
  outer_->idle_workers_stack_cv_for_testing_->Broadcast();
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::OnMainExit(
    SchedulerWorker* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

#if DCHECK_IS_ON()
  {
    bool shutdown_complete = outer_->task_tracker_->IsShutdownComplete();
    AutoSchedulerLock auto_lock(outer_->lock_);

    // |worker| should already have been removed from the idle workers stack and
    // |workers_| by the time the thread is about to exit. (except in the cases
    // where the pool is no longer going to be used - in which case, it's fine
    // for there to be invalid workers in the pool.
    if (!shutdown_complete && !outer_->join_for_testing_started_.IsSet()) {
      DCHECK(!outer_->idle_workers_stack_.Contains(worker));
      DCHECK(!ContainsWorker(outer_->workers_, worker));
    }
  }
#endif

#if defined(OS_WIN)
  worker_only().win_thread_environment.reset();
#endif  // defined(OS_WIN)
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::BlockingStarted(
    BlockingType blocking_type) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(worker_only().is_running_task);

  // MayBlock with no delay reuses WillBlock implementation.
  if (outer_->after_start().may_block_without_delay_)
    blocking_type = BlockingType::WILL_BLOCK;

  switch (blocking_type) {
    case BlockingType::MAY_BLOCK:
      MayBlockEntered();
      break;
    case BlockingType::WILL_BLOCK:
      WillBlockEntered();
      break;
  }
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    BlockingTypeUpgraded() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(worker_only().is_running_task);

  // The blocking type always being WILL_BLOCK in this experiment, it should
  // never be considered "upgraded".
  if (outer_->after_start().may_block_without_delay_)
    return;

  {
    AutoSchedulerLock auto_lock(outer_->lock_);

    // Don't do anything if a MAY_BLOCK ScopedBlockingCall instantiated in the
    // same scope already caused the max tasks to be incremented.
    if (incremented_max_tasks_since_blocked_)
      return;

    // Cancel the effect of a MAY_BLOCK ScopedBlockingCall instantiated in the
    // same scope.
    if (!read_worker().may_block_start_time.is_null()) {
      write_worker().may_block_start_time = TimeTicks();
      --outer_->num_pending_may_block_workers_;
      if (read_worker().is_running_best_effort_task)
        --outer_->num_pending_best_effort_may_block_workers_;
    }
  }

  WillBlockEntered();
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::BlockingEnded() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(worker_only().is_running_task);

  AutoSchedulerLock auto_lock(outer_->lock_);
  if (incremented_max_tasks_since_blocked_) {
    outer_->DecrementMaxTasksLockRequired(
        read_worker().is_running_best_effort_task);
  } else {
    DCHECK(!read_worker().may_block_start_time.is_null());
    --outer_->num_pending_may_block_workers_;
    if (read_worker().is_running_best_effort_task)
      --outer_->num_pending_best_effort_may_block_workers_;
  }

  incremented_max_tasks_since_blocked_ = false;
  write_worker().may_block_start_time = TimeTicks();
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::MayBlockEntered() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(worker_only().is_running_task);

  bool must_schedule_adjust_max_tasks = false;
  {
    AutoSchedulerLock auto_lock(outer_->lock_);

    DCHECK(!incremented_max_tasks_since_blocked_);
    DCHECK(read_worker().may_block_start_time.is_null());
    write_worker().may_block_start_time = TimeTicks::Now();
    ++outer_->num_pending_may_block_workers_;
    if (read_worker().is_running_best_effort_task)
      ++outer_->num_pending_best_effort_may_block_workers_;

    must_schedule_adjust_max_tasks =
        outer_->MustScheduleAdjustMaxTasksLockRequired();
  }
  if (must_schedule_adjust_max_tasks)
    outer_->ScheduleAdjustMaxTasks();
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::WillBlockEntered() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(worker_only().is_running_task);

  SchedulerWorkerActionExecutor executor(outer_);
  AutoSchedulerLock auto_lock(outer_->lock_);

  DCHECK(!incremented_max_tasks_since_blocked_);
  DCHECK(read_worker().may_block_start_time.is_null());
  incremented_max_tasks_since_blocked_ = true;
  outer_->IncrementMaxTasksLockRequired(
      read_worker().is_running_best_effort_task);

  // If the number of workers was less than the old max tasks, PostTask would've
  // handled creating extra workers during WakeUpOneWorker. Therefore, we don't
  // need to do anything here.
  if (outer_->workers_.size() < outer_->max_tasks_ - 1)
    return;

  if (outer_->priority_queue_.IsEmpty()) {
    outer_->MaintainAtLeastOneIdleWorkerLockRequired(&executor);
  } else {
    // TODO(crbug.com/757897): We may create extra workers in this case:
    // |workers.size()| was equal to the old |max_tasks_|, we had multiple
    // ScopedBlockingCalls in parallel and we had work on the PQ.
    outer_->WakeUpOneWorkerLockRequired(&executor);
  }

  // Entering a WILL_BLOCK ScopedBlockingCall is not expected to require
  // AdjustMaxTasks() to be scheduled.
  DCHECK(!outer_->MustScheduleAdjustMaxTasksLockRequired());
}

bool SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    CanGetWorkLockRequired(SchedulerWorker* worker) {
  // To avoid searching through the idle stack : use GetLastUsedTime() not being
  // null (or being directly on top of the idle stack) as a proxy for being on
  // the idle stack.
  const bool is_on_idle_workers_stack =
      outer_->idle_workers_stack_.Peek() == worker ||
      !worker->GetLastUsedTime().is_null();
  DCHECK_EQ(is_on_idle_workers_stack,
            outer_->idle_workers_stack_.Contains(worker));

  if (is_on_idle_workers_stack) {
    if (CanCleanupLockRequired(worker))
      CleanupLockRequired(worker);
    return false;
  }

  // Excess workers should not get work, until they are no longer excess (i.e.
  // max tasks increases or another worker cleans up). This ensures that if we
  // have excess workers in the pool, they get a chance to no longer be excess
  // before being cleaned up.
  if (outer_->NumberOfExcessWorkersLockRequired() >
      outer_->idle_workers_stack_.Size()) {
    OnWorkerBecomesIdleLockRequired(worker);
    return false;
  }

  return true;
}

bool SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    MustIncrementMaxTasksLockRequired() {
  if (!incremented_max_tasks_since_blocked_ &&
      !read_any().may_block_start_time.is_null() &&
      TimeTicks::Now() - read_any().may_block_start_time >=
          outer_->after_start().may_block_threshold) {
    incremented_max_tasks_since_blocked_ = true;

    --outer_->num_pending_may_block_workers_;
    if (read_any().is_running_best_effort_task)
      --outer_->num_pending_best_effort_may_block_workers_;

    return true;
  }

  return false;
}

void SchedulerWorkerPoolImpl::WaitForWorkersIdleLockRequiredForTesting(
    size_t n) {
  // Make sure workers do not cleanup while watching the idle count.
  AutoReset<bool> ban_cleanups(&worker_cleanup_disallowed_for_testing_, true);

  while (idle_workers_stack_.Size() < n)
    idle_workers_stack_cv_for_testing_->Wait();
}

void SchedulerWorkerPoolImpl::WakeUpOneWorkerLockRequired(
    SchedulerWorkerActionExecutor* executor) {
  if (workers_.empty()) {
    ++num_wake_ups_before_start_;
    return;
  }

  // Ensure that there is one worker that can run tasks on top of the idle
  // stack, capacity permitting.
  MaintainAtLeastOneIdleWorkerLockRequired(executor);

  // If the worker on top of the idle stack can run tasks, wake it up.
  if (NumberOfExcessWorkersLockRequired() < idle_workers_stack_.Size()) {
    SchedulerWorker* worker_to_wakeup = idle_workers_stack_.Pop();
    DCHECK(worker_to_wakeup);
    executor->ScheduleWakeUp(worker_to_wakeup);
  }
}

void SchedulerWorkerPoolImpl::WakeUpOneWorker() {
  bool must_schedule_adjust_max_tasks = false;
  SchedulerWorkerActionExecutor executor(tracked_ref_factory_.GetTrackedRef());
  {
    AutoSchedulerLock auto_lock(lock_);
    WakeUpOneWorkerLockRequired(&executor);
    must_schedule_adjust_max_tasks = MustScheduleAdjustMaxTasksLockRequired();
  }
  if (must_schedule_adjust_max_tasks)
    ScheduleAdjustMaxTasks();
}

void SchedulerWorkerPoolImpl::MaintainAtLeastOneIdleWorkerLockRequired(
    SchedulerWorkerActionExecutor* executor) {
  if (workers_.size() == kMaxNumberOfWorkers)
    return;
  DCHECK_LT(workers_.size(), kMaxNumberOfWorkers);

  if (!idle_workers_stack_.IsEmpty())
    return;

  if (workers_.size() >= max_tasks_)
    return;

  scoped_refptr<SchedulerWorker> new_worker =
      CreateAndRegisterWorkerLockRequired(executor);
  DCHECK(new_worker);
  idle_workers_stack_.Push(new_worker.get());
}

scoped_refptr<SchedulerWorker>
SchedulerWorkerPoolImpl::CreateAndRegisterWorkerLockRequired(
    SchedulerWorkerActionExecutor* executor) {
  DCHECK_LT(workers_.size(), max_tasks_);
  DCHECK_LT(workers_.size(), kMaxNumberOfWorkers);
  // SchedulerWorker needs |lock_| as a predecessor for its thread lock
  // because in WakeUpOneWorker, |lock_| is first acquired and then
  // the thread lock is acquired when WakeUp is called on the worker.
  scoped_refptr<SchedulerWorker> worker = MakeRefCounted<SchedulerWorker>(
      priority_hint_,
      std::make_unique<SchedulerWorkerDelegateImpl>(
          tracked_ref_factory_.GetTrackedRef()),
      task_tracker_, &lock_, after_start().backward_compatibility);

  workers_.push_back(worker);
  executor->ScheduleStart(worker);
  DCHECK_LE(workers_.size(), max_tasks_);

  if (!cleanup_timestamps_.empty()) {
    detach_duration_histogram_->AddTime(TimeTicks::Now() -
                                        cleanup_timestamps_.top());
    cleanup_timestamps_.pop();
  }

  return worker;
}

size_t SchedulerWorkerPoolImpl::NumberOfExcessWorkersLockRequired() const {
  return std::max<int>(0, workers_.size() - max_tasks_);
}

void SchedulerWorkerPoolImpl::AdjustMaxTasks() {
  DCHECK(
      after_start().service_thread_task_runner->RunsTasksInCurrentSequence());

  SchedulerWorkerActionExecutor executor(tracked_ref_factory_.GetTrackedRef());
  AutoSchedulerLock auto_lock(lock_);

  const size_t previous_max_tasks = max_tasks_;

  // Increment max tasks for each worker that has been within a MAY_BLOCK
  // ScopedBlockingCall for more than may_block_threshold.
  for (scoped_refptr<SchedulerWorker> worker : workers_) {
    // The delegates of workers inside a SchedulerWorkerPoolImpl should be
    // SchedulerWorkerDelegateImpls.
    SchedulerWorkerDelegateImpl* delegate =
        static_cast<SchedulerWorkerDelegateImpl*>(worker->delegate());
    AnnotateAcquiredLockAlias annotate(lock_, delegate->lock());
    if (delegate->MustIncrementMaxTasksLockRequired()) {
      IncrementMaxTasksLockRequired(
          delegate->is_running_best_effort_task_lock_required());
    }
  }

  // Wake up a worker per pending sequence, capacity permitting.
  const size_t num_pending_sequences = priority_queue_.Size();
  const size_t num_wake_ups_needed =
      std::min(max_tasks_ - previous_max_tasks, num_pending_sequences);

  for (size_t i = 0; i < num_wake_ups_needed; ++i) {
    // No need to call ScheduleAdjustMaxTasks() as the caller will
    // take care of that for us.
    WakeUpOneWorkerLockRequired(&executor);
  }

  MaintainAtLeastOneIdleWorkerLockRequired(&executor);
}

void SchedulerWorkerPoolImpl::ScheduleAdjustMaxTasks() {
  // |polling_max_tasks_| can't change before the task posted below runs. Skip
  // check on NaCl to avoid unsafe reference acquisition warning.
#if !defined(OS_NACL)
  DCHECK(TS_UNCHECKED_READ(polling_max_tasks_));
#endif

  after_start().service_thread_task_runner->PostDelayedTask(
      FROM_HERE,
      BindOnce(&SchedulerWorkerPoolImpl::AdjustMaxTasksFunction,
               Unretained(this)),
      after_start().blocked_workers_poll_period);
}

bool SchedulerWorkerPoolImpl::MustScheduleAdjustMaxTasksLockRequired() {
  if (polling_max_tasks_ || !ShouldPeriodicallyAdjustMaxTasksLockRequired())
    return false;
  polling_max_tasks_ = true;
  return true;
}

void SchedulerWorkerPoolImpl::AdjustMaxTasksFunction() {
  DCHECK(
      after_start().service_thread_task_runner->RunsTasksInCurrentSequence());

  AdjustMaxTasks();
  {
    AutoSchedulerLock auto_lock(lock_);
    DCHECK(polling_max_tasks_);

    if (!ShouldPeriodicallyAdjustMaxTasksLockRequired()) {
      polling_max_tasks_ = false;
      return;
    }
  }
  ScheduleAdjustMaxTasks();
}

bool SchedulerWorkerPoolImpl::ShouldPeriodicallyAdjustMaxTasksLockRequired() {
  // The maximum number of best-effort tasks that can run concurrently must be
  // adjusted periodically when (1) the number of best-effort tasks that are
  // currently running is equal to it and (2) there are workers running
  // best-effort tasks within the scope of a MAY_BLOCK ScopedBlockingCall but
  // haven't cause a max best-effort tasks increment yet.
  // - When (1) is false: A newly posted best-effort task will be allowed to run
  //   normally. There is no hurry to increase max best-effort tasks.
  // - When (2) is false: AdjustMaxTasks() wouldn't affect
  //   |max_best_effort_tasks_|.
  if (num_running_best_effort_tasks_ >= max_best_effort_tasks_ &&
      num_pending_best_effort_may_block_workers_ > 0) {
    return true;
  }

  // The maximum number of tasks that can run concurrently must be adjusted
  // periodically when (1) there are no idle workers that can do work (2) there
  // are workers that are within the scope of a MAY_BLOCK ScopedBlockingCall but
  // haven't cause a max tasks increment yet.
  // - When (1) is false: A newly posted task will run on one of the idle
  //   workers that are allowed to do work. There is no hurry to increase max
  //   tasks.
  // - When (2) is false: AdjustMaxTasks() wouldn't affect |max_tasks_|.
  const int idle_workers_that_can_do_work =
      idle_workers_stack_.Size() - NumberOfExcessWorkersLockRequired();
  return idle_workers_that_can_do_work <= 0 &&
         num_pending_may_block_workers_ > 0;
}

void SchedulerWorkerPoolImpl::DecrementMaxTasksLockRequired(
    bool is_running_best_effort_task) {
  --max_tasks_;
  if (is_running_best_effort_task)
    --max_best_effort_tasks_;
}

void SchedulerWorkerPoolImpl::IncrementMaxTasksLockRequired(
    bool is_running_best_effort_task) {
  ++max_tasks_;
  if (is_running_best_effort_task)
    ++max_best_effort_tasks_;
}

SchedulerWorkerPoolImpl::InitializedInStart::InitializedInStart() = default;
SchedulerWorkerPoolImpl::InitializedInStart::~InitializedInStart() = default;

}  // namespace internal
}  // namespace base
