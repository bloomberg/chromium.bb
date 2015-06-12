// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/renderer_scheduler_impl.h"

#include "base/callback.h"
#include "cc/output/begin_frame_args.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "cc/test/test_now_source.h"
#include "components/scheduler/child/nestable_task_runner_for_test.h"
#include "components/scheduler/child/scheduler_message_loop_delegate.h"
#include "components/scheduler/child/test_time_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scheduler {

namespace {
class FakeInputEvent : public blink::WebInputEvent {
 public:
  explicit FakeInputEvent(blink::WebInputEvent::Type event_type)
      : WebInputEvent(sizeof(FakeInputEvent)) {
    type = event_type;
  }

  FakeInputEvent(blink::WebInputEvent::Type event_type, int event_modifiers)
      : WebInputEvent(sizeof(FakeInputEvent)) {
    type = event_type;
    modifiers = event_modifiers;
  }
};

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

void AppendToVectorReentrantTask(base::SingleThreadTaskRunner* task_runner,
                                 std::vector<int>* vector,
                                 int* reentrant_count,
                                 int max_reentrant_count) {
  vector->push_back((*reentrant_count)++);
  if (*reentrant_count < max_reentrant_count) {
    task_runner->PostTask(
        FROM_HERE,
        base::Bind(AppendToVectorReentrantTask, base::Unretained(task_runner),
                   vector, reentrant_count, max_reentrant_count));
  }
}

void IdleTestTask(int* run_count,
                  base::TimeTicks* deadline_out,
                  base::TimeTicks deadline) {
  (*run_count)++;
  *deadline_out = deadline;
}

int max_idle_task_reposts = 2;

void RepostingIdleTestTask(SingleThreadIdleTaskRunner* idle_task_runner,
                           int* run_count,
                           base::TimeTicks deadline) {
  if ((*run_count + 1) < max_idle_task_reposts) {
    idle_task_runner->PostIdleTask(
        FROM_HERE, base::Bind(&RepostingIdleTestTask,
                              base::Unretained(idle_task_runner), run_count));
  }
  (*run_count)++;
}

void RepostingUpdateClockIdleTestTask(
    SingleThreadIdleTaskRunner* idle_task_runner,
    int* run_count,
    scoped_refptr<cc::TestNowSource> clock,
    base::TimeDelta advance_time,
    std::vector<base::TimeTicks>* deadlines,
    base::TimeTicks deadline) {
  if ((*run_count + 1) < max_idle_task_reposts) {
    idle_task_runner->PostIdleTask(
        FROM_HERE, base::Bind(&RepostingUpdateClockIdleTestTask,
                              base::Unretained(idle_task_runner), run_count,
                              clock, advance_time, deadlines));
  }
  deadlines->push_back(deadline);
  (*run_count)++;
  clock->AdvanceNow(advance_time);
}

void WillBeginFrameIdleTask(RendererScheduler* scheduler,
                            scoped_refptr<cc::TestNowSource> clock,
                            base::TimeTicks deadline) {
  scheduler->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock->Now(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL));
}

void UpdateClockToDeadlineIdleTestTask(
    cc::TestNowSource* clock,
    int* run_count,
    base::TimeTicks deadline) {
  clock->SetNow(deadline);
  (*run_count)++;
}

void PostingYieldingTestTask(RendererSchedulerImpl* scheduler,
                             base::SingleThreadTaskRunner* task_runner,
                             bool simulate_input,
                             bool* should_yield_before,
                             bool* should_yield_after) {
  *should_yield_before = scheduler->ShouldYieldForHighPriorityWork();
  task_runner->PostTask(FROM_HERE, base::Bind(NullTask));
  if (simulate_input) {
    scheduler->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureFlingStart),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  }
  *should_yield_after = scheduler->ShouldYieldForHighPriorityWork();
}

void AnticipationTestTask(RendererSchedulerImpl* scheduler,
                          bool simulate_input,
                          bool* is_anticipated_before,
                          bool* is_anticipated_after) {
  *is_anticipated_before = scheduler->IsHighPriorityWorkAnticipated();
  if (simulate_input) {
    scheduler->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureFlingStart),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  }
  *is_anticipated_after = scheduler->IsHighPriorityWorkAnticipated();
}
};  // namespace

class RendererSchedulerImplForTest : public RendererSchedulerImpl {
 public:
  using RendererSchedulerImpl::OnIdlePeriodEnded;
  using RendererSchedulerImpl::OnIdlePeriodStarted;
  using RendererSchedulerImpl::Policy;
  using RendererSchedulerImpl::PolicyToString;

  RendererSchedulerImplForTest(
      scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner)
      : RendererSchedulerImpl(main_task_runner), update_policy_count_(0) {}

  void UpdatePolicyLocked(UpdateType update_type) override {
    update_policy_count_++;
    RendererSchedulerImpl::UpdatePolicyLocked(update_type);
  }

  void EnsureUrgentPolicyUpdatePostedOnMainThread() {
    base::AutoLock lock(any_thread_lock_);
    RendererSchedulerImpl::EnsureUrgentPolicyUpdatePostedOnMainThread(
        FROM_HERE);
  }

  void ScheduleDelayedPolicyUpdate(base::TimeTicks now, base::TimeDelta delay) {
    delayed_update_policy_runner_.SetDeadline(FROM_HERE, delay, now);
  }

  bool BeginMainFrameOnCriticalPath() {
    base::AutoLock lock(any_thread_lock_);
    return AnyThread().begin_main_frame_on_critical_path_;
  }

