// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_task_environment.h"

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {

namespace {

class ScopedTaskEnvironmentTest
    : public testing::TestWithParam<ScopedTaskEnvironment::MainThreadType> {};

void VerifyRunUntilIdleDidNotReturnAndSetFlag(
    AtomicFlag* run_until_idle_returned,
    AtomicFlag* task_ran) {
  EXPECT_FALSE(run_until_idle_returned->IsSet());
  task_ran->Set();
}

void RunUntilIdleTest(
    ScopedTaskEnvironment::MainThreadType main_thread_type,
    ScopedTaskEnvironment::ExecutionMode execution_control_mode) {
  AtomicFlag run_until_idle_returned;
  ScopedTaskEnvironment scoped_task_environment(main_thread_type,
                                                execution_control_mode);

  AtomicFlag first_main_thread_task_ran;
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
                          Unretained(&run_until_idle_returned),
                          Unretained(&first_main_thread_task_ran)));

  AtomicFlag first_task_scheduler_task_ran;
  PostTask(FROM_HERE, BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
                               Unretained(&run_until_idle_returned),
                               Unretained(&first_task_scheduler_task_ran)));

  AtomicFlag second_task_scheduler_task_ran;
  AtomicFlag second_main_thread_task_ran;
  PostTaskAndReply(FROM_HERE,
                   BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
                            Unretained(&run_until_idle_returned),
                            Unretained(&second_task_scheduler_task_ran)),
                   BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
                            Unretained(&run_until_idle_returned),
                            Unretained(&second_main_thread_task_ran)));

  scoped_task_environment.RunUntilIdle();
  run_until_idle_returned.Set();

  EXPECT_TRUE(first_main_thread_task_ran.IsSet());
  EXPECT_TRUE(first_task_scheduler_task_ran.IsSet());
  EXPECT_TRUE(second_task_scheduler_task_ran.IsSet());
  EXPECT_TRUE(second_main_thread_task_ran.IsSet());
}

}  // namespace

TEST_P(ScopedTaskEnvironmentTest, QueuedRunUntilIdle) {
  RunUntilIdleTest(GetParam(), ScopedTaskEnvironment::ExecutionMode::QUEUED);
}

TEST_P(ScopedTaskEnvironmentTest, AsyncRunUntilIdle) {
  RunUntilIdleTest(GetParam(), ScopedTaskEnvironment::ExecutionMode::ASYNC);
}

// Verify that tasks posted to an ExecutionMode::QUEUED ScopedTaskEnvironment do
// not run outside of RunUntilIdle().
TEST_P(ScopedTaskEnvironmentTest, QueuedTasksDoNotRunOutsideOfRunUntilIdle) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ExecutionMode::QUEUED);

  AtomicFlag run_until_idle_called;
  PostTask(FROM_HERE, BindOnce(
                          [](AtomicFlag* run_until_idle_called) {
                            EXPECT_TRUE(run_until_idle_called->IsSet());
                          },
                          Unretained(&run_until_idle_called)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  run_until_idle_called.Set();
  scoped_task_environment.RunUntilIdle();

  AtomicFlag other_run_until_idle_called;
  PostTask(FROM_HERE, BindOnce(
                          [](AtomicFlag* other_run_until_idle_called) {
                            EXPECT_TRUE(other_run_until_idle_called->IsSet());
                          },
                          Unretained(&other_run_until_idle_called)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  other_run_until_idle_called.Set();
  scoped_task_environment.RunUntilIdle();
}

// Verify that a task posted to an ExecutionMode::ASYNC ScopedTaskEnvironment
// can run without a call to RunUntilIdle().
TEST_P(ScopedTaskEnvironmentTest, AsyncTasksRunAsTheyArePosted) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ExecutionMode::ASYNC);

  WaitableEvent task_ran(WaitableEvent::ResetPolicy::MANUAL,
                         WaitableEvent::InitialState::NOT_SIGNALED);
  PostTask(FROM_HERE,
           BindOnce([](WaitableEvent* task_ran) { task_ran->Signal(); },
                    Unretained(&task_ran)));
  task_ran.Wait();
}

// Verify that a task posted to an ExecutionMode::ASYNC ScopedTaskEnvironment
// after a call to RunUntilIdle() can run without another call to
// RunUntilIdle().
TEST_P(ScopedTaskEnvironmentTest,
       AsyncTasksRunAsTheyArePostedAfterRunUntilIdle) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ExecutionMode::ASYNC);

  scoped_task_environment.RunUntilIdle();

  WaitableEvent task_ran(WaitableEvent::ResetPolicy::MANUAL,
                         WaitableEvent::InitialState::NOT_SIGNALED);
  PostTask(FROM_HERE,
           BindOnce([](WaitableEvent* task_ran) { task_ran->Signal(); },
                    Unretained(&task_ran)));
  task_ran.Wait();
}

