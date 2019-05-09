// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_pool_impl.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task/task_features.h"
#include "base/task/thread_pool/pooled_parallel_task_runner.h"
#include "base/task/thread_pool/pooled_sequenced_task_runner.h"
#include "base/task/thread_pool/sequence_sort_key.h"
#include "base/task/thread_pool/service_thread.h"
#include "base/task/thread_pool/task.h"
#include "base/task/thread_pool/task_source.h"
#include "base/task/thread_pool/thread_group_impl.h"
#include "base/task/thread_pool/thread_group_params.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

#if defined(OS_WIN)
#include "base/task/thread_pool/thread_group_native_win.h"
#endif

#if defined(OS_MACOSX)
#include "base/task/thread_pool/thread_group_native_mac.h"
#endif

namespace base {
namespace internal {

namespace {

constexpr EnvironmentParams kForegroundPoolEnvironmentParams{
    "Foreground", base::ThreadPriority::NORMAL};

constexpr EnvironmentParams kBackgroundPoolEnvironmentParams{
    "Background", base::ThreadPriority::BACKGROUND};

// Indicates whether BEST_EFFORT tasks are disabled by a command line switch.
bool HasDisableBestEffortTasksSwitch() {
  // The CommandLine might not be initialized if TaskScheduler is initialized
  // in a dynamic library which doesn't have access to argc/argv.
  return CommandLine::InitializedForCurrentProcess() &&
         CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableBestEffortTasks);
}

}  // namespace

ThreadPoolImpl::ThreadPoolImpl(StringPiece histogram_label)
    : ThreadPoolImpl(histogram_label,
                     std::make_unique<TaskTrackerImpl>(histogram_label)) {}

ThreadPoolImpl::ThreadPoolImpl(StringPiece histogram_label,
                               std::unique_ptr<TaskTrackerImpl> task_tracker)
    : task_tracker_(std::move(task_tracker)),
      service_thread_(std::make_unique<ServiceThread>(
          task_tracker_.get(),
          BindRepeating(&ThreadPoolImpl::ReportHeartbeatMetrics,
                        Unretained(this)))),
      single_thread_task_runner_manager_(task_tracker_->GetTrackedRef(),
                                         &delayed_task_manager_),
      can_run_best_effort_(!HasDisableBestEffortTasksSwitch()),
      tracked_ref_factory_(this) {
  DCHECK(!histogram_label.empty());

  foreground_thread_group_ = std::make_unique<ThreadGroupImpl>(
      JoinString(
          {histogram_label, kForegroundPoolEnvironmentParams.name_suffix}, "."),
      kForegroundPoolEnvironmentParams.name_suffix,
      kForegroundPoolEnvironmentParams.priority_hint,
      task_tracker_->GetTrackedRef(), tracked_ref_factory_.GetTrackedRef());

  if (CanUseBackgroundPriorityForWorkerThread()) {
    background_thread_group_ = std::make_unique<ThreadGroupImpl>(
        JoinString(
            {histogram_label, kBackgroundPoolEnvironmentParams.name_suffix},
            "."),
        kBackgroundPoolEnvironmentParams.name_suffix,
        kBackgroundPoolEnvironmentParams.priority_hint,
        task_tracker_->GetTrackedRef(), tracked_ref_factory_.GetTrackedRef());
  }
}

ThreadPoolImpl::~ThreadPoolImpl() {
#if DCHECK_IS_ON()
  DCHECK(join_for_testing_returned_.IsSet());
#endif

  // Reset thread groups to release held TrackedRefs, which block teardown.
  foreground_thread_group_.reset();
  background_thread_group_.reset();
}

void ThreadPoolImpl::Start(const ThreadPool::InitParams& init_params,
                           WorkerThreadObserver* worker_thread_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!started_);

  internal::InitializeThreadPrioritiesFeature();

  // This is set in Start() and not in the constructor because variation params
  // are usually not ready when ThreadPoolImpl is instantiated in a process.
  if (FeatureList::IsEnabled(kAllTasksUserBlocking))
    all_tasks_user_blocking_.Set();

#if defined(OS_WIN) || defined(OS_MACOSX)
  if (FeatureList::IsEnabled(kUseNativeThreadPool)) {
    std::unique_ptr<ThreadGroup> pool = std::move(foreground_thread_group_);
    foreground_thread_group_ = std::make_unique<ThreadGroupNativeImpl>(
        task_tracker_->GetTrackedRef(), tracked_ref_factory_.GetTrackedRef(),
        pool.get());
    pool->InvalidateAndHandoffAllTaskSourcesToOtherThreadGroup(
        foreground_thread_group_.get());
  }
#endif