  int update_policy_count_;
};

// Lets gtest print human readable Policy values.
::std::ostream& operator<<(::std::ostream& os,
                           const RendererSchedulerImplForTest::Policy& policy) {
  return os << RendererSchedulerImplForTest::PolicyToString(policy);
}

class RendererSchedulerImplTest : public testing::Test {
 public:
  using Policy = RendererSchedulerImpl::Policy;

  RendererSchedulerImplTest() : clock_(cc::TestNowSource::Create(5000)) {}

  RendererSchedulerImplTest(base::MessageLoop* message_loop)
      : clock_(cc::TestNowSource::Create(5000)), message_loop_(message_loop) {}

  ~RendererSchedulerImplTest() override {}

  void SetUp() override {
    if (message_loop_) {
      nestable_task_runner_ =
          SchedulerMessageLoopDelegate::Create(message_loop_.get());
    } else {
      mock_task_runner_ =
          make_scoped_refptr(new cc::OrderedSimpleTaskRunner(clock_, false));
      nestable_task_runner_ =
          NestableTaskRunnerForTest::Create(mock_task_runner_);
    }
    Initialize(make_scoped_ptr(
        new RendererSchedulerImplForTest(nestable_task_runner_)));
  }

  void Initialize(scoped_ptr<RendererSchedulerImplForTest> scheduler) {
    scheduler_ = scheduler.Pass();
    default_task_runner_ = scheduler_->DefaultTaskRunner();
    compositor_task_runner_ = scheduler_->CompositorTaskRunner();
    loading_task_runner_ = scheduler_->LoadingTaskRunner();
    idle_task_runner_ = scheduler_->IdleTaskRunner();
    timer_task_runner_ = scheduler_->TimerTaskRunner();
    scheduler_->GetSchedulerHelperForTesting()->SetTimeSourceForTesting(
        make_scoped_ptr(new TestTimeSource(clock_)));
    scheduler_->GetSchedulerHelperForTesting()
        ->GetTaskQueueManagerForTesting()
        ->SetTimeSourceForTesting(make_scoped_ptr(new TestTimeSource(clock_)));
  }

  void TearDown() override {
    DCHECK(!mock_task_runner_.get() || !message_loop_.get());
    scheduler_->Shutdown();
    if (mock_task_runner_.get()) {
      // Check that all tests stop posting tasks.
      mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
      while (mock_task_runner_->RunUntilIdle()) {
      }
    } else {
      message_loop_->RunUntilIdle();
    }
    scheduler_.reset();
  }

  void RunUntilIdle() {
    // Only one of mock_task_runner_ or message_loop_ should be set.
    DCHECK(!mock_task_runner_.get() || !message_loop_.get());
    if (mock_task_runner_.get())
      mock_task_runner_->RunUntilIdle();
    else
      message_loop_->RunUntilIdle();
  }

  void DoMainFrame() {
    scheduler_->WillBeginFrame(cc::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, clock_->Now(), base::TimeTicks(),
        base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL));
    scheduler_->DidCommitFrameToCompositor();
  }

  void EnableIdleTasks() { DoMainFrame(); }

  Policy CurrentPolicy() {
    return scheduler_->MainThreadOnly().current_policy_;
  }

  // Helper for posting several tasks of specific types. |task_descriptor| is a
  // string with space delimited task identifiers. The first letter of each
  // task identifier specifies the task type:
  // - 'D': Default task
  // - 'C': Compositor task
  // - 'L': Loading task
  // - 'I': Idle task
  // - 'T': Timer task
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
        case 'C':
          compositor_task_runner_->PostTask(
              FROM_HERE, base::Bind(&AppendToVectorTestTask, run_order, task));
          break;
        case 'L':
          loading_task_runner_->PostTask(
              FROM_HERE, base::Bind(&AppendToVectorTestTask, run_order, task));
          break;
        case 'I':
          idle_task_runner_->PostIdleTask(
              FROM_HERE,
              base::Bind(&AppendToVectorIdleTestTask, run_order, task));
          break;
        case 'T':
          timer_task_runner_->PostTask(
              FROM_HERE, base::Bind(&AppendToVectorTestTask, run_order, task));
          break;
        default:
          NOTREACHED();
      }
    }
  }

 protected:
  static base::TimeDelta priority_escalation_after_input_duration() {
    return base::TimeDelta::FromMilliseconds(
        RendererSchedulerImpl::kPriorityEscalationAfterInputMillis);
  }

  static base::TimeDelta maximum_idle_period_duration() {
    return base::TimeDelta::FromMilliseconds(
        IdleHelper::kMaximumIdlePeriodMillis);
  }

  static base::TimeDelta end_idle_when_hidden_delay() {
    return base::TimeDelta::FromMilliseconds(
        RendererSchedulerImpl::kEndIdleWhenHiddenDelayMillis);
  }

  static base::TimeDelta idle_period_starvation_threshold() {
    return base::TimeDelta::FromMilliseconds(
        RendererSchedulerImpl::kIdlePeriodStarvationThresholdMillis);
  }

  template <typename E>
  static void CallForEachEnumValue(E first,
                                   E last,
                                   const char* (*function)(E)) {
    for (E val = first; val < last;
         val = static_cast<E>(static_cast<int>(val) + 1)) {
      (*function)(val);
    }
  }

  static void CheckAllTaskQueueIdToString() {
    CallForEachEnumValue<RendererSchedulerImpl::QueueId>(
        RendererSchedulerImpl::FIRST_QUEUE_ID,
        RendererSchedulerImpl::TASK_QUEUE_COUNT,
        &RendererSchedulerImpl::TaskQueueIdToString);
  }

  static void CheckAllPolicyToString() {
    CallForEachEnumValue<RendererSchedulerImpl::Policy>(
        RendererSchedulerImpl::Policy::FIRST_POLICY,
        RendererSchedulerImpl::Policy::POLICY_COUNT,
        &RendererSchedulerImpl::PolicyToString);
  }

  scoped_refptr<cc::TestNowSource> clock_;
  // Only one of mock_task_runner_ or message_loop_ will be set.
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_ptr<base::MessageLoop> message_loop_;

  scoped_refptr<NestableSingleThreadTaskRunner> nestable_task_runner_;
  scoped_ptr<RendererSchedulerImplForTest> scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(RendererSchedulerImplTest);
};

