// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"

#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/debug/stack_trace.h"

#include <queue>

using testing::_;
using testing::Invoke;
using testing::ElementsAre;

namespace base {
namespace sequence_manager {

namespace {

class ThreadControllerForTest
    : public internal::ThreadControllerWithMessagePumpImpl {
 public:
  ThreadControllerForTest(std::unique_ptr<MessagePump> pump,
                          const TickClock* clock)
      : ThreadControllerWithMessagePumpImpl(std::move(pump), clock) {}

  using ThreadControllerWithMessagePumpImpl::DoWork;
  using ThreadControllerWithMessagePumpImpl::DoDelayedWork;
  using ThreadControllerWithMessagePumpImpl::DoIdleWork;
};

class MockMessagePump : public MessagePump {
 public:
  MockMessagePump() {}
  ~MockMessagePump() override {}

  MOCK_METHOD1(Run, void(MessagePump::Delegate*));
  MOCK_METHOD0(Quit, void());
  MOCK_METHOD0(ScheduleWork, void());
  MOCK_METHOD1(ScheduleDelayedWork, void(const TimeTicks&));
  MOCK_METHOD1(SetTimerSlack, void(TimerSlack));
};

// TODO(crbug.com/901373): Deduplicate FakeTaskRunners.
class FakeTaskRunner : public SingleThreadTaskRunner {
 public:
  bool PostDelayedTask(const Location& from_here,
                       OnceClosure task,
                       TimeDelta delay) override {
    return true;
  }

  bool PostNonNestableDelayedTask(const Location& from_here,
                                  OnceClosure task,
                                  TimeDelta delay) override {
    return true;
  }

  bool RunsTasksInCurrentSequence() const override { return true; }

 protected:
  ~FakeTaskRunner() override = default;
};

class FakeSequencedTaskSource : public internal::SequencedTaskSource {
 public:
  explicit FakeSequencedTaskSource(TickClock* clock) : clock_(clock) {}
  ~FakeSequencedTaskSource() override = default;

  Optional<PendingTask> TakeTask() override {
    if (tasks_.empty())
      return nullopt;
    if (tasks_.front().delayed_run_time > clock_->NowTicks())
      return nullopt;
    PendingTask task = std::move(tasks_.front());
    tasks_.pop();
    return task;
  }

  void DidRunTask() override {}

  TimeDelta DelayTillNextTask(LazyNow* lazy_now) const override {
    if (tasks_.empty())
      return TimeDelta::Max();
    if (tasks_.front().delayed_run_time.is_null())
      return TimeDelta();
    if (lazy_now->Now() > tasks_.front().delayed_run_time)
      return TimeDelta();
    return tasks_.front().delayed_run_time - lazy_now->Now();
  }

  void AddTask(PendingTask task) {
    DCHECK(tasks_.empty() || task.delayed_run_time.is_null() ||
           tasks_.back().delayed_run_time < task.delayed_run_time);
    tasks_.push(std::move(task));
  }

  bool HasPendingHighResolutionTasks() override { return false; }

 private:
  TickClock* clock_;
  std::queue<PendingTask> tasks_;
};

TimeTicks Seconds(int seconds) {
  return TimeTicks() + TimeDelta::FromSeconds(seconds);
}

TimeTicks Days(int seconds) {
  return TimeTicks() + TimeDelta::FromDays(seconds);
}

}  // namespace

class ThreadControllerWithMessagePumpTest : public testing::Test {
 public:
  ThreadControllerWithMessagePumpTest()
      : message_pump_(new testing::StrictMock<MockMessagePump>()),
        thread_controller_(std::unique_ptr<MessagePump>(message_pump_),
                           &clock_),
        task_source_(&clock_) {
    thread_controller_.SetWorkBatchSize(1);
    thread_controller_.SetSequencedTaskSource(&task_source_);
  }

