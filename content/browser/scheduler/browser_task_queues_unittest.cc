// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_queues.h"

#include <array>
#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using ::base::MessageLoop;
using ::base::RunLoop;
using ::base::sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump;
using ::base::sequence_manager::SequenceManager;
using ::testing::Invoke;
using ::testing::Mock;

using MockTask =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void()>>>;

using QueueType = BrowserTaskQueues::QueueType;

class BrowserTaskQueuesTest : public testing::Test {
 protected:
  BrowserTaskQueuesTest()
      : sequence_manager_(CreateSequenceManagerOnCurrentThreadWithPump(
            MessageLoop::CreateMessagePumpForType(MessageLoop::TYPE_DEFAULT))),
        queues_(std::make_unique<BrowserTaskQueues>(
            BrowserThread::UI,
            sequence_manager_.get(),
            sequence_manager_->GetRealTimeDomain())),
        handle_(queues_->CreateHandle()) {
    sequence_manager_->SetDefaultTaskRunner(
        handle_.task_runner(QueueType::kDefault));
  }

  std::unique_ptr<SequenceManager> sequence_manager_;
  std::unique_ptr<BrowserTaskQueues> queues_;
  BrowserTaskQueues::Handle handle_;
};

TEST_F(BrowserTaskQueuesTest, SimplePosting) {
  scoped_refptr<base::SingleThreadTaskRunner> tq =
      handle_.task_runner(QueueType::kDefault);

  MockTask task_1;
  MockTask task_2;
  MockTask task_3;

  {
    testing::InSequence s;
    EXPECT_CALL(task_1, Run);
    EXPECT_CALL(task_2, Run);
    EXPECT_CALL(task_3, Run);
  }

  tq->PostTask(FROM_HERE, task_1.Get());
  tq->PostTask(FROM_HERE, task_2.Get());
  tq->PostTask(FROM_HERE, task_3.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(BrowserTaskQueuesTest, RunAllPendingTasksForTesting) {
  handle_.EnableBestEffortQueues();

  MockTask task;
  MockTask followup_task;
  EXPECT_CALL(task, Run).WillOnce(Invoke([&]() {
    for (size_t i = 0; i < BrowserTaskQueues::kNumQueueTypes; ++i) {
      handle_.task_runner(static_cast<QueueType>(i))
          ->PostTask(FROM_HERE, followup_task.Get());
    }
  }));

  handle_.task_runner(QueueType::kDefault)->PostTask(FROM_HERE, task.Get());

  {
    RunLoop run_loop;
    handle_.ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  Mock::VerifyAndClearExpectations(&task);
  EXPECT_CALL(followup_task, Run).Times(BrowserTaskQueues::kNumQueueTypes);

  RunLoop run_loop;
  handle_.ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BrowserTaskQueuesTest, RunAllPendingTasksForTestingRunsAllTasks) {
  constexpr size_t kTasksPerPriority = 100;
  handle_.EnableBestEffortQueues();

  MockTask task;
  EXPECT_CALL(task, Run).Times(BrowserTaskQueues::kNumQueueTypes *
                               kTasksPerPriority);
  for (size_t i = 0; i < BrowserTaskQueues::kNumQueueTypes; ++i) {
    for (size_t j = 0; j < kTasksPerPriority; ++j) {
      handle_.task_runner(static_cast<QueueType>(i))
          ->PostTask(FROM_HERE, task.Get());
    }
  }

  RunLoop run_loop;
  handle_.ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BrowserTaskQueuesTest, RunAllPendingTasksForTestingIsReentrant) {
  MockTask task_1;
  MockTask task_2;
  MockTask task_3;

  EXPECT_CALL(task_1, Run).WillOnce(Invoke([&]() {
    handle_.task_runner(QueueType::kDefault)->PostTask(FROM_HERE, task_2.Get());
    RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
    handle_.ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }));

  EXPECT_CALL(task_2, Run).WillOnce(Invoke([&]() {
    handle_.task_runner(QueueType::kDefault)->PostTask(FROM_HERE, task_3.Get());
  }));

  handle_.task_runner(QueueType::kDefault)->PostTask(FROM_HERE, task_1.Get());

  RunLoop run_loop;
  handle_.ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BrowserTaskQueuesTest,
       RunAllPendingTasksForTestingIgnoresBestEffortIfNotEnabled) {
  MockTask best_effort_task;
  MockTask default_task;

  handle_.task_runner(QueueType::kBestEffort)
      ->PostTask(FROM_HERE, best_effort_task.Get());
  handle_.task_runner(QueueType::kDefault)
      ->PostTask(FROM_HERE, default_task.Get());

  EXPECT_CALL(default_task, Run);

  RunLoop run_loop;
  handle_.ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BrowserTaskQueuesTest,
       RunAllPendingTasksForTestingRunsBestEffortTasksWhenEnabled) {
  MockTask task_1;
  MockTask task_2;
  MockTask task_3;

  EXPECT_CALL(task_1, Run).WillOnce(Invoke([&]() {
    // This task should not run as it is posted after the
    // RunAllPendingTasksForTesting() call
    handle_.task_runner(QueueType::kBestEffort)
        ->PostTask(FROM_HERE, task_3.Get());
    handle_.EnableBestEffortQueues();
  }));
  EXPECT_CALL(task_2, Run);

  handle_.task_runner(QueueType::kDefault)->PostTask(FROM_HERE, task_1.Get());
  handle_.task_runner(QueueType::kBestEffort)
      ->PostTask(FROM_HERE, task_2.Get());

  RunLoop run_loop;
  handle_.ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BrowserTaskQueuesTest, HandleStillWorksWhenQueuesDestroyed) {
  MockTask task;
  queues_.reset();

  for (size_t i = 0; i < BrowserTaskQueues::kNumQueueTypes; ++i) {
    EXPECT_FALSE(
        handle_.task_runner(static_cast<QueueType>(i))
            ->PostTask(FROM_HERE, base::BindLambdaForTesting([]() {})));
  }

  handle_.EnableBestEffortQueues();
  RunLoop run_loop;
  handle_.ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace
}  // namespace content