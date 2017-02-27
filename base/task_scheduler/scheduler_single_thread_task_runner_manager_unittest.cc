// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/scheduler_single_thread_task_runner_manager.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_scheduler/delayed_task_manager.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task_scheduler/task_tracker.h"
#include "base/task_scheduler/task_traits.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

enum WorkerPoolType {
  BACKGROUND_WORKER_POOL = 0,
  FOREGROUND_WORKER_POOL,
};

static size_t GetThreadPoolIndexForTraits(const TaskTraits& traits) {
  return traits.priority() == TaskPriority::BACKGROUND ? BACKGROUND_WORKER_POOL
                                                       : FOREGROUND_WORKER_POOL;
}

class TaskSchedulerSingleThreadTaskRunnerManagerTest : public testing::Test {
 public:
  TaskSchedulerSingleThreadTaskRunnerManagerTest()
      : service_thread_("TaskSchedulerServiceThread") {}

  void SetUp() override {
    service_thread_.Start();

    using StandbyThreadPolicy = SchedulerWorkerPoolParams::StandbyThreadPolicy;

    std::vector<SchedulerWorkerPoolParams> params_vector;

    ASSERT_EQ(BACKGROUND_WORKER_POOL, params_vector.size());
    params_vector.emplace_back("Background", ThreadPriority::BACKGROUND,
                               StandbyThreadPolicy::LAZY, 1U, TimeDelta::Max());

    ASSERT_EQ(FOREGROUND_WORKER_POOL, params_vector.size());
    params_vector.emplace_back("Foreground", ThreadPriority::NORMAL,
                               StandbyThreadPolicy::LAZY, 1U, TimeDelta::Max());

    delayed_task_manager_ =
        MakeUnique<DelayedTaskManager>(service_thread_.task_runner());
    single_thread_task_runner_manager_ =
        MakeUnique<SchedulerSingleThreadTaskRunnerManager>(
            params_vector, Bind(&GetThreadPoolIndexForTraits), &task_tracker_,
            delayed_task_manager_.get());
  }

  void TearDown() override {
    single_thread_task_runner_manager_->JoinForTesting();
    single_thread_task_runner_manager_.reset();
    delayed_task_manager_.reset();
    service_thread_.Stop();
  }

 protected:
  std::unique_ptr<SchedulerSingleThreadTaskRunnerManager>
      single_thread_task_runner_manager_;
  TaskTracker task_tracker_;

 private:
  Thread service_thread_;
  std::unique_ptr<DelayedTaskManager> delayed_task_manager_;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerSingleThreadTaskRunnerManagerTest);
};

void CaptureThreadRef(PlatformThreadRef* thread_ref) {
  ASSERT_TRUE(thread_ref);
  *thread_ref = PlatformThread::CurrentRef();
}

void CaptureThreadPriority(ThreadPriority* thread_priority) {
  ASSERT_TRUE(thread_priority);
  *thread_priority = PlatformThread::GetCurrentThreadPriority();
}

void ShouldNotRun() {
  ADD_FAILURE() << "Ran a task that shouldn't run.";
}

}  // namespace

TEST_F(TaskSchedulerSingleThreadTaskRunnerManagerTest, DifferentThreadsUsed) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1 =
      single_thread_task_runner_manager_
          ->CreateSingleThreadTaskRunnerWithTraits(
              TaskTraits().WithShutdownBehavior(
                  TaskShutdownBehavior::BLOCK_SHUTDOWN));
  scoped_refptr<SingleThreadTaskRunner> task_runner_2 =
      single_thread_task_runner_manager_
          ->CreateSingleThreadTaskRunnerWithTraits(
              TaskTraits().WithShutdownBehavior(
                  TaskShutdownBehavior::BLOCK_SHUTDOWN));

  PlatformThreadRef thread_ref_1;
  task_runner_1->PostTask(FROM_HERE, Bind(&CaptureThreadRef, &thread_ref_1));
  PlatformThreadRef thread_ref_2;
  task_runner_2->PostTask(FROM_HERE, Bind(&CaptureThreadRef, &thread_ref_2));

  task_tracker_.Shutdown();

  ASSERT_FALSE(thread_ref_1.is_null());
  ASSERT_FALSE(thread_ref_2.is_null());
  EXPECT_NE(thread_ref_1, thread_ref_2);
}