 protected:
  MockMessagePump* message_pump_;
  SimpleTestTickClock clock_;
  ThreadControllerForTest thread_controller_;
  FakeSequencedTaskSource task_source_;
};

TEST_F(ThreadControllerWithMessagePumpTest, ScheduleDelayedWork) {
  TimeTicks next_run_time;

  MockCallback<OnceClosure> task1;
  task_source_.AddTask(PendingTask(FROM_HERE, task1.Get(), Seconds(10)));
  MockCallback<OnceClosure> task2;
  task_source_.AddTask(PendingTask(FROM_HERE, task2.Get(), TimeTicks()));
  MockCallback<OnceClosure> task3;
  task_source_.AddTask(PendingTask(FROM_HERE, task3.Get(), Seconds(20)));

  // Call a no-op DoWork. Expect that it doesn't do any work, but
  // schedules a delayed wake-up appropriately.
  clock_.SetNowTicks(Seconds(5));
  EXPECT_CALL(*message_pump_, ScheduleDelayedWork(Seconds(10)));
  EXPECT_FALSE(thread_controller_.DoWork());
  testing::Mock::VerifyAndClearExpectations(message_pump_);

  // Call DoDelayedWork after the expiration of the delay.
  // Expect that a task will run and the next delay will equal to
  // TimeTicks() as we have immediate work to do.
  clock_.SetNowTicks(Seconds(11));
  EXPECT_CALL(task1, Run()).Times(1);
  EXPECT_TRUE(thread_controller_.DoDelayedWork(&next_run_time));
  EXPECT_EQ(next_run_time, TimeTicks());
  testing::Mock::VerifyAndClearExpectations(message_pump_);
  testing::Mock::VerifyAndClearExpectations(&task1);

  // Call DoWork immediately after the previous call. Expect a new task
  // to be run.
  EXPECT_CALL(task2, Run()).Times(1);
  EXPECT_CALL(*message_pump_, ScheduleDelayedWork(Seconds(20)));
  EXPECT_TRUE(thread_controller_.DoWork());
  testing::Mock::VerifyAndClearExpectations(message_pump_);
  testing::Mock::VerifyAndClearExpectations(&task2);

  // Call DoDelayedWork for the last task and expect to be told
  // about the lack of further delayed work (next run time being TimeTicks()).
  clock_.SetNowTicks(Seconds(21));
  EXPECT_CALL(task3, Run()).Times(1);
  EXPECT_TRUE(thread_controller_.DoDelayedWork(&next_run_time));
  EXPECT_EQ(next_run_time, TimeTicks());
  testing::Mock::VerifyAndClearExpectations(message_pump_);
  testing::Mock::VerifyAndClearExpectations(&task3);
}

TEST_F(ThreadControllerWithMessagePumpTest, SetNextDelayedDoWork) {
  EXPECT_CALL(*message_pump_, ScheduleDelayedWork(Seconds(123)));

  LazyNow lazy_now(&clock_);
  thread_controller_.SetNextDelayedDoWork(&lazy_now, Seconds(123));
}

TEST_F(ThreadControllerWithMessagePumpTest, SetNextDelayedDoWork_CapAtOneDay) {
  EXPECT_CALL(*message_pump_, ScheduleDelayedWork(Days(1)));

  LazyNow lazy_now(&clock_);
  thread_controller_.SetNextDelayedDoWork(&lazy_now, Days(2));
}

TEST_F(ThreadControllerWithMessagePumpTest, DelayedWork_CapAtOneDay) {
  MockCallback<OnceClosure> task1;
  task_source_.AddTask(PendingTask(FROM_HERE, task1.Get(), Days(10)));

  EXPECT_CALL(*message_pump_, ScheduleDelayedWork(Days(1)));
  EXPECT_FALSE(thread_controller_.DoWork());
}

TEST_F(ThreadControllerWithMessagePumpTest, NestedExecution) {
  // This test posts three immediate tasks. The first creates a nested RunLoop
  // and the test expects that the second and third tasks are run outside of
  // the nested loop.
  std::vector<std::string> log;

  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&log, this](MessagePump::Delegate* delegate) {
        log.push_back("entering top-level runloop");
        EXPECT_EQ(delegate, &thread_controller_);
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_FALSE(delegate->DoWork());
        log.push_back("exiting top-level runloop");
      }))
      .WillOnce(Invoke([&log, this](MessagePump::Delegate* delegate) {
        log.push_back("entering nested runloop");
        EXPECT_EQ(delegate, &thread_controller_);
        EXPECT_FALSE(thread_controller_.IsTaskExecutionAllowed());
        EXPECT_FALSE(delegate->DoWork());
        log.push_back("exiting nested runloop");
      }));

  task_source_.AddTask(
      PendingTask(FROM_HERE,
                  base::BindOnce(
                      [](std::vector<std::string>* log,
                         ThreadControllerForTest* controller) {
                        EXPECT_FALSE(controller->IsTaskExecutionAllowed());
                        log->push_back("task1");
                        RunLoop().Run();
                      },
                      &log, &thread_controller_),
                  TimeTicks()));
  task_source_.AddTask(
      PendingTask(FROM_HERE,
                  base::BindOnce(
                      [](std::vector<std::string>* log,
                         ThreadControllerForTest* controller) {
                        EXPECT_FALSE(controller->IsTaskExecutionAllowed());
                        log->push_back("task2");
                      },
                      &log, &thread_controller_),
                  TimeTicks()));
  task_source_.AddTask(
      PendingTask(FROM_HERE,
                  base::BindOnce(
                      [](std::vector<std::string>* log,
                         ThreadControllerForTest* controller) {
                        EXPECT_FALSE(controller->IsTaskExecutionAllowed());
                        log->push_back("task3");
                      },
                      &log, &thread_controller_),
                  TimeTicks()));

  EXPECT_TRUE(thread_controller_.IsTaskExecutionAllowed());
  RunLoop().Run();

  EXPECT_THAT(log,
              ElementsAre("entering top-level runloop", "task1",
                          "entering nested runloop", "exiting nested runloop",
                          "task2", "task3", "exiting top-level runloop"));
  testing::Mock::VerifyAndClearExpectations(message_pump_);
}

