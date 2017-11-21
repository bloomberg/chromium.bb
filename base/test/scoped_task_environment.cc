// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_task_environment.h"

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/task_scheduler/task_scheduler_impl.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"

namespace base {
namespace test {

class ScopedTaskEnvironment::TestTaskTracker
    : public internal::TaskSchedulerImpl::TaskTrackerImpl {
 public:
  TestTaskTracker();

  // Allow running tasks.
  void AllowRunTasks();

  // Disallow running tasks. No-ops and returns false if a task is running.
  bool DisallowRunTasks();

 private:
  friend class ScopedTaskEnvironment;

  // internal::TaskSchedulerImpl::TaskTrackerImpl:
  void RunOrSkipTask(std::unique_ptr<internal::Task> task,
                     internal::Sequence* sequence,
                     bool can_run_task) override;

  // Synchronizes accesses to members below.
  Lock lock_;

  // True if running tasks is allowed.
  bool can_run_tasks_ = true;

  // Signaled when |can_run_tasks_| becomes true.
  ConditionVariable can_run_tasks_cv_;

  // Number of tasks that are currently running.
  int num_tasks_running_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestTaskTracker);
};

ScopedTaskEnvironment::ScopedTaskEnvironment(
    MainThreadType main_thread_type,
    ExecutionMode execution_control_mode)
    : execution_control_mode_(execution_control_mode),
      message_loop_(main_thread_type == MainThreadType::MOCK_TIME
                        ? nullptr
                        : (std::make_unique<MessageLoop>(
                              main_thread_type == MainThreadType::DEFAULT
                                  ? MessageLoop::TYPE_DEFAULT
                                  : (main_thread_type == MainThreadType::UI
                                         ? MessageLoop::TYPE_UI
                                         : MessageLoop::TYPE_IO)))),
      mock_time_task_runner_(
          main_thread_type == MainThreadType::MOCK_TIME
              ? MakeRefCounted<TestMockTimeTaskRunner>(
                    TestMockTimeTaskRunner::Type::kBoundToThread)
              : nullptr),
      task_tracker_(new TestTaskTracker()) {
  CHECK(!TaskScheduler::GetInstance());

  // Instantiate a TaskScheduler with 2 threads in each of its 4 pools. Threads
  // stay alive even when they don't have work.
  // Each pool uses two threads to prevent deadlocks in unit tests that have a
  // sequence that uses WithBaseSyncPrimitives() to wait on the result of
  // another sequence. This isn't perfect (doesn't solve wait chains) but solves
  // the basic use case for now.
  // TODO(fdoray/jeffreyhe): Make the TaskScheduler dynamically replace blocked
  // threads and get rid of this limitation. http://crbug.com/738104
  constexpr int kMaxThreads = 2;
  const TimeDelta kSuggestedReclaimTime = TimeDelta::Max();
  const SchedulerWorkerPoolParams worker_pool_params(kMaxThreads,
                                                     kSuggestedReclaimTime);
  TaskScheduler::SetInstance(std::make_unique<internal::TaskSchedulerImpl>(
      "ScopedTaskEnvironment", WrapUnique(task_tracker_)));
  task_scheduler_ = TaskScheduler::GetInstance();
  TaskScheduler::GetInstance()->Start({worker_pool_params, worker_pool_params,
                                       worker_pool_params, worker_pool_params});

  if (execution_control_mode_ == ExecutionMode::QUEUED)
    CHECK(task_tracker_->DisallowRunTasks());
}

ScopedTaskEnvironment::~ScopedTaskEnvironment() {
  // Ideally this would RunLoop().RunUntilIdle() here to catch any errors or
  // infinite post loop in the remaining work but this isn't possible right now
  // because base::~MessageLoop() didn't use to do this and adding it here would
  // make the migration away from MessageLoop that much harder.
  CHECK_EQ(TaskScheduler::GetInstance(), task_scheduler_);
  // Without FlushForTesting(), DeleteSoon() and ReleaseSoon() tasks could be
  // skipped, resulting in memory leaks.
  task_tracker_->AllowRunTasks();
  TaskScheduler::GetInstance()->FlushForTesting();
  TaskScheduler::GetInstance()->Shutdown();
  TaskScheduler::GetInstance()->JoinForTesting();
  TaskScheduler::SetInstance(nullptr);
}

scoped_refptr<base::SingleThreadTaskRunner>
ScopedTaskEnvironment::GetMainThreadTaskRunner() {
  if (message_loop_)
    return message_loop_->task_runner();
  DCHECK(mock_time_task_runner_);
  return mock_time_task_runner_;
}

void ScopedTaskEnvironment::RunUntilIdle() {
  for (int i = 0;; ++i) {
    LOG_IF(WARNING, i % 1000 == 999)
        << "ScopedTaskEnvironment::RunUntilIdle() appears to be stuck in an "
           "infinite loop (e.g. A posts B posts C posts A?).";

    task_tracker_->AllowRunTasks();

    // Another pass is only ever required when there is work remaining in the
    // TaskScheduler (see logic below), yield the main thread to avoid it
    // entering a DisallowRunTasks()/AllowRunTasks() busy loop that merely
    // confirms TaskScheduler has work to do while preventing its execution.
    if (i > 0)
      PlatformThread::YieldCurrentThread();

    // First run as many tasks as possible on the main thread in parallel with
    // tasks in TaskScheduler. This increases likelihood of TSAN catching
    // threading errors and eliminates possibility of hangs should a
    // TaskScheduler task synchronously block on a main thread task
    // (TaskScheduler::FlushForTesting() can't be used here for that reason).
    RunLoop().RunUntilIdle();

    // TODO(gab): The complex piece of logic below can be boiled down to "loop
    // again if >0 incomplete tasks" when TaskTracker is made aware of main
    // thread tasks. https://crbug.com/660078.

    // Then halt TaskScheduler. DisallowRunTasks() failing indicates that there
    // are TaskScheduler tasks currently running, yield to them and try again
    // (restarting from top as they may have posted main thread tasks).
    if (!task_tracker_->DisallowRunTasks())
      continue;

    // Once TaskScheduler is halted. Run any remaining main thread tasks (which
    // may have been posted by TaskScheduler tasks that completed between the
    // above main thread RunUntilIdle() and TaskScheduler DisallowRunTasks()).
    // Note: this assumes that no main thread task synchronously blocks on a
    // TaskScheduler tasks (it certainly shouldn't); this call could otherwise
    // hang.
    RunLoop().RunUntilIdle();

    // The above RunUntilIdle() guarantees there are no remaining main thread
    // tasks (the TaskScheduler being halted during the last RunUntilIdle() is
    // key as it prevents a task being posted to it racily with it determining
    // it had no work remaining). Therefore, we're done if there is no more work
    // on TaskScheduler either (there can be TaskScheduler work remaining if
    // DisallowRunTasks() preempted work and/or the last RunUntilIdle() posted
    // more TaskScheduler tasks).
    // Note: this last |if| couldn't be turned into a |do {} while();|
    // (regardless of the use of a for-loop for |i|). A conditional loop makes
    // it such that |continue;| results in checking the condition (not
    // unconditionally loop again) which would be incorrect for the above logic
    // as it'd then be possible for a TaskScheduler task to be running during
    // the DisallowRunTasks() test, causing it to fail, but then post to the
    // main thread and complete before the loop's condition is verified which
    // could result in GetNumIncompleteUndelayedTasksForTesting() returning 0
    // and the loop erroneously exiting with a pending task on the main thread.
    if (task_tracker_->GetNumIncompleteUndelayedTasksForTesting() == 0)
      break;
  }

  // The above loop always ends with running tasks being disallowed. Re-enable
  // parallel execution before returning unless in ExecutionMode::QUEUED.
  if (execution_control_mode_ != ExecutionMode::QUEUED)
    task_tracker_->AllowRunTasks();
}

void ScopedTaskEnvironment::FastForwardBy(TimeDelta delta) {
  DCHECK(mock_time_task_runner_);
  mock_time_task_runner_->FastForwardBy(delta);
}

void ScopedTaskEnvironment::FastForwardUntilNoTasksRemain() {
  DCHECK(mock_time_task_runner_);
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

ScopedTaskEnvironment::TestTaskTracker::TestTaskTracker()
    : can_run_tasks_cv_(&lock_) {}

void ScopedTaskEnvironment::TestTaskTracker::AllowRunTasks() {
  AutoLock auto_lock(lock_);
  can_run_tasks_ = true;
  can_run_tasks_cv_.Broadcast();
}

bool ScopedTaskEnvironment::TestTaskTracker::DisallowRunTasks() {
  AutoLock auto_lock(lock_);

  // Can't disallow run task if there are tasks running.
  if (num_tasks_running_ > 0)
    return false;

  can_run_tasks_ = false;
  return true;
}

void ScopedTaskEnvironment::TestTaskTracker::RunOrSkipTask(
    std::unique_ptr<internal::Task> task,
    internal::Sequence* sequence,
    bool can_run_task) {
  {
    AutoLock auto_lock(lock_);

    while (!can_run_tasks_)
      can_run_tasks_cv_.Wait();

    ++num_tasks_running_;
  }

  internal::TaskSchedulerImpl::TaskTrackerImpl::RunOrSkipTask(
      std::move(task), sequence, can_run_task);

  {
    AutoLock auto_lock(lock_);

    CHECK_GT(num_tasks_running_, 0);
    CHECK(can_run_tasks_);

    --num_tasks_running_;
  }
}

}  // namespace test
}  // namespace base
