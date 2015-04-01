// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/scheduler/scheduler_helper.h"

#include "base/callback.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "content/child/scheduler/nestable_task_runner_for_test.h"
#include "content/child/scheduler/scheduler_message_loop_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace content {

namespace {
void AppendToVectorTestTask(std::vector<std::string>* vector,
                            std::string value) {
  vector->push_back(value);
}

void AppendToVectorIdleTestTask(std::vector<std::string>* vector,
                                std::string value,
                                base::TimeTicks deadline) {
  AppendToVectorTestTask(vector, value);
}

void NullTask() {
}

void AppendToVectorReentrantTask(
    base::SingleThreadTaskRunner* task_runner,
    std::vector<int>* vector,
    int* reentrant_count,
    int max_reentrant_count) {
  vector->push_back((*reentrant_count)++);
  if (*reentrant_count < max_reentrant_count) {
    task_runner->PostTask(
        FROM_HERE, base::Bind(AppendToVectorReentrantTask,
                              base::Unretained(task_runner), vector,
                              reentrant_count, max_reentrant_count));
  }
}

void NullIdleTask(base::TimeTicks) {
}

void IdleTestTask(int* run_count,
                  base::TimeTicks* deadline_out,
                  base::TimeTicks deadline) {
  (*run_count)++;
  *deadline_out = deadline;
}

int max_idle_task_reposts = 2;

void RepostingIdleTestTask(
    SingleThreadIdleTaskRunner* idle_task_runner,
    int* run_count,
    base::TimeTicks deadline) {
  if ((*run_count + 1) < max_idle_task_reposts) {
    idle_task_runner->PostIdleTask(
        FROM_HERE,
        base::Bind(&RepostingIdleTestTask,
                   base::Unretained(idle_task_runner), run_count));
  }
  (*run_count)++;
}

void UpdateClockToDeadlineIdleTestTask(
    scoped_refptr<cc::TestNowSource> clock,
    base::SingleThreadTaskRunner* task_runner,
    int* run_count,
    base::TimeTicks deadline) {
  clock->SetNow(deadline);
  // Due to the way in which OrderedSimpleTestRunner orders tasks and the fact
  // that we updated the time within a task, the delayed pending task to call
  // EndIdlePeriod will not happen until after a TaskQueueManager DoWork, so
  // post a normal task here to ensure it runs before the next idle task.
  task_runner->PostTask(FROM_HERE, base::Bind(NullTask));
  (*run_count)++;
}


};  // namespace

class SchedulerHelperForTest : public SchedulerHelper,
                               public SchedulerHelper::SchedulerHelperDelegate {
 public:
  explicit SchedulerHelperForTest(
      scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner)
      : SchedulerHelper(main_task_runner,
                        this,
                        "test.scheduler",
                        TRACE_DISABLED_BY_DEFAULT("test.scheduler"),
                        TASK_QUEUE_COUNT) {}

  ~SchedulerHelperForTest() override {}

  using SchedulerHelper::CanExceedIdleDeadlineIfRequired;
  using SchedulerHelper::EndIdlePeriod;
  using SchedulerHelper::StartIdlePeriod;
  using SchedulerHelper::InitiateLongIdlePeriod;

  // SchedulerHelperDelegate implementation:
  MOCK_METHOD2(CanEnterLongIdlePeriod,
               bool(base::TimeTicks now,
                    base::TimeDelta* next_long_idle_period_delay_out));
};

class SchedulerHelperTest : public testing::Test {
 public:
  SchedulerHelperTest()
      : clock_(cc::TestNowSource::Create(5000)),
        mock_task_runner_(new cc::OrderedSimpleTaskRunner(clock_, false)),
        nestable_task_runner_(
            NestableTaskRunnerForTest::Create(mock_task_runner_)),
        scheduler_helper_(new SchedulerHelperForTest(nestable_task_runner_)),
        default_task_runner_(scheduler_helper_->DefaultTaskRunner()),
        idle_task_runner_(scheduler_helper_->IdleTaskRunner()) {
    scheduler_helper_->SetTimeSourceForTesting(clock_);
  }