TEST_F(RendererSchedulerImplTest, TestPostDefaultTask) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 D2 D3 D4");

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("D2"),
                                   std::string("D3"), std::string("D4")));
}

TEST_F(RendererSchedulerImplTest, TestPostDefaultAndCompositor) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 C1");
  RunUntilIdle();
  EXPECT_THAT(run_order, testing::Contains("D1"));
  EXPECT_THAT(run_order, testing::Contains("C1"));
}

TEST_F(RendererSchedulerImplTest, TestRentrantTask) {
  int count = 0;
  std::vector<int> run_order;
  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(AppendToVectorReentrantTask, default_task_runner_,
                            &run_order, &count, 5));
  RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST_F(RendererSchedulerImplTest, TestPostIdleTask) {
  int run_count = 0;
  base::TimeTicks expected_deadline =
      clock_->Now() + base::TimeDelta::FromMilliseconds(2300);
  base::TimeTicks deadline_in_task;

  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(100));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run yet as no WillBeginFrame.

  scheduler_->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->Now(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL));
  RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run as no DidCommitFrameToCompositor.

  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(1200));
  scheduler_->DidCommitFrameToCompositor();
  RunUntilIdle();
  EXPECT_EQ(0, run_count);  // We missed the deadline.

  scheduler_->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->Now(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL));
  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(800));
  scheduler_->DidCommitFrameToCompositor();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(RendererSchedulerImplTest, TestRepostingIdleTask) {
  int run_count = 0;

  max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingIdleTestTask, idle_task_runner_, &run_count));
  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);

  // Reposted tasks shouldn't run until next idle period.
  RunUntilIdle();
  EXPECT_EQ(1, run_count);

  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_EQ(2, run_count);
}

TEST_F(RendererSchedulerImplTest, TestIdleTaskExceedsDeadline) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  int run_count = 0;

  // Post two UpdateClockToDeadlineIdleTestTask tasks.
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&UpdateClockToDeadlineIdleTestTask, clock_, &run_count));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&UpdateClockToDeadlineIdleTestTask, clock_, &run_count));

  EnableIdleTasks();
  RunUntilIdle();
  // Only the first idle task should execute since it's used up the deadline.
  EXPECT_EQ(1, run_count);

  EnableIdleTasks();
  RunUntilIdle();
  // Second task should be run on the next idle period.
  EXPECT_EQ(2, run_count);
}

TEST_F(RendererSchedulerImplTest, TestPostIdleTaskAfterWakeup) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  EnableIdleTasks();
  RunUntilIdle();
  // Shouldn't run yet as no other task woke up the scheduler.
  EXPECT_EQ(0, run_count);

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  EnableIdleTasks();
  RunUntilIdle();
  // Another after wakeup idle task shouldn't wake the scheduler.
  EXPECT_EQ(0, run_count);

  default_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));

  RunUntilIdle();
  EnableIdleTasks();  // Must start a new idle period before idle task runs.
  RunUntilIdle();
  // Execution of default task queue task should trigger execution of idle task.
  EXPECT_EQ(2, run_count);
}

TEST_F(RendererSchedulerImplTest, TestPostIdleTaskAfterWakeupWhileAwake) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  default_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));

  RunUntilIdle();
  EnableIdleTasks();  // Must start a new idle period before idle task runs.
  RunUntilIdle();
  // Should run as the scheduler was already awakened by the normal task.
  EXPECT_EQ(1, run_count);
}

TEST_F(RendererSchedulerImplTest, TestPostIdleTaskWakesAfterWakeupIdleTask) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  EnableIdleTasks();
  RunUntilIdle();
  // Must start a new idle period before after-wakeup idle task runs.
  EnableIdleTasks();
  RunUntilIdle();
  // Normal idle task should wake up after-wakeup idle task.
  EXPECT_EQ(2, run_count);
}

TEST_F(RendererSchedulerImplTest, TestDelayedEndIdlePeriodCanceled) {
  int run_count = 0;

  base::TimeTicks deadline_in_task;
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  // Trigger the beginning of an idle period for 1000ms.
  scheduler_->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->Now(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL));
  DoMainFrame();

  // End the idle period early (after 500ms), and send a WillBeginFrame which
  // specifies that the next idle period should end 1000ms from now.
  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(500));
  scheduler_->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->Now(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL));

  RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Not currently in an idle period.

  // Trigger the start of the idle period before the task to end the previous
  // idle period has been triggered.
  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(400));
  scheduler_->DidCommitFrameToCompositor();

  // Post a task which simulates running until after the previous end idle
  // period delayed task was scheduled for
  scheduler_->DefaultTaskRunner()->PostTask(FROM_HERE, base::Bind(NullTask));
  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(300));

  RunUntilIdle();
  EXPECT_EQ(1, run_count);  // We should still be in the new idle period.
}