TEST_F(ThreadControllerWithMessagePumpTest,
       NestedExecutionWithApplicationTasks) {
  // THis test is similar to the previous one, but execution is explicitly
  // allowed (by specifying appropriate RunLoop type), and tasks are run inside
  // nested runloop.
  std::vector<std::string> log;

  ThreadTaskRunnerHandle handle(MakeRefCounted<FakeTaskRunner>());

  EXPECT_CALL(*message_pump_, Run(_))
      .WillOnce(Invoke([&log, this](MessagePump::Delegate* delegate) {
        log.push_back("entering top-level runloop");
        EXPECT_EQ(delegate, &thread_controller_);
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_FALSE(delegate->DoWork());
        log.push_back("exiting top-level runloop");
      }))
      .WillOnce(Invoke([&log, this](MessagePump::Delegate* delegate) {
        log.push_back("entering nested runloop");
        EXPECT_EQ(delegate, &thread_controller_);
        EXPECT_TRUE(thread_controller_.IsTaskExecutionAllowed());
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_TRUE(delegate->DoWork());
        EXPECT_FALSE(delegate->DoWork());
        log.push_back("exiting nested runloop");
      }));

  task_source_.AddTask(
      PendingTask(FROM_HERE,
                  base::BindOnce(
                      [](std::vector<std::string>* log,
                         ThreadControllerForTest* controller) {
                        EXPECT_FALSE(controller->IsTaskExecutionAllowed());
                        log->push_back("task1");
                        RunLoop(RunLoop::Type::kNestableTasksAllowed).Run();
                      },
                      &log, &thread_controller_),
                  TimeTicks()));
  task_source_.AddTask(
      PendingTask(FROM_HERE,
                  base::BindOnce(
                      [](std::vector<std::string>* log,
                         ThreadControllerForTest* controller) {
                        EXPECT_FALSE(controller->IsTaskExecutionAllowed());
                        log->push_back("task2");
                      },
                      &log, &thread_controller_),
                  TimeTicks()));
  task_source_.AddTask(
      PendingTask(FROM_HERE,
                  base::BindOnce(
                      [](std::vector<std::string>* log,
                         ThreadControllerForTest* controller) {
                        EXPECT_FALSE(controller->IsTaskExecutionAllowed());
                        log->push_back("task3");
                      },
                      &log, &thread_controller_),
                  TimeTicks()));

  EXPECT_TRUE(thread_controller_.IsTaskExecutionAllowed());
  RunLoop().Run();

  EXPECT_THAT(
      log, ElementsAre("entering top-level runloop", "task1",
                       "entering nested runloop", "task2", "task3",
                       "exiting nested runloop", "exiting top-level runloop"));
  testing::Mock::VerifyAndClearExpectations(message_pump_);
}

TEST_F(ThreadControllerWithMessagePumpTest, SetDefaultTaskRunner) {
  scoped_refptr<SingleThreadTaskRunner> task_runner1 =
      MakeRefCounted<FakeTaskRunner>();
  thread_controller_.SetDefaultTaskRunner(task_runner1);
  EXPECT_EQ(task_runner1, ThreadTaskRunnerHandle::Get());

  // Check that we are correctly supporting overriding.
  scoped_refptr<SingleThreadTaskRunner> task_runner2 =
      MakeRefCounted<FakeTaskRunner>();
  thread_controller_.SetDefaultTaskRunner(task_runner2);
  EXPECT_EQ(task_runner2, ThreadTaskRunnerHandle::Get());
}

}  // namespace sequence_manager
}  // namespace base