  // Start the service thread. On platforms that support it (POSIX except NaCL
  // SFI), the service thread runs a MessageLoopForIO which is used to support
  // FileDescriptorWatcher in the scope in which tasks run.
  ServiceThread::Options service_thread_options;
  service_thread_options.message_loop_type =
#if defined(OS_POSIX) && !defined(OS_NACL_SFI)
      MessageLoop::TYPE_IO;
#else
      MessageLoop::TYPE_DEFAULT;
#endif
  service_thread_options.timer_slack = TIMER_SLACK_MAXIMUM;
  CHECK(service_thread_->StartWithOptions(service_thread_options));

#if defined(OS_POSIX) && !defined(OS_NACL_SFI)
  // Needs to happen after starting the service thread to get its
  // task_runner().
  task_tracker_->set_io_thread_task_runner(service_thread_->task_runner());
#endif  // defined(OS_POSIX) && !defined(OS_NACL_SFI)

  // Needs to happen after starting the service thread to get its task_runner().
  scoped_refptr<TaskRunner> service_thread_task_runner =
      service_thread_->task_runner();
  delayed_task_manager_.Start(service_thread_task_runner);

  single_thread_task_runner_manager_.Start(worker_thread_observer);

  const ThreadGroup::WorkerEnvironment worker_environment =
#if defined(OS_WIN)
      init_params.common_thread_pool_environment ==
              InitParams::CommonThreadPoolEnvironment::COM_MTA
          ? ThreadGroup::WorkerEnvironment::COM_MTA
          : ThreadGroup::WorkerEnvironment::NONE;
#else
      ThreadGroup::WorkerEnvironment::NONE;
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
  if (FeatureList::IsEnabled(kUseNativeThreadPool)) {
    static_cast<ThreadGroupNative*>(foreground_thread_group_.get())
        ->Start(worker_environment);
  } else
#endif
  {
    // On platforms that can't use the background thread priority, best-effort
    // tasks run in foreground pools. A cap is set on the number of background
    // tasks that can run in foreground pools to ensure that there is always
    // room for incoming foreground tasks and to minimize the performance impact
    // of best-effort tasks.

    const int max_best_effort_tasks_in_foreground_thread_group = std::max(
        1,
        std::min(init_params.background_thread_group_params.max_tasks(),
                 init_params.foreground_thread_group_params.max_tasks() / 2));
    static_cast<ThreadGroupImpl*>(foreground_thread_group_.get())
        ->Start(init_params.foreground_thread_group_params,
                max_best_effort_tasks_in_foreground_thread_group,
                service_thread_task_runner, worker_thread_observer,
                worker_environment);
  }

  if (background_thread_group_) {
    background_thread_group_->Start(
        init_params.background_thread_group_params,
        init_params.background_thread_group_params.max_tasks(),
        service_thread_task_runner, worker_thread_observer, worker_environment);
  }

  started_ = true;
}

bool ThreadPoolImpl::PostDelayedTaskWithTraits(const Location& from_here,
                                               const TaskTraits& traits,
                                               OnceClosure task,
                                               TimeDelta delay) {
  // Post |task| as part of a one-off single-task Sequence.
  const TaskTraits new_traits = SetUserBlockingPriorityIfNeeded(traits);
  return PostTaskWithSequence(
      Task(from_here, std::move(task), delay),
      MakeRefCounted<Sequence>(new_traits, nullptr,
                               TaskSourceExecutionMode::kParallel));
}

scoped_refptr<TaskRunner> ThreadPoolImpl::CreateTaskRunnerWithTraits(
    const TaskTraits& traits) {
  const TaskTraits new_traits = SetUserBlockingPriorityIfNeeded(traits);
  return MakeRefCounted<PooledParallelTaskRunner>(new_traits, this);
}