TEST_F(RendererSchedulerImplTest, TestDefaultPolicy) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("C1"), std::string("D2"),
                                   std::string("C2"), std::string("I1")));
}

TEST_F(RendererSchedulerImplTest, TestCompositorPolicy_CompositorHandlesInput) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("L1"), std::string("I1")));
}

TEST_F(RendererSchedulerImplTest, TestCompositorPolicy_MainThreadHandlesInput) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("L1"), std::string("I1")));
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart));
}

TEST_F(RendererSchedulerImplTest, TestCompositorPolicy_DidAnimateForInput) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  scheduler_->DidAnimateForInputOnCompositorThread();
  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicy_TimersOnlyRunWhenIdle_MainThreadOnCriticalPath) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "C1 T1");

  scheduler_->DidAnimateForInputOnCompositorThread();
  scheduler_->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->Now(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL));
  scheduler_->DidCommitFrameToCompositor();  // Starts Idle Period
  RunUntilIdle();

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("T1")));

  // End the idle period.
  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(500));
  scheduler_->DidAnimateForInputOnCompositorThread();
  scheduler_->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->Now(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL));

  run_order.clear();
  PostTestTasks(&run_order, "C1 T1");
  RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre(std::string("C1")));
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicy_TimersAlwaysRunIfNoRecentIdlePeriod) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "C1 T1");

  // Simulate no recent idle period.
  clock_->AdvanceNow(idle_period_starvation_threshold() * 2);

  scheduler_->DidAnimateForInputOnCompositorThread();
  scheduler_->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->Now(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL));

  RunUntilIdle();

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("T1")));
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicy_TimersAlwaysRun_MainThreadNotOnCriticalPath) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "C1 T1");

  scheduler_->DidAnimateForInputOnCompositorThread();
  cc::BeginFrameArgs begin_frame_args1 = cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->Now(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL);
  begin_frame_args1.on_critical_path = false;
  scheduler_->WillBeginFrame(begin_frame_args1);
  scheduler_->DidCommitFrameToCompositor();  // Starts Idle Period
  RunUntilIdle();

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("T1")));

  // End the idle period.
  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(500));
  scheduler_->DidAnimateForInputOnCompositorThread();
  cc::BeginFrameArgs begin_frame_args2 = cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->Now(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL);
  begin_frame_args2.on_critical_path = false;
  scheduler_->WillBeginFrame(begin_frame_args2);

  run_order.clear();
  PostTestTasks(&run_order, "C1 T1");
  RunUntilIdle();

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("T1")));
}

TEST_F(RendererSchedulerImplTest, TestTouchstartPolicy_Compositor) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2 T1 T2");

  // Observation of touchstart should defer execution of timer, idle and loading
  // tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2")));

  // Meta events like TapDown/FlingCancel shouldn't affect the priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingCancel),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureTapDown),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Action events like ScrollBegin will kick us back into compositor priority,
  // allowing service of the timer, loading and idle queues.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureScrollBegin),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T1"), std::string("T2"),
                                   std::string("L1")));
}

TEST_F(RendererSchedulerImplTest, TestTouchstartPolicy_MainThread) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2 T1 T2");

  // Observation of touchstart should defer execution of timer, idle and loading
  // tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart));
  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2")));

  // Meta events like TapDown/FlingCancel shouldn't affect the priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingCancel),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingCancel));
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureTapDown),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::GestureTapDown));
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Action events like ScrollBegin will kick us back into compositor priority,
  // allowing service of the timer, loading and idle queues.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureScrollBegin),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::GestureScrollBegin));
  RunUntilIdle();

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T1"), std::string("T2"),
                                   std::string("L1")));
}

TEST_F(RendererSchedulerImplTest, LoadingPriorityPolicy) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->OnPageLoadStarted();
  EnableIdleTasks();
  RunUntilIdle();
  // In loading policy compositor tasks are best effort and should be run last.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("D2"), std::string("I1"),
                                   std::string("C1"), std::string("C2")));

  // Advance 1.5s and try again, the loading policy should have ended and the
  // task order should return to normal.
  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(1500));
  run_order.clear();
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");
  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("C1"), std::string("D2"),
                                   std::string("C2"), std::string("I1")));
}

TEST_F(RendererSchedulerImplTest,
       EventConsumedOnCompositorThread_IgnoresMouseMove_WhenMouseUp) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::MouseMove),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EnableIdleTasks();
  RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2"),
                                   std::string("I1")));
}

TEST_F(RendererSchedulerImplTest,
       EventForwardedToMainThread_IgnoresMouseMove_WhenMouseUp) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::MouseMove),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EnableIdleTasks();
  RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2"),
                                   std::string("I1")));
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::MouseMove));
}

TEST_F(RendererSchedulerImplTest,
       EventConsumedOnCompositorThread_MouseMove_WhenMouseDown) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::MouseMove,
                     blink::WebInputEvent::LeftButtonDown),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EnableIdleTasks();
  RunUntilIdle();
  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
}

TEST_F(RendererSchedulerImplTest,
       EventForwardedToMainThread_MouseMove_WhenMouseDown) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::MouseMove,
                     blink::WebInputEvent::LeftButtonDown),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EnableIdleTasks();
  RunUntilIdle();
  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
  scheduler_->DidHandleInputEventOnMainThread(FakeInputEvent(
      blink::WebInputEvent::MouseMove, blink::WebInputEvent::LeftButtonDown));
}

