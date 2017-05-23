// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_task_environment.h"

#include "base/run_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/task_scheduler/task_scheduler_impl.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"

namespace base {
namespace test {

namespace {

class TaskObserver : public MessageLoop::TaskObserver {
 public:
  TaskObserver() = default;

  // MessageLoop::TaskObserver:
  void WillProcessTask(const PendingTask& pending_task) override {}
  void DidProcessTask(const PendingTask& pending_task) override {
    ++task_count_;
  }

  int task_count() const { return task_count_; }

 private:
  int task_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TaskObserver);
};

}  // namespace

class ScopedTaskEnvironment::TestTaskTracker
    : public internal::TaskSchedulerImpl::TaskTrackerType {
 public:
  TestTaskTracker(ScopedTaskEnvironment* outer);

  void SetTaskQueueEmptyClosure(OnceClosure task_queue_empty_closure);

  // Allow running tasks.
  void AllowRunRask();

  // Disallow running tasks. No-ops and returns false if a task is running.
  bool DisallowRunTasks();

 private:
  friend class ScopedTaskEnvironment;

  // internal::TaskSchedulerImpl::TaskTrackerType:
  void PerformRunTask(std::unique_ptr<internal::Task> task,
                      const SequenceToken& sequence_token) override;

  ScopedTaskEnvironment* const outer_;

  // Synchronizes accesses to members below.
  Lock lock_;

  // Closure posted to the main thread when the task queue becomes empty.
  OnceClosure task_queue_empty_closure_;

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
    ExecutionControlMode execution_control_mode)
    : execution_control_mode_(execution_control_mode),
      message_loop_(main_thread_type == MainThreadType::DEFAULT
                        ? MessageLoop::TYPE_DEFAULT
                        : (main_thread_type == MainThreadType::UI
                               ? MessageLoop::TYPE_UI
                               : MessageLoop::TYPE_IO)),
      task_tracker_(new TestTaskTracker(this)) {
  CHECK(!TaskScheduler::GetInstance());

  // Instantiate a TaskScheduler with 1 thread in each of its 4 pools. Threads
  // stay alive even when they don't have work.
  constexpr int kMaxThreads = 1;
  const TimeDelta kSuggestedReclaimTime = TimeDelta::Max();
  const SchedulerWorkerPoolParams worker_pool_params(
      SchedulerWorkerPoolParams::StandbyThreadPolicy::ONE, kMaxThreads,
      kSuggestedReclaimTime);
  TaskScheduler::SetInstance(MakeUnique<internal::TaskSchedulerImpl>(
      "ScopedTaskEnvironment", WrapUnique(task_tracker_)));
  task_scheduler_ = TaskScheduler::GetInstance();
  TaskScheduler::GetInstance()->Start({worker_pool_params, worker_pool_params,
                                       worker_pool_params, worker_pool_params});

  if (execution_control_mode_ == ExecutionControlMode::QUEUED)
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
  task_tracker_->AllowRunRask();
  TaskScheduler::GetInstance()->FlushForTesting();
  TaskScheduler::GetInstance()->Shutdown();
  TaskScheduler::GetInstance()->JoinForTesting();
  TaskScheduler::SetInstance(nullptr);
}

scoped_refptr<base::SingleThreadTaskRunner>
ScopedTaskEnvironment::GetMainThreadTaskRunner() {
  return message_loop_.task_runner();
}

void ScopedTaskEnvironment::RunUntilIdle() {
  for (;;) {
    RunLoop run_loop;

    // Register a closure to stop running tasks on the main thread when the
    // TaskScheduler queue becomes empty.
    task_tracker_->SetTaskQueueEmptyClosure(run_loop.QuitWhenIdleClosure());

    // The closure registered above may never run if the TaskScheduler queue
    // starts empty. Post a TaskScheduler tasks to make sure that the queue
    // doesn't start empty.
    PostTask(FROM_HERE, BindOnce(&DoNothing));

    // Run main thread and TaskScheduler tasks.
    task_tracker_->AllowRunRask();
    TaskObserver task_observer;
    MessageLoop::current()->AddTaskObserver(&task_observer);
    run_loop.Run();
    MessageLoop::current()->RemoveTaskObserver(&task_observer);

    // If the ExecutionControlMode is QUEUED and DisallowRunTasks() fails,
    // another iteration is required to make sure that RunUntilIdle() doesn't
    // return while TaskScheduler tasks are still allowed to run.
    //
    // If tasks other than the QuitWhenIdle closure ran on the main thread, they
    // may have posted TaskScheduler tasks that didn't run yet. Another
    // iteration is required to run them.
    //
    // Note: DisallowRunTasks() fails when a TaskScheduler task is running. A
    // TaskScheduler task may be running after the TaskScheduler queue became
    // empty even if no tasks ran on the main thread in these cases:
    // - An undelayed task became ripe for execution.
    // - A task was posted from an external thread.
    if ((execution_control_mode_ == ExecutionControlMode::QUEUED &&
         task_tracker_->DisallowRunTasks()) ||
        task_observer.task_count() == 1) {
      return;
    }
  }
}

ScopedTaskEnvironment::TestTaskTracker::TestTaskTracker(
    ScopedTaskEnvironment* outer)
    : outer_(outer), can_run_tasks_cv_(&lock_) {}

void ScopedTaskEnvironment::TestTaskTracker::SetTaskQueueEmptyClosure(
    OnceClosure task_queue_empty_closure) {
  AutoLock auto_lock(lock_);
  CHECK(!task_queue_empty_closure_);
  task_queue_empty_closure_ = std::move(task_queue_empty_closure);
}

void ScopedTaskEnvironment::TestTaskTracker::AllowRunRask() {
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

void ScopedTaskEnvironment::TestTaskTracker::PerformRunTask(
    std::unique_ptr<internal::Task> task,
    const SequenceToken& sequence_token) {
  {
    AutoLock auto_lock(lock_);

    while (!can_run_tasks_)
      can_run_tasks_cv_.Wait();

    ++num_tasks_running_;
  }

  internal::TaskSchedulerImpl::TaskTrackerType::PerformRunTask(std::move(task),
                                                               sequence_token);

  {
    AutoLock auto_lock(lock_);

    CHECK_GT(num_tasks_running_, 0);
    CHECK(can_run_tasks_);

    --num_tasks_running_;

    // Notify the main thread when no undelayed tasks are queued and no task
    // other than the current one is running.
    if (num_tasks_running_ == 0 &&
        GetNumPendingUndelayedTasksForTesting() == 1 &&
        task_queue_empty_closure_) {
      outer_->GetMainThreadTaskRunner()->PostTask(
          FROM_HERE, std::move(task_queue_empty_closure_));
    }
  }
}

}  // namespace test
}  // namespace base