  SchedulerHelperTest(base::MessageLoop* message_loop)
      : clock_(cc::TestNowSource::Create(5000)),
        message_loop_(message_loop),
        nestable_task_runner_(
            SchedulerMessageLoopDelegate::Create(message_loop)),
        scheduler_helper_(new SchedulerHelperForTest(nestable_task_runner_)),
        default_task_runner_(scheduler_helper_->DefaultTaskRunner()),
        idle_task_runner_(scheduler_helper_->IdleTaskRunner()) {
    scheduler_helper_->SetTimeSourceForTesting(clock_);
  }
  ~SchedulerHelperTest() override {}

  void TearDown() override {
    DCHECK(!mock_task_runner_.get() || !message_loop_.get());
    if (mock_task_runner_.get()) {
      // Check that all tests stop posting tasks.
      mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
      while (mock_task_runner_->RunUntilIdle()) {
      }
    } else {
      message_loop_->RunUntilIdle();
    }
  }

  void RunUntilIdle() {
    // Only one of mock_task_runner_ or message_loop_ should be set.
    DCHECK(!mock_task_runner_.get() || !message_loop_.get());
    if (mock_task_runner_.get())
      mock_task_runner_->RunUntilIdle();
    else
      message_loop_->RunUntilIdle();
  }

  // Helper for posting several tasks of specific types. |task_descriptor| is a
  // string with space delimited task identifiers. The first letter of each
  // task identifier specifies the task type:
  // - 'D': Default task
  // - 'I': Idle task
  void PostTestTasks(std::vector<std::string>* run_order,
                     const std::string& task_descriptor) {
    std::istringstream stream(task_descriptor);
    while (!stream.eof()) {
      std::string task;
      stream >> task;
      switch (task[0]) {
        case 'D':
          default_task_runner_->PostTask(
              FROM_HERE, base::Bind(&AppendToVectorTestTask, run_order, task));
          break;
        case 'I':
          idle_task_runner_->PostIdleTask(
              FROM_HERE,
              base::Bind(&AppendToVectorIdleTestTask, run_order, task));
          break;
        default:
          NOTREACHED();
      }
    }
  }

 protected:
  static base::TimeDelta maximum_idle_period_duration() {
    return base::TimeDelta::FromMilliseconds(
        SchedulerHelper::kMaximumIdlePeriodMillis);
  }

  base::TimeTicks CurrentIdleTaskDeadlineForTesting() {
    base::TimeTicks deadline;
    scheduler_helper_->CurrentIdleTaskDeadlineCallback(&deadline);
    return deadline;
  }

  scoped_refptr<cc::TestNowSource> clock_;
  // Only one of mock_task_runner_ or message_loop_ will be set.
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_ptr<base::MessageLoop> message_loop_;

  scoped_refptr<NestableSingleThreadTaskRunner> nestable_task_runner_;
  scoped_ptr<SchedulerHelperForTest> scheduler_helper_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerHelperTest);
};

TEST_F(SchedulerHelperTest, TestPostDefaultTask) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 D2 D3 D4");

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("D2"),
                                   std::string("D3"), std::string("D4")));
}

TEST_F(SchedulerHelperTest, TestRentrantTask) {
  int count = 0;
  std::vector<int> run_order;
  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(AppendToVectorReentrantTask, default_task_runner_,
                            &run_order, &count, 5));
  RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST_F(SchedulerHelperTest, TestPostIdleTask) {
  int run_count = 0;
  base::TimeTicks expected_deadline =
      clock_->Now() + base::TimeDelta::FromMilliseconds(2300);
  base::TimeTicks deadline_in_task;

  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(100));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  scheduler_helper_->StartIdlePeriod(
      SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
      clock_->Now(),
      expected_deadline,
      true);
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(SchedulerHelperTest, TestPostIdleTask_EndIdlePeriod) {
  int run_count = 0;
  base::TimeTicks deadline_in_task;

  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(100));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  scheduler_helper_->StartIdlePeriod(
      SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
      clock_->Now(),
      clock_->Now() + base::TimeDelta::FromMilliseconds(10),
      true);
  scheduler_helper_->EndIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(0, run_count);
}