TEST_F(RendererSchedulerImplTest, EventConsumedOnCompositorThread_MouseWheel) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::MouseWheel),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EnableIdleTasks();
  RunUntilIdle();
  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
}

TEST_F(RendererSchedulerImplTest, EventForwardedToMainThread_MouseWheel) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::MouseWheel),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EnableIdleTasks();
  RunUntilIdle();
  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::MouseWheel));
}

TEST_F(RendererSchedulerImplTest,
       EventConsumedOnCompositorThread_IgnoresKeyboardEvents) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::KeyDown),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EnableIdleTasks();
  RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2"),
                                   std::string("I1")));
}

TEST_F(RendererSchedulerImplTest,
       EventForwardedToMainThread_IgnoresKeyboardEvents) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::KeyDown),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EnableIdleTasks();
  RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2"),
                                   std::string("I1")));
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::KeyDown));
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicyDoesNotStarveDefaultTasks) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 C1");

  for (int i = 0; i < 20; i++) {
    compositor_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));
  }
  PostTestTasks(&run_order, "C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  // Ensure that the default D1 task gets to run at some point before the final
  // C2 compositor task.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("D1"),
                                   std::string("C2")));
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicyEnds_CompositorHandlesInput) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2")));

  run_order.clear();
  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(1000));
  PostTestTasks(&run_order, "D1 C1 D2 C2");

  // Compositor policy mode should have ended now that the clock has advanced.
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2")));
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicyEnds_MainThreadHandlesInput) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart));
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2")));

  run_order.clear();
  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(1000));
  PostTestTasks(&run_order, "D1 C1 D2 C2");

  // Compositor policy mode should have ended now that the clock has advanced.
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2")));
}

TEST_F(RendererSchedulerImplTest, TestTouchstartPolicyEndsAfterTimeout) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2")));

  run_order.clear();
  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(1000));

  // Don't post any compositor tasks to simulate a very long running event
  // handler.
  PostTestTasks(&run_order, "D1 D2");

  // Touchstart policy mode should have ended now that the clock has advanced.
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("D2")));
}

TEST_F(RendererSchedulerImplTest,
       TestTouchstartPolicyEndsAfterConsecutiveTouchmoves) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2");

  // Observation of touchstart should defer execution of idle and loading tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2")));

  // Receiving the first touchmove will not affect scheduler priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Receiving the second touchmove will kick us back into compositor priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("L1")));
}

TEST_F(RendererSchedulerImplTest, TestIsHighPriorityWorkAnticipated) {
  bool is_anticipated_before = false;
  bool is_anticipated_after = false;

  bool simulate_input = false;
  default_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AnticipationTestTask, scheduler_.get(), simulate_input,
                 &is_anticipated_before, &is_anticipated_after));
  RunUntilIdle();
  // In its default state, without input receipt, the scheduler should indicate
  // that no high-priority is anticipated.
  EXPECT_FALSE(is_anticipated_before);
  EXPECT_FALSE(is_anticipated_after);

  simulate_input = true;
  default_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AnticipationTestTask, scheduler_.get(), simulate_input,
                 &is_anticipated_before, &is_anticipated_after));
  RunUntilIdle();
  // When input is received, the scheduler should indicate that high-priority
  // work is anticipated.
  EXPECT_FALSE(is_anticipated_before);
  EXPECT_TRUE(is_anticipated_after);

  clock_->AdvanceNow(priority_escalation_after_input_duration() * 2);
  simulate_input = false;
  default_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AnticipationTestTask, scheduler_.get(), simulate_input,
                 &is_anticipated_before, &is_anticipated_after));
  RunUntilIdle();
  // Without additional input, the scheduler should indicate that high-priority
  // work is no longer anticipated.
  EXPECT_FALSE(is_anticipated_before);
  EXPECT_FALSE(is_anticipated_after);
}

TEST_F(RendererSchedulerImplTest, TestShouldYield) {
  bool should_yield_before = false;
  bool should_yield_after = false;

  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PostingYieldingTestTask, scheduler_.get(),
                            default_task_runner_, false, &should_yield_before,
                            &should_yield_after));
  RunUntilIdle();
  // Posting to default runner shouldn't cause yielding.
  EXPECT_FALSE(should_yield_before);
  EXPECT_FALSE(should_yield_after);

  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PostingYieldingTestTask, scheduler_.get(),
                            compositor_task_runner_, false,
                            &should_yield_before, &should_yield_after));
  RunUntilIdle();
  // Posting while not in compositor priority shouldn't cause yielding.
  EXPECT_FALSE(should_yield_before);
  EXPECT_FALSE(should_yield_after);

  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PostingYieldingTestTask, scheduler_.get(),
                            compositor_task_runner_, true, &should_yield_before,
                            &should_yield_after));
  RunUntilIdle();
  // We should be able to switch to compositor priority mid-task.
  EXPECT_FALSE(should_yield_before);
  EXPECT_TRUE(should_yield_after);

  // Receiving a touchstart should immediately trigger yielding, even if
  // there's no immediately pending work in the compositor queue.
  EXPECT_FALSE(scheduler_->ShouldYieldForHighPriorityWork());
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EXPECT_TRUE(scheduler_->ShouldYieldForHighPriorityWork());
  RunUntilIdle();
}