scoped_refptr<SequencedTaskRunner>
ThreadPoolImpl::CreateSequencedTaskRunnerWithTraits(const TaskTraits& traits) {
  const TaskTraits new_traits = SetUserBlockingPriorityIfNeeded(traits);
  return MakeRefCounted<PooledSequencedTaskRunner>(new_traits, this);
}

scoped_refptr<SingleThreadTaskRunner>
ThreadPoolImpl::CreateSingleThreadTaskRunnerWithTraits(
    const TaskTraits& traits,
    SingleThreadTaskRunnerThreadMode thread_mode) {
  return single_thread_task_runner_manager_
      .CreateSingleThreadTaskRunnerWithTraits(
          SetUserBlockingPriorityIfNeeded(traits), thread_mode);
}

#if defined(OS_WIN)
scoped_refptr<SingleThreadTaskRunner>
ThreadPoolImpl::CreateCOMSTATaskRunnerWithTraits(
    const TaskTraits& traits,
    SingleThreadTaskRunnerThreadMode thread_mode) {
  return single_thread_task_runner_manager_.CreateCOMSTATaskRunnerWithTraits(
      SetUserBlockingPriorityIfNeeded(traits), thread_mode);
}
#endif  // defined(OS_WIN)

scoped_refptr<UpdateableSequencedTaskRunner>
ThreadPoolImpl::CreateUpdateableSequencedTaskRunnerWithTraitsForTesting(
    const TaskTraits& traits) {
  const TaskTraits new_traits = SetUserBlockingPriorityIfNeeded(traits);
  return MakeRefCounted<PooledSequencedTaskRunner>(new_traits, this);
}

int ThreadPoolImpl::GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
    const TaskTraits& traits) const {
  // This method does not support getting the maximum number of BEST_EFFORT
  // tasks that can run concurrently in a pool.
  DCHECK_NE(traits.priority(), TaskPriority::BEST_EFFORT);
  return GetThreadGroupForTraits(traits)
      ->GetMaxConcurrentNonBlockedTasksDeprecated();
}

void ThreadPoolImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  task_tracker_->StartShutdown();

  // Allow all tasks to run. Done after initiating shutdown to ensure that non-
  // BLOCK_SHUTDOWN tasks don't get a chance to run and that BLOCK_SHUTDOWN
  // tasks run with a normal thread priority.
  can_run_ = true;
  can_run_best_effort_ = true;
  UpdateCanRunPolicy();

  task_tracker_->CompleteShutdown();
}

void ThreadPoolImpl::FlushForTesting() {
  task_tracker_->FlushForTesting();
}

void ThreadPoolImpl::FlushAsyncForTesting(OnceClosure flush_callback) {
  task_tracker_->FlushAsyncForTesting(std::move(flush_callback));
}

void ThreadPoolImpl::JoinForTesting() {
#if DCHECK_IS_ON()
  DCHECK(!join_for_testing_returned_.IsSet());
#endif
  // The service thread must be stopped before the workers are joined, otherwise
  // tasks scheduled by the DelayedTaskManager might be posted between joining
  // those workers and stopping the service thread which will cause a CHECK. See
  // https://crbug.com/771701.
  service_thread_->Stop();
  single_thread_task_runner_manager_.JoinForTesting();
  foreground_thread_group_->JoinForTesting();
  if (background_thread_group_)
    background_thread_group_->JoinForTesting();
#if DCHECK_IS_ON()
  join_for_testing_returned_.Set();
#endif
}

void ThreadPoolImpl::SetCanRun(bool can_run) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(can_run_, can_run);
  can_run_ = can_run;
  UpdateCanRunPolicy();
}

void ThreadPoolImpl::SetCanRunBestEffort(bool can_run_best_effort) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(can_run_best_effort_, can_run_best_effort);
  can_run_best_effort_ = can_run_best_effort;
  UpdateCanRunPolicy();
}

bool ThreadPoolImpl::PostTaskWithSequenceNow(Task task,
                                             scoped_refptr<Sequence> sequence) {
  auto transaction = sequence->BeginTransaction();
  const bool sequence_should_be_queued = transaction.WillPushTask();
  if (sequence_should_be_queued &&
      !task_tracker_->WillQueueTaskSource(sequence.get()))
    return false;
  transaction.PushTask(std::move(task));
  if (sequence_should_be_queued) {
    const TaskTraits traits = transaction.traits();
    GetThreadGroupForTraits(traits)->PushTaskSourceAndWakeUpWorkers(
        {std::move(sequence), std::move(transaction)});
  }
  return true;
}