TEST_F(SchedulerHelperTest, TestRepostingIdleTask) {
  int run_count = 0;

  max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingIdleTestTask, idle_task_runner_, &run_count));
  scheduler_helper_->StartIdlePeriod(
      SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
      clock_->Now(),
      clock_->Now() + base::TimeDelta::FromMilliseconds(10),
      true);
  RunUntilIdle();
  EXPECT_EQ(1, run_count);

  // Reposted tasks shouldn't run until next idle period.
  RunUntilIdle();
  EXPECT_EQ(1, run_count);

  scheduler_helper_->StartIdlePeriod(
      SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
      clock_->Now(),
      clock_->Now() + base::TimeDelta::FromMilliseconds(10),
      true);
  RunUntilIdle();
  EXPECT_EQ(2, run_count);
}

TEST_F(SchedulerHelperTest, TestIdleTaskExceedsDeadline) {
  int run_count = 0;

  // Post two UpdateClockToDeadlineIdleTestTask tasks.
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&UpdateClockToDeadlineIdleTestTask, clock_,
                            default_task_runner_, &run_count));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&UpdateClockToDeadlineIdleTestTask, clock_,
                            default_task_runner_, &run_count));

  scheduler_helper_->StartIdlePeriod(
      SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
      clock_->Now(),
      clock_->Now() + base::TimeDelta::FromMilliseconds(10),
      true);
  RunUntilIdle();
  // Only the first idle task should execute since it's used up the deadline.
  EXPECT_EQ(1, run_count);

  scheduler_helper_->EndIdlePeriod();
  scheduler_helper_->StartIdlePeriod(
      SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
      clock_->Now(),
      clock_->Now() + base::TimeDelta::FromMilliseconds(10),
      true);
  RunUntilIdle();
  // Second task should be run on the next idle period.
  EXPECT_EQ(2, run_count);
}

TEST_F(SchedulerHelperTest, TestPostIdleTaskAfterWakeup) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  scheduler_helper_->StartIdlePeriod(
      SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
      clock_->Now(),
      clock_->Now() + base::TimeDelta::FromMilliseconds(10),
      true);
  RunUntilIdle();
  // Shouldn't run yet as no other task woke up the scheduler.
  EXPECT_EQ(0, run_count);

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  scheduler_helper_->StartIdlePeriod(
      SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
      clock_->Now(),
      clock_->Now() + base::TimeDelta::FromMilliseconds(10),
      true);
  RunUntilIdle();
  // Another after wakeup idle task shouldn't wake the scheduler.
  EXPECT_EQ(0, run_count);

  default_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));

  RunUntilIdle();
  // Must start a new idle period before idle task runs.
  scheduler_helper_->StartIdlePeriod(
      SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
      clock_->Now(),
      clock_->Now() + base::TimeDelta::FromMilliseconds(10),
      true);
  RunUntilIdle();
  // Execution of default task queue task should trigger execution of idle task.
  EXPECT_EQ(2, run_count);
}

TEST_F(SchedulerHelperTest, TestPostIdleTaskAfterWakeupWhileAwake) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  default_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));

  RunUntilIdle();
  // Must start a new idle period before idle task runs.
  scheduler_helper_->StartIdlePeriod(
      SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
      clock_->Now(),
      clock_->Now() + base::TimeDelta::FromMilliseconds(10),
      true);
  RunUntilIdle();
  // Should run as the scheduler was already awakened by the normal task.
  EXPECT_EQ(1, run_count);
}

TEST_F(SchedulerHelperTest, TestPostIdleTaskWakesAfterWakeupIdleTask) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  scheduler_helper_->StartIdlePeriod(
      SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
      clock_->Now(),
      clock_->Now() + base::TimeDelta::FromMilliseconds(10),
      true);
  RunUntilIdle();
  // Must start a new idle period before after-wakeup idle task runs.
  scheduler_helper_->StartIdlePeriod(
      SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
      clock_->Now(),
      clock_->Now() + base::TimeDelta::FromMilliseconds(10),
      true);
  RunUntilIdle();
  // Normal idle task should wake up after-wakeup idle task.
  EXPECT_EQ(2, run_count);
}