TEST_F(RendererSchedulerImplTest, SlowMainThreadInputEvent) {
  EXPECT_EQ(Policy::NORMAL, CurrentPolicy());

  // An input event should bump us into input priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  RunUntilIdle();
  EXPECT_EQ(Policy::COMPOSITOR_PRIORITY, CurrentPolicy());

  // Simulate the input event being queued for a very long time. The compositor
  // task we post here represents the enqueued input task.
  clock_->AdvanceNow(priority_escalation_after_input_duration() * 2);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart));
  RunUntilIdle();

  // Even though we exceeded the input priority escalation period, we should
  // still be in compositor priority since the input remains queued.
  EXPECT_EQ(Policy::COMPOSITOR_PRIORITY, CurrentPolicy());

  // After the escalation period ends we should go back into normal mode.
  clock_->AdvanceNow(priority_escalation_after_input_duration() * 2);
  RunUntilIdle();
  EXPECT_EQ(Policy::NORMAL, CurrentPolicy());
}

class RendererSchedulerImplWithMockSchedulerTest
    : public RendererSchedulerImplTest {
 public:
  void SetUp() override {
    mock_task_runner_ =
        make_scoped_refptr(new cc::OrderedSimpleTaskRunner(clock_, false));
    nestable_task_runner_ =
        NestableTaskRunnerForTest::Create(mock_task_runner_);
    mock_scheduler_ = new RendererSchedulerImplForTest(nestable_task_runner_);
    Initialize(make_scoped_ptr(mock_scheduler_));
  }

 protected:
  RendererSchedulerImplForTest* mock_scheduler_;
};

TEST_F(RendererSchedulerImplWithMockSchedulerTest,
       OnlyOnePendingUrgentPolicyUpdatey) {
  mock_scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();
  mock_scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();
  mock_scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();
  mock_scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();

  RunUntilIdle();

  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);
}

TEST_F(RendererSchedulerImplWithMockSchedulerTest,
       OnePendingDelayedAndOneUrgentUpdatePolicy) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  mock_scheduler_->ScheduleDelayedPolicyUpdate(
      clock_->Now(), base::TimeDelta::FromMilliseconds(1));
  mock_scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();

  RunUntilIdle();

  // We expect both the urgent and the delayed updates to run.
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);
}

TEST_F(RendererSchedulerImplWithMockSchedulerTest,
       OneUrgentAndOnePendingDelayedUpdatePolicy) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  mock_scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();
  mock_scheduler_->ScheduleDelayedPolicyUpdate(
      clock_->Now(), base::TimeDelta::FromMilliseconds(1));

  RunUntilIdle();

  // We expect both the urgent and the delayed updates to run.
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);
}

TEST_F(RendererSchedulerImplWithMockSchedulerTest,
       UpdatePolicyCountTriggeredByOneInputEvent) {
  // We expect DidHandleInputEventOnCompositorThread to post an urgent policy
  // update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, mock_scheduler_->update_policy_count_);
  mock_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart));
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(1000));
  RunUntilIdle();

  // We finally expect a delayed policy update 100ms later.
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);
}

TEST_F(RendererSchedulerImplWithMockSchedulerTest,
       UpdatePolicyCountTriggeredByThreeInputEvents) {
  // We expect DidHandleInputEventOnCompositorThread to post an urgent policy
  // update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, mock_scheduler_->update_policy_count_);
  mock_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart));
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  // The second call to DidHandleInputEventOnCompositorThread should not post a
  // policy update because we are already in compositor priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  mock_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  // We expect DidHandleInputEvent to trigger a policy update.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove));
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  // The third call to DidHandleInputEventOnCompositorThread should post a
  // policy update because the awaiting_touch_start_response_ flag changed.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);
  mock_task_runner_->RunPendingTasks();
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);

  // We expect DidHandleInputEvent to trigger a policy update.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove));
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);

  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(1000));
  RunUntilIdle();

  // We finally expect a delayed policy update.
  EXPECT_EQ(3, mock_scheduler_->update_policy_count_);
}

TEST_F(RendererSchedulerImplWithMockSchedulerTest,
       UpdatePolicyCountTriggeredByTwoInputEventsWithALongSeparatingDelay) {
  // We expect DidHandleInputEventOnCompositorThread to post an urgent policy
  // update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, mock_scheduler_->update_policy_count_);
  mock_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart));
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(1000));
  RunUntilIdle();
  // We expect a delayed policy update.
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);

  // We expect the second call to DidHandleInputEventOnCompositorThread to post
  // an urgent policy update because we are no longer in compositor priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);
  mock_task_runner_->RunPendingTasks();
  EXPECT_EQ(3, mock_scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove));
  EXPECT_EQ(3, mock_scheduler_->update_policy_count_);

  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(1000));
  RunUntilIdle();

  // We finally expect a delayed policy update.
  EXPECT_EQ(4, mock_scheduler_->update_policy_count_);
}

TEST_F(RendererSchedulerImplWithMockSchedulerTest,
       EnsureUpdatePolicyNotTriggeredTooOften) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

  // We expect the first call to IsHighPriorityWorkAnticipated to be called
  // after recieving an input event (but before the UpdateTask was processed) to
  // call UpdatePolicy.
  EXPECT_EQ(0, mock_scheduler_->update_policy_count_);
  scheduler_->IsHighPriorityWorkAnticipated();
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);
  // Subsequent calls should not call UpdatePolicy.
  scheduler_->IsHighPriorityWorkAnticipated();
  scheduler_->IsHighPriorityWorkAnticipated();
  scheduler_->IsHighPriorityWorkAnticipated();
  scheduler_->ShouldYieldForHighPriorityWork();
  scheduler_->ShouldYieldForHighPriorityWork();
  scheduler_->ShouldYieldForHighPriorityWork();
  scheduler_->ShouldYieldForHighPriorityWork();

  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart));
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove));

  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  RunUntilIdle();
  // We expect both the urgent and the delayed updates to run in addition to the
  // earlier updated cause by IsHighPriorityWorkAnticipated.
  EXPECT_EQ(3, mock_scheduler_->update_policy_count_);
}