TEST_F(TaskSchedulerSingleThreadTaskRunnerManagerTest, PrioritySetCorrectly) {
  // Why are events used here instead of the task tracker?
  // Shutting down can cause priorities to get raised. This means we have to use
  // events to determine when a task is run.
  scoped_refptr<SingleThreadTaskRunner> task_runner_background =
      single_thread_task_runner_manager_
          ->CreateSingleThreadTaskRunnerWithTraits(
              TaskTraits().WithPriority(TaskPriority::BACKGROUND));
  scoped_refptr<SingleThreadTaskRunner> task_runner_user_visible =
      single_thread_task_runner_manager_
          ->CreateSingleThreadTaskRunnerWithTraits(
              TaskTraits().WithPriority(TaskPriority::USER_VISIBLE));
  scoped_refptr<SingleThreadTaskRunner> task_runner_user_blocking =
      single_thread_task_runner_manager_
          ->CreateSingleThreadTaskRunnerWithTraits(
              TaskTraits()
                  .WithPriority(TaskPriority::USER_BLOCKING)
                  .WithShutdownBehavior(TaskShutdownBehavior::BLOCK_SHUTDOWN));

  ThreadPriority thread_priority_background;
  task_runner_background->PostTask(
      FROM_HERE, Bind(&CaptureThreadPriority, &thread_priority_background));
  WaitableEvent waitable_event_background(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner_background->PostTask(
      FROM_HERE,
      Bind(&WaitableEvent::Signal, Unretained(&waitable_event_background)));

  ThreadPriority thread_priority_user_visible;
  task_runner_user_visible->PostTask(
      FROM_HERE, Bind(&CaptureThreadPriority, &thread_priority_user_visible));
  WaitableEvent waitable_event_user_visible(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner_user_visible->PostTask(
      FROM_HERE,
      Bind(&WaitableEvent::Signal, Unretained(&waitable_event_user_visible)));

  ThreadPriority thread_priority_user_blocking;
  task_runner_user_blocking->PostTask(
      FROM_HERE, Bind(&CaptureThreadPriority, &thread_priority_user_blocking));
  WaitableEvent waitable_event_user_blocking(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner_user_blocking->PostTask(
      FROM_HERE,
      Bind(&WaitableEvent::Signal, Unretained(&waitable_event_user_blocking)));

  waitable_event_background.Wait();
  waitable_event_user_visible.Wait();
  waitable_event_user_blocking.Wait();

  if (Lock::HandlesMultipleThreadPriorities())
    EXPECT_EQ(ThreadPriority::BACKGROUND, thread_priority_background);
  else
    EXPECT_EQ(ThreadPriority::NORMAL, thread_priority_background);
  EXPECT_EQ(ThreadPriority::NORMAL, thread_priority_user_visible);
  EXPECT_EQ(ThreadPriority::NORMAL, thread_priority_user_blocking);
}

TEST_F(TaskSchedulerSingleThreadTaskRunnerManagerTest, PostTaskAfterShutdown) {
  auto task_runner = single_thread_task_runner_manager_
                         ->CreateSingleThreadTaskRunnerWithTraits(TaskTraits());
  task_tracker_.Shutdown();
  EXPECT_FALSE(task_runner->PostTask(FROM_HERE, Bind(&ShouldNotRun)));
}

// Verify that a Task runs shortly after its delay expires.
TEST_F(TaskSchedulerSingleThreadTaskRunnerManagerTest, PostDelayedTask) {
  TimeTicks start_time = TimeTicks::Now();

  // Post a task with a short delay.
  WaitableEvent task_ran(WaitableEvent::ResetPolicy::MANUAL,
                         WaitableEvent::InitialState::NOT_SIGNALED);
  auto task_runner = single_thread_task_runner_manager_
                         ->CreateSingleThreadTaskRunnerWithTraits(TaskTraits());
  EXPECT_TRUE(task_runner->PostDelayedTask(
      FROM_HERE, Bind(&WaitableEvent::Signal, Unretained(&task_ran)),
      TestTimeouts::tiny_timeout()));

  // Wait until the task runs.
  task_ran.Wait();

  // Expect the task to run after its delay expires, but not more than 250 ms
  // after that.
  const TimeDelta actual_delay = TimeTicks::Now() - start_time;
  EXPECT_GE(actual_delay, TestTimeouts::tiny_timeout());
  EXPECT_LT(actual_delay,
            TimeDelta::FromMilliseconds(250) + TestTimeouts::tiny_timeout());
}

TEST_F(TaskSchedulerSingleThreadTaskRunnerManagerTest,
       RunsTasksOnCurrentThread) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1 =
      single_thread_task_runner_manager_
          ->CreateSingleThreadTaskRunnerWithTraits(
              TaskTraits().WithShutdownBehavior(
                  TaskShutdownBehavior::BLOCK_SHUTDOWN));
  scoped_refptr<SingleThreadTaskRunner> task_runner_2 =
      single_thread_task_runner_manager_
          ->CreateSingleThreadTaskRunnerWithTraits(
              TaskTraits().WithShutdownBehavior(
                  TaskShutdownBehavior::BLOCK_SHUTDOWN));

  EXPECT_FALSE(task_runner_1->RunsTasksOnCurrentThread());
  EXPECT_FALSE(task_runner_2->RunsTasksOnCurrentThread());

  task_runner_1->PostTask(
      FROM_HERE,
      Bind(
          [](scoped_refptr<SingleThreadTaskRunner> task_runner_1,
             scoped_refptr<SingleThreadTaskRunner> task_runner_2) {
            EXPECT_TRUE(task_runner_1->RunsTasksOnCurrentThread());
            EXPECT_FALSE(task_runner_2->RunsTasksOnCurrentThread());
          },
          task_runner_1, task_runner_2));

  task_runner_2->PostTask(
      FROM_HERE,
      Bind(
          [](scoped_refptr<SingleThreadTaskRunner> task_runner_1,
             scoped_refptr<SingleThreadTaskRunner> task_runner_2) {
            EXPECT_FALSE(task_runner_1->RunsTasksOnCurrentThread());
            EXPECT_TRUE(task_runner_2->RunsTasksOnCurrentThread());
          },
          task_runner_1, task_runner_2));

  task_tracker_.Shutdown();
}

}  // namespace internal
}  // namespace base