TEST_F(SchedulerHelperTest, TestDelayedEndIdlePeriod) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  TaskQueueManager* task_queue_manager =
      scheduler_helper_->SchedulerTaskQueueManager();

  EXPECT_CALL(*scheduler_helper_, CanEnterLongIdlePeriod(_, _))
      .Times(2)
      .WillRepeatedly(Return(true));

  // We need an idle task posted or InitiateLongIdlePeriod will use the
  // control_task_after_wakeup_runner_ instead of the control_task_runner_.
  idle_task_runner_->PostIdleTask(FROM_HERE, base::Bind(&NullIdleTask));
  scheduler_helper_->InitiateLongIdlePeriod();

  // Check there is a pending delayed task.
  EXPECT_GT(
      task_queue_manager->NextPendingDelayedTaskRunTime(), base::TimeTicks());

  RunUntilIdle();

  // If the delayed task ran, it will an InitiateLongIdlePeriod on the control
  // task after wake up queue.
  EXPECT_FALSE(task_queue_manager->IsQueueEmpty(
      SchedulerHelper::CONTROL_TASK_AFTER_WAKEUP_QUEUE));
}

TEST_F(SchedulerHelperTest, TestDelayedEndIdlePeriodCanceled) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  TaskQueueManager* task_queue_manager =
      scheduler_helper_->SchedulerTaskQueueManager();

  EXPECT_CALL(*scheduler_helper_, CanEnterLongIdlePeriod(_, _))
      .Times(1)
      .WillRepeatedly(Return(true));

  // We need an idle task posted or InitiateLongIdlePeriod will use the
  // control_task_after_wakeup_runner_ instead of the control_task_runner_.
  idle_task_runner_->PostIdleTask(FROM_HERE, base::Bind(&NullIdleTask));
  scheduler_helper_->InitiateLongIdlePeriod();

  // Check there is a pending delayed task.
  EXPECT_GT(
      task_queue_manager->NextPendingDelayedTaskRunTime(), base::TimeTicks());

  scheduler_helper_->EndIdlePeriod();
  RunUntilIdle();

  // If the delayed task didn't run, there will be nothing on the control task
  // after wake up queue.
  EXPECT_TRUE(task_queue_manager->IsQueueEmpty(
      SchedulerHelper::CONTROL_TASK_AFTER_WAKEUP_QUEUE));
}

class SchedulerHelperWithMessageLoopTest : public SchedulerHelperTest {
 public:
  SchedulerHelperWithMessageLoopTest()
      : SchedulerHelperTest(new base::MessageLoop()) {}
  ~SchedulerHelperWithMessageLoopTest() override {}

  void PostFromNestedRunloop(std::vector<
      std::pair<SingleThreadIdleTaskRunner::IdleTask, bool>>* tasks) {
    base::MessageLoop::ScopedNestableTaskAllower allow(message_loop_.get());
    for (std::pair<SingleThreadIdleTaskRunner::IdleTask, bool>& pair : *tasks) {
      if (pair.second) {
        idle_task_runner_->PostIdleTask(FROM_HERE, pair.first);
      } else {
        idle_task_runner_->PostNonNestableIdleTask(FROM_HERE, pair.first);
      }
    }
    scheduler_helper_->StartIdlePeriod(
        SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
        clock_->Now(),
        clock_->Now() + base::TimeDelta::FromMilliseconds(10),
        true);
    message_loop_->RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SchedulerHelperWithMessageLoopTest);
};

TEST_F(SchedulerHelperWithMessageLoopTest,
       NonNestableIdleTaskDoesntExecuteInNestedLoop) {
  std::vector<std::string> order;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&AppendToVectorIdleTestTask, &order, std::string("1")));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&AppendToVectorIdleTestTask, &order, std::string("2")));

  std::vector<std::pair<SingleThreadIdleTaskRunner::IdleTask, bool>>
      tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::Bind(&AppendToVectorIdleTestTask, &order, std::string("3")),
      false));
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::Bind(&AppendToVectorIdleTestTask, &order, std::string("4")), true));
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::Bind(&AppendToVectorIdleTestTask, &order, std::string("5")), true));

  default_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SchedulerHelperWithMessageLoopTest::PostFromNestedRunloop,
                 base::Unretained(this),
                 base::Unretained(&tasks_to_post_from_nested_loop)));

  scheduler_helper_->StartIdlePeriod(
      SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
      clock_->Now(),
      clock_->Now() + base::TimeDelta::FromMilliseconds(10),
      true);
  RunUntilIdle();
  // Note we expect task 3 to run last because it's non-nestable.
  EXPECT_THAT(order, testing::ElementsAre(std::string("1"), std::string("2"),
                                          std::string("4"), std::string("5"),
                                          std::string("3")));
}