bool ThreadPoolImpl::PostTaskWithSequence(Task task,
                                          scoped_refptr<Sequence> sequence) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.task);
  DCHECK(sequence);

  if (!task_tracker_->WillPostTask(&task, sequence->shutdown_behavior()))
    return false;

  if (task.delayed_run_time.is_null()) {
    return PostTaskWithSequenceNow(std::move(task), std::move(sequence));
  } else {
    // It's safe to take a ref on this pointer since the caller must have a ref
    // to the TaskRunner in order to post.
    scoped_refptr<TaskRunner> task_runner = sequence->task_runner();
    delayed_task_manager_.AddDelayedTask(
        std::move(task),
        BindOnce(
            [](scoped_refptr<Sequence> sequence,
               ThreadPoolImpl* thread_pool_impl, Task task) {
              thread_pool_impl->PostTaskWithSequenceNow(std::move(task),
                                                        std::move(sequence));
            },
            std::move(sequence), Unretained(this)),
        std::move(task_runner));
  }

  return true;
}

bool ThreadPoolImpl::IsRunningPoolWithTraits(const TaskTraits& traits) const {
  return GetThreadGroupForTraits(traits)->IsBoundToCurrentThread();
}

void ThreadPoolImpl::UpdatePriority(scoped_refptr<TaskSource> task_source,
                                    TaskPriority priority) {
  auto task_source_and_transaction =
      TaskSourceAndTransaction::FromTaskSource(std::move(task_source));

  ThreadGroup* const current_thread_group =
      GetThreadGroupForTraits(task_source_and_transaction.transaction.traits());
  task_source_and_transaction.transaction.UpdatePriority(priority);
  ThreadGroup* const new_thread_group =
      GetThreadGroupForTraits(task_source_and_transaction.transaction.traits());

  if (new_thread_group == current_thread_group) {
    // |task_source|'s position needs to be updated within its current thread
    // group.
    current_thread_group->UpdateSortKey(std::move(task_source_and_transaction));
  } else {
    // |task_source| is changing thread groups; remove it from its current
    // thread group and reenqueue it.
    const bool task_source_was_found = current_thread_group->RemoveTaskSource(
        task_source_and_transaction.task_source);
    if (task_source_was_found) {
      DCHECK(task_source_and_transaction.task_source);
      new_thread_group->PushTaskSourceAndWakeUpWorkers(
          std::move(task_source_and_transaction));
    }
  }
}

const ThreadGroup* ThreadPoolImpl::GetThreadGroupForTraits(
    const TaskTraits& traits) const {
  return const_cast<ThreadPoolImpl*>(this)->GetThreadGroupForTraits(traits);
}

ThreadGroup* ThreadPoolImpl::GetThreadGroupForTraits(const TaskTraits& traits) {
  if (traits.priority() == TaskPriority::BEST_EFFORT &&
      background_thread_group_) {
    return background_thread_group_.get();
  }

  return foreground_thread_group_.get();
}

void ThreadPoolImpl::UpdateCanRunPolicy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const CanRunPolicy can_run_policy =
      can_run_ ? (can_run_best_effort_ ? CanRunPolicy::kAll
                                       : CanRunPolicy::kForegroundOnly)
               : CanRunPolicy::kNone;
  task_tracker_->SetCanRunPolicy(can_run_policy);
  foreground_thread_group_->DidUpdateCanRunPolicy();
  if (background_thread_group_)
    background_thread_group_->DidUpdateCanRunPolicy();
  single_thread_task_runner_manager_.DidUpdateCanRunPolicy();
}

TaskTraits ThreadPoolImpl::SetUserBlockingPriorityIfNeeded(
    TaskTraits traits) const {
  if (all_tasks_user_blocking_.IsSet())
    traits.UpdatePriority(TaskPriority::USER_BLOCKING);
  return traits;
}

void ThreadPoolImpl::ReportHeartbeatMetrics() const {
  foreground_thread_group_->ReportHeartbeatMetrics();
  if (background_thread_group_)
    background_thread_group_->ReportHeartbeatMetrics();
}

}  // namespace internal
}  // namespace base