class RendererSchedulerImplWithMessageLoopTest
    : public RendererSchedulerImplTest {
 public:
  RendererSchedulerImplWithMessageLoopTest()
      : RendererSchedulerImplTest(new base::MessageLoop()) {}
  ~RendererSchedulerImplWithMessageLoopTest() override {}

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
    EnableIdleTasks();
    message_loop_->RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererSchedulerImplWithMessageLoopTest);
};

TEST_F(RendererSchedulerImplWithMessageLoopTest,
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
      base::Bind(
          &RendererSchedulerImplWithMessageLoopTest::PostFromNestedRunloop,
          base::Unretained(this),
          base::Unretained(&tasks_to_post_from_nested_loop)));

  EnableIdleTasks();
  RunUntilIdle();
  // Note we expect task 3 to run last because it's non-nestable.
  EXPECT_THAT(order, testing::ElementsAre(std::string("1"), std::string("2"),
                                          std::string("4"), std::string("5"),
                                          std::string("3")));
}

TEST_F(RendererSchedulerImplTest, TestLongIdlePeriod) {
  base::TimeTicks expected_deadline =
      clock_->Now() + maximum_idle_period_duration();
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run yet as no idle period.

  scheduler_->BeginFrameNotExpectedSoon();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(RendererSchedulerImplTest, TestLongIdlePeriodWithPendingDelayedTask) {
  base::TimeDelta pending_task_delay = base::TimeDelta::FromMilliseconds(30);
  base::TimeTicks expected_deadline = clock_->Now() + pending_task_delay;
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        pending_task_delay);

  scheduler_->BeginFrameNotExpectedSoon();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(RendererSchedulerImplTest,
       TestLongIdlePeriodWithLatePendingDelayedTask) {
  base::TimeDelta pending_task_delay = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        pending_task_delay);

  // Advance clock until after delayed task was meant to be run.
  clock_->AdvanceNow(base::TimeDelta::FromMilliseconds(20));

  // Post an idle task and BeginFrameNotExpectedSoon to initiate a long idle
  // period. Since there is a late pending delayed task this shouldn't actually
  // start an idle period.
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  scheduler_->BeginFrameNotExpectedSoon();
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // After the delayed task has been run we should trigger an idle period.
  clock_->AdvanceNow(maximum_idle_period_duration());
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
}

TEST_F(RendererSchedulerImplTest, TestLongIdlePeriodRepeating) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  std::vector<base::TimeTicks> actual_deadlines;
  int run_count = 0;

  max_idle_task_reposts = 3;
  base::TimeTicks clock_before(clock_->Now());
  base::TimeDelta idle_task_runtime(base::TimeDelta::FromMilliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingUpdateClockIdleTestTask, idle_task_runner_,
                 &run_count, clock_, idle_task_runtime, &actual_deadlines));
  scheduler_->BeginFrameNotExpectedSoon();
  RunUntilIdle();
  EXPECT_EQ(3, run_count);
  EXPECT_THAT(
      actual_deadlines,
      testing::ElementsAre(
          clock_before + maximum_idle_period_duration(),
          clock_before + idle_task_runtime + maximum_idle_period_duration(),
          clock_before + (2 * idle_task_runtime) +
              maximum_idle_period_duration()));

  // Check that idle tasks don't run after the idle period ends with a
  // new BeginMainFrame.
  max_idle_task_reposts = 5;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingUpdateClockIdleTestTask, idle_task_runner_,
                 &run_count, clock_, idle_task_runtime, &actual_deadlines));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&WillBeginFrameIdleTask,
                            base::Unretained(scheduler_.get()), clock_));
  RunUntilIdle();
  EXPECT_EQ(4, run_count);
}

TEST_F(RendererSchedulerImplTest, TestLongIdlePeriodDoesNotWakeScheduler) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  // Start a long idle period and get the time it should end.
  scheduler_->BeginFrameNotExpectedSoon();
  // The scheduler should not run the initiate_next_long_idle_period task if
  // there are no idle tasks and no other task woke up the scheduler, thus
  // the idle period deadline shouldn't update at the end of the current long
  // idle period.
  base::TimeTicks idle_period_deadline =
      scheduler_->CurrentIdleTaskDeadlineForTesting();
  clock_->AdvanceNow(maximum_idle_period_duration());
  RunUntilIdle();

  base::TimeTicks new_idle_period_deadline =
      scheduler_->CurrentIdleTaskDeadlineForTesting();
  EXPECT_EQ(idle_period_deadline, new_idle_period_deadline);

  // Posting a after-wakeup idle task also shouldn't wake the scheduler or
  // initiate the next long idle period.
  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  RunUntilIdle();
  new_idle_period_deadline = scheduler_->CurrentIdleTaskDeadlineForTesting();
  EXPECT_EQ(idle_period_deadline, new_idle_period_deadline);
  EXPECT_EQ(0, run_count);

  // Running a normal task should initiate a new long idle period though.
  default_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));
  RunUntilIdle();
  new_idle_period_deadline = scheduler_->CurrentIdleTaskDeadlineForTesting();
  EXPECT_EQ(idle_period_deadline + maximum_idle_period_duration(),
            new_idle_period_deadline);

  EXPECT_EQ(1, run_count);
}