TEST_P(ScopedTaskEnvironmentTest, DelayedTasks) {
  // Use a QUEUED execution-mode environment, so that no tasks are actually
  // executed until RunUntilIdle()/FastForwardBy() are invoked.
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ExecutionMode::QUEUED);

  subtle::Atomic32 counter = 0;

  // Should run only in MOCK_TIME environment when time is fast-forwarded.
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      Bind(
          [](subtle::Atomic32* counter) {
            subtle::NoBarrier_AtomicIncrement(counter, 4);
          },
          Unretained(&counter)),
      TimeDelta::FromDays(1));
  // TODO(gab): This currently doesn't run because the TaskScheduler's clock
  // isn't mocked but it should be.
  PostDelayedTask(FROM_HERE,
                  Bind(
                      [](subtle::Atomic32* counter) {
                        subtle::NoBarrier_AtomicIncrement(counter, 128);
                      },
                      Unretained(&counter)),
                  TimeDelta::FromDays(1));

  // Same as first task, longer delays to exercise
  // FastForwardUntilNoTasksRemain().
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      Bind(
          [](subtle::Atomic32* counter) {
            subtle::NoBarrier_AtomicIncrement(counter, 8);
          },
          Unretained(&counter)),
      TimeDelta::FromDays(5));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      Bind(
          [](subtle::Atomic32* counter) {
            subtle::NoBarrier_AtomicIncrement(counter, 16);
          },
          Unretained(&counter)),
      TimeDelta::FromDays(7));

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, Bind(
                     [](subtle::Atomic32* counter) {
                       subtle::NoBarrier_AtomicIncrement(counter, 1);
                     },
                     Unretained(&counter)));
  PostTask(FROM_HERE, Bind(
                          [](subtle::Atomic32* counter) {
                            subtle::NoBarrier_AtomicIncrement(counter, 2);
                          },
                          Unretained(&counter)));

  // This expectation will fail flakily if the preceding PostTask() is executed
  // asynchronously, indicating a problem with the QUEUED execution mode.
  int expected_value = 0;
  EXPECT_EQ(expected_value, counter);

  // RunUntilIdle() should process non-delayed tasks only in all queues.
  scoped_task_environment.RunUntilIdle();
  expected_value += 1;
  expected_value += 2;
  EXPECT_EQ(expected_value, counter);

  if (GetParam() == ScopedTaskEnvironment::MainThreadType::MOCK_TIME) {
    scoped_task_environment.FastForwardBy(TimeDelta::FromSeconds(1));
    EXPECT_EQ(expected_value, counter);

    scoped_task_environment.FastForwardBy(TimeDelta::FromDays(1));
    expected_value += 4;
    EXPECT_EQ(expected_value, counter);

    scoped_task_environment.FastForwardUntilNoTasksRemain();
    expected_value += 8;
    expected_value += 16;
    EXPECT_EQ(expected_value, counter);
  }
}

INSTANTIATE_TEST_CASE_P(
    MainThreadDefault,
    ScopedTaskEnvironmentTest,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::DEFAULT));
INSTANTIATE_TEST_CASE_P(
    MainThreadMockTime,
    ScopedTaskEnvironmentTest,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::MOCK_TIME));
INSTANTIATE_TEST_CASE_P(
    MainThreadUI,
    ScopedTaskEnvironmentTest,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::UI));
INSTANTIATE_TEST_CASE_P(
    MainThreadIO,
    ScopedTaskEnvironmentTest,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::IO));

}  // namespace test
}  // namespace base