TEST_F(SchedulerHelperTest, TestLongIdlePeriod) {
  base::TimeTicks expected_deadline =
      clock_->Now() + maximum_idle_period_duration();
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  EXPECT_CALL(*scheduler_helper_, CanEnterLongIdlePeriod(_, _))
      .Times(2)
      .WillRepeatedly(Return(true));

  RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run yet as no idle period.

  scheduler_helper_->InitiateLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(SchedulerHelperTest, TestLongIdlePeriodWithPendingDelayedTask) {
  base::TimeDelta pending_task_delay = base::TimeDelta::FromMilliseconds(30);
  base::TimeTicks expected_deadline = clock_->Now() + pending_task_delay;
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  EXPECT_CALL(*scheduler_helper_, CanEnterLongIdlePeriod(_, _))
      .Times(2)
      .WillRepeatedly(Return(true));

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        pending_task_delay);

  scheduler_helper_->InitiateLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(SchedulerHelperTest, TestLongIdlePeriodWithLatePendingDelayedTask) {
  base::TimeDelta pending_task_delay = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  EXPECT_CALL(*scheduler_helper_, CanEnterLongIdlePeriod(_, _))
      .Times(3)
      .WillRepeatedly(Return(true));

  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        pending_task_delay);

  // Advance clock until after delayed task was meant to be run.
  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(20));

  // Post an idle task and then InitiateLongIdlePeriod. Since there is a late
  // pending delayed task this shouldn't actually start an idle period.
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  scheduler_helper_->InitiateLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // After the delayed task has been run we should trigger an idle period.
  clock_->AdvanceNow(maximum_idle_period_duration());
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
}

TEST_F(SchedulerHelperTest, TestLongIdlePeriodRepeating) {
  int run_count = 0;

  EXPECT_CALL(*scheduler_helper_, CanEnterLongIdlePeriod(_, _))
      .Times(2)
      .WillRepeatedly(Return(true));

  max_idle_task_reposts = 3;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingIdleTestTask, idle_task_runner_, &run_count));

  scheduler_helper_->InitiateLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should only run once per idle period.

  // Advance time to start of next long idle period and check task reposted task
  // gets run.
  clock_->AdvanceNow(maximum_idle_period_duration());
  RunUntilIdle();
  EXPECT_EQ(2, run_count);

  // Advance time to start of next long idle period then end the idle period and
  // check the task doesn't get run.
  clock_->AdvanceNow(maximum_idle_period_duration());
  scheduler_helper_->EndIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(2, run_count);
}

TEST_F(SchedulerHelperTest, TestLongIdlePeriodDoesNotWakeScheduler) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  EXPECT_CALL(*scheduler_helper_, CanEnterLongIdlePeriod(_, _))
      .Times(3)
      .WillRepeatedly(Return(true));

  // Start a long idle period and get the time it should end.
  scheduler_helper_->InitiateLongIdlePeriod();
  // The scheduler should not run the initiate_next_long_idle_period task if
  // there are no idle tasks and no other task woke up the scheduler, thus
  // the idle period deadline shouldn't update at the end of the current long
  // idle period.
  base::TimeTicks idle_period_deadline = CurrentIdleTaskDeadlineForTesting();
  clock_->AdvanceNow(maximum_idle_period_duration());
  RunUntilIdle();

  base::TimeTicks new_idle_period_deadline =
      CurrentIdleTaskDeadlineForTesting();
  EXPECT_EQ(idle_period_deadline, new_idle_period_deadline);

  // Posting a after-wakeup idle task also shouldn't wake the scheduler or
  // initiate the next long idle period.
  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  RunUntilIdle();
  new_idle_period_deadline = CurrentIdleTaskDeadlineForTesting();
  EXPECT_EQ(idle_period_deadline, new_idle_period_deadline);
  EXPECT_EQ(0, run_count);

  // Running a normal task should initiate a new long idle period though.
  default_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));
  RunUntilIdle();
  new_idle_period_deadline = CurrentIdleTaskDeadlineForTesting();
  EXPECT_EQ(idle_period_deadline + maximum_idle_period_duration(),
            new_idle_period_deadline);

  EXPECT_EQ(1, run_count);
}