TEST_F(RendererSchedulerImplTest, TestLongIdlePeriodInTouchStartPolicy) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  // Observation of touchstart should defer the start of the long idle period.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->BeginFrameNotExpectedSoon();
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // The long idle period should start after the touchstart policy has finished.
  clock_->AdvanceNow(priority_escalation_after_input_duration());
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
}

void TestCanExceedIdleDeadlineIfRequiredTask(RendererScheduler* scheduler,
                                             bool* can_exceed_idle_deadline_out,
                                             int* run_count,
                                             base::TimeTicks deadline) {
  *can_exceed_idle_deadline_out = scheduler->CanExceedIdleDeadlineIfRequired();
  (*run_count)++;
}

TEST_F(RendererSchedulerImplTest, CanExceedIdleDeadlineIfRequired) {
  int run_count = 0;
  bool can_exceed_idle_deadline = false;

  // Should return false if not in an idle period.
  EXPECT_FALSE(scheduler_->CanExceedIdleDeadlineIfRequired());

  // Should return false for short idle periods.
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&TestCanExceedIdleDeadlineIfRequiredTask, scheduler_.get(),
                 &can_exceed_idle_deadline, &run_count));
  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_FALSE(can_exceed_idle_deadline);

  // Should return false for a long idle period which is shortened due to a
  // pending delayed task.
  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        base::TimeDelta::FromMilliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&TestCanExceedIdleDeadlineIfRequiredTask, scheduler_.get(),
                 &can_exceed_idle_deadline, &run_count));
  scheduler_->BeginFrameNotExpectedSoon();
  RunUntilIdle();
  EXPECT_EQ(2, run_count);
  EXPECT_FALSE(can_exceed_idle_deadline);

  // Next long idle period will be for the maximum time, so
  // CanExceedIdleDeadlineIfRequired should return true.
  clock_->AdvanceNow(maximum_idle_period_duration());
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&TestCanExceedIdleDeadlineIfRequiredTask, scheduler_.get(),
                 &can_exceed_idle_deadline, &run_count));
  RunUntilIdle();
  EXPECT_EQ(3, run_count);
  EXPECT_TRUE(can_exceed_idle_deadline);

  // Next long idle period will be for the maximum time, so
  // CanExceedIdleDeadlineIfRequired should return true.
  scheduler_->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->Now(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL));
  EXPECT_FALSE(scheduler_->CanExceedIdleDeadlineIfRequired());
}

TEST_F(RendererSchedulerImplTest, TestRendererHiddenIdlePeriod) {
  int run_count = 0;

  max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingIdleTestTask, idle_task_runner_, &run_count));

  // Renderer should start in visible state.
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // When we hide the renderer it should start a max deadline idle period, which
  // will run an idle task and then immediately start a new idle period, which
  // runs the second idle task.
  scheduler_->OnRendererHidden();
  RunUntilIdle();
  EXPECT_EQ(2, run_count);

  // Advance time by amount of time by the maximum amount of time we execute
  // idle tasks when hidden (plus some slack) - idle period should have ended.
  max_idle_task_reposts = 3;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingIdleTestTask, idle_task_runner_, &run_count));
  clock_->AdvanceNow(end_idle_when_hidden_delay() +
                     base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  EXPECT_EQ(2, run_count);
}

TEST_F(RendererSchedulerImplTest, TimerQueueEnabledByDefault) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T1"), std::string("T2")));
}

TEST_F(RendererSchedulerImplTest, SuspendAndResumeTimerQueue) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");

  scheduler_->SuspendTimerQueue();
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  scheduler_->ResumeTimerQueue();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T1"), std::string("T2")));
}

TEST_F(RendererSchedulerImplTest, MultipleSuspendsNeedMultipleResumes) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");

  scheduler_->SuspendTimerQueue();
  scheduler_->SuspendTimerQueue();
  scheduler_->SuspendTimerQueue();
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  scheduler_->ResumeTimerQueue();
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  scheduler_->ResumeTimerQueue();
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  scheduler_->ResumeTimerQueue();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T1"), std::string("T2")));
}

TEST_F(RendererSchedulerImplTest, TaskQueueIdToString) {
  CheckAllTaskQueueIdToString();
}

TEST_F(RendererSchedulerImplTest, PolicyToString) {
  CheckAllTaskQueueIdToString();
}

TEST_F(RendererSchedulerImplTest, MismatchedDidHandleInputEventOnMainThread) {
  // This should not DCHECK because there was no corresponding compositor side
  // call to DidHandleInputEventOnCompositorThread with
  // INPUT_EVENT_ACK_STATE_NOT_CONSUMED. There are legitimate reasons for the
  // compositor to not be there and we don't want to make debugging impossible.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart));
}

TEST_F(RendererSchedulerImplTest, BeginMainFrameOnCriticalPath) {
  ASSERT_FALSE(scheduler_->BeginMainFrameOnCriticalPath());

  cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->Now(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL);
  scheduler_->WillBeginFrame(begin_frame_args);
  ASSERT_TRUE(scheduler_->BeginMainFrameOnCriticalPath());

  begin_frame_args.on_critical_path = false;
  scheduler_->WillBeginFrame(begin_frame_args);
  ASSERT_FALSE(scheduler_->BeginMainFrameOnCriticalPath());
}

TEST_F(RendererSchedulerImplTest, ShutdownPreventsPostingOfNewTasks) {
  scheduler_->Shutdown();
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 C1");
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

}  // namespace scheduler
