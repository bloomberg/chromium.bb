// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_CAN_RUN_POLICY_TEST_H_
#define BASE_TASK_TASK_SCHEDULER_CAN_RUN_POLICY_TEST_H_

#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_scheduler/task_tracker.h"
#include "base/task/task_scheduler/test_utils.h"
#include "base/task_runner.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"

namespace base {
namespace internal {
namespace test {

// Verify that tasks only run when allowed by the CanRunPolicy. |target| is the
// object on which DidUpdateCanRunPolicy() must be called after updating the
// CanRunPolicy in |task_tracker|. |create_task_runner| is a function that
// receives a TaskPriority and returns a TaskRunner. |task_tracker| is the
// TaskTracker.
template <typename Target, typename CreateTaskRunner>
void TestCanRunPolicyBasic(Target* target,
                           CreateTaskRunner create_task_runner,
                           TaskTracker* task_tracker) {
  AtomicFlag foreground_can_run;
  WaitableEvent foreground_did_run;
  AtomicFlag best_effort_can_run;
  WaitableEvent best_effort_did_run;

  task_tracker->SetCanRunPolicy(CanRunPolicy::kNone);
  target->DidUpdateCanRunPolicy();

  const auto user_visible_task_runner =
      create_task_runner(TaskPriority::USER_VISIBLE);
  user_visible_task_runner->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                                       EXPECT_TRUE(foreground_can_run.IsSet());
                                       foreground_did_run.Signal();
                                     }));
  const auto best_effort_task_runner =
      create_task_runner(TaskPriority::BEST_EFFORT);
  best_effort_task_runner->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                                      EXPECT_TRUE(best_effort_can_run.IsSet());
                                      best_effort_did_run.Signal();
                                    }));

  PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  foreground_can_run.Set();
  task_tracker->SetCanRunPolicy(CanRunPolicy::kForegroundOnly);
  target->DidUpdateCanRunPolicy();
  foreground_did_run.Wait();

  PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  best_effort_can_run.Set();
  task_tracker->SetCanRunPolicy(CanRunPolicy::kAll);
  target->DidUpdateCanRunPolicy();
  best_effort_did_run.Wait();
}

// Verify that if a task was allowed to run by the CanRunPolicy when it was
// posted, but the CanRunPolicy is updated to disallow it from running before it
// starts running, it doesn't run. |target| is the object on which
// DidUpdateCanRunPolicy() must be called after updating the CanRunPolicy in
// |task_tracker|. |create_task_runner| is a function that receives a
// TaskPriority and returns a *Sequenced*TaskRunner. |task_tracker| is the
// TaskTracker.
template <typename Target, typename CreateTaskRunner>
void TestCanRunPolicyChangedBeforeRun(Target* target,
                                      CreateTaskRunner create_task_runner,
                                      TaskTracker* task_tracker) {
  constexpr struct {
    // Descriptor for the test case.
    const char* descriptor;
    // Task priority being tested.
    TaskPriority priority;
    // Policy that disallows running tasks with |priority|.
    CanRunPolicy disallow_policy;
    // Policy that allows running tasks with |priority|.
    CanRunPolicy allow_policy;
  } kTestCases[] = {
      {"BestEffort/kNone/kAll", TaskPriority::BEST_EFFORT, CanRunPolicy::kNone,
       CanRunPolicy::kAll},
      {"BestEffort/kForegroundOnly/kAll", TaskPriority::BEST_EFFORT,
       CanRunPolicy::kForegroundOnly, CanRunPolicy::kAll},
      {"UserVisible/kNone/kForegroundOnly", TaskPriority::USER_VISIBLE,
       CanRunPolicy::kNone, CanRunPolicy::kForegroundOnly},
      {"UserVisible/kNone/kAll", TaskPriority::USER_VISIBLE,
       CanRunPolicy::kNone, CanRunPolicy::kAll}};

  for (auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.descriptor);

    WaitableEvent first_task_started;
    WaitableEvent first_task_blocked;
    AtomicFlag second_task_can_run;

    task_tracker->SetCanRunPolicy(test_case.allow_policy);
    target->DidUpdateCanRunPolicy();

    const auto task_runner = create_task_runner(test_case.priority);
    task_runner->PostTask(
        FROM_HERE, BindLambdaForTesting([&]() {
          first_task_started.Signal();
          test::WaitWithoutBlockingObserver(&first_task_blocked);
        }));
    task_runner->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                            EXPECT_TRUE(second_task_can_run.IsSet());
                          }));

    first_task_started.Wait();
    task_tracker->SetCanRunPolicy(test_case.disallow_policy);
    target->DidUpdateCanRunPolicy();
    first_task_blocked.Signal();

    PlatformThread::Sleep(TestTimeouts::tiny_timeout());

    second_task_can_run.Set();
    task_tracker->SetCanRunPolicy(test_case.allow_policy);
    target->DidUpdateCanRunPolicy();
    task_tracker->FlushForTesting();
  }
}

}  // namespace test
}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_TASK_SCHEDULER_CAN_RUN_POLICY_TEST_H_