TEST_F(SchedulerHelperTest, TestLongIdlePeriodWhenNotCanEnterLongIdlePeriod) {
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(1000);
  base::TimeDelta halfDelay = base::TimeDelta::FromMilliseconds(500);
  base::TimeTicks delayOver = clock_->Now() + delay;
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  ON_CALL(*scheduler_helper_, CanEnterLongIdlePeriod(_, _))
      .WillByDefault(Invoke([delay, delayOver](
          base::TimeTicks now,
          base::TimeDelta* next_long_idle_period_delay_out) {
        if (now >= delayOver)
          return true;
        *next_long_idle_period_delay_out = delay;
        return false;
      }));

  EXPECT_CALL(*scheduler_helper_, CanEnterLongIdlePeriod(_, _)).Times(3);

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  // Make sure Idle tasks don't run until the delay has occured.
  scheduler_helper_->InitiateLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  clock_->AdvanceNow(halfDelay);
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // Delay is finished, idle task should run.
  clock_->AdvanceNow(halfDelay);
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
}

void TestCanExceedIdleDeadlineIfRequiredTask(SchedulerHelperForTest* scheduler,
                                             bool* can_exceed_idle_deadline_out,
                                             int* run_count,
                                             base::TimeTicks deadline) {
  *can_exceed_idle_deadline_out = scheduler->CanExceedIdleDeadlineIfRequired();
  (*run_count)++;
}

TEST_F(SchedulerHelperTest, CanExceedIdleDeadlineIfRequired) {
  int run_count = 0;
  bool can_exceed_idle_deadline = false;

  EXPECT_CALL(*scheduler_helper_, CanEnterLongIdlePeriod(_, _))
      .Times(3)
      .WillRepeatedly(Return(true));

  // Should return false if not in an idle period.
  EXPECT_FALSE(scheduler_helper_->CanExceedIdleDeadlineIfRequired());

  // Should return false for short idle periods.
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&TestCanExceedIdleDeadlineIfRequiredTask,
                            scheduler_helper_.get(), &can_exceed_idle_deadline,
                            &run_count));
  scheduler_helper_->StartIdlePeriod(
      SchedulerHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD,
      clock_->Now(),
      clock_->Now() + base::TimeDelta::FromMilliseconds(10),
      true);
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_FALSE(can_exceed_idle_deadline);

  // Should return false for a long idle period which is shortened due to a
  // pending delayed task.
  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        base::TimeDelta::FromMilliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&TestCanExceedIdleDeadlineIfRequiredTask,
                            scheduler_helper_.get(), &can_exceed_idle_deadline,
                            &run_count));
  scheduler_helper_->InitiateLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(2, run_count);
  EXPECT_FALSE(can_exceed_idle_deadline);

  // Next long idle period will be for the maximum time, so
  // CanExceedIdleDeadlineIfRequired should return true.
  clock_->AdvanceNow(maximum_idle_period_duration());
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&TestCanExceedIdleDeadlineIfRequiredTask,
                            scheduler_helper_.get(), &can_exceed_idle_deadline,
                            &run_count));
  RunUntilIdle();
  EXPECT_EQ(3, run_count);
  EXPECT_TRUE(can_exceed_idle_deadline);
}

TEST_F(SchedulerHelperTest, IsShutdown) {
  EXPECT_FALSE(scheduler_helper_->IsShutdown());

  scheduler_helper_->Shutdown();
  EXPECT_TRUE(scheduler_helper_->IsShutdown());
}

}  // namespace content
