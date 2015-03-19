// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_ACTION(action, client, action_index, expected_num_actions)  \
  do {                                                                     \
    EXPECT_EQ(expected_num_actions, client->num_actions_());               \
    if (action_index >= 0) {                                               \
      ASSERT_LT(action_index, client->num_actions_()) << scheduler_.get(); \
      EXPECT_STREQ(action, client->Action(action_index));                  \
    }                                                                      \
    for (int i = expected_num_actions; i < client->num_actions_(); ++i)    \
      ADD_FAILURE() << "Unexpected action: " << client->Action(i)          \
                    << " with state:\n" << client->StateForAction(i);      \
  } while (false)

#define EXPECT_NO_ACTION(client) EXPECT_ACTION("", client, -1, 0)

#define EXPECT_SINGLE_ACTION(action, client) \
  EXPECT_ACTION(action, client, 0, 1)

#define EXPECT_SCOPED(statements) \
  {                               \
    SCOPED_TRACE("");             \
    statements;                   \
  }

namespace cc {
namespace {

class FakeSchedulerClient : public SchedulerClient {
 public:
  FakeSchedulerClient()
      : automatic_swap_ack_(true),
        begin_frame_is_sent_to_children_(false),
        scheduler_(nullptr) {
    Reset();
  }

  void Reset() {
    actions_.clear();
    states_.clear();
    draw_will_happen_ = true;
    swap_will_happen_if_draw_happens_ = true;
    num_draws_ = 0;
    log_anticipated_draw_time_change_ = false;
    begin_frame_is_sent_to_children_ = false;
  }

  void set_scheduler(TestScheduler* scheduler) { scheduler_ = scheduler; }

  // Most tests don't care about DidAnticipatedDrawTimeChange, so only record it
  // for tests that do.
  void set_log_anticipated_draw_time_change(bool log) {
    log_anticipated_draw_time_change_ = log;
  }
  bool needs_begin_frames() {
    return scheduler_->frame_source().NeedsBeginFrames();
  }
  int num_draws() const { return num_draws_; }
  int num_actions_() const { return static_cast<int>(actions_.size()); }
  const char* Action(int i) const { return actions_[i]; }
  std::string StateForAction(int i) const { return states_[i]->ToString(); }
  base::TimeTicks posted_begin_impl_frame_deadline() const {
    return posted_begin_impl_frame_deadline_;
  }

  int ActionIndex(const char* action) const {
    for (size_t i = 0; i < actions_.size(); i++)
      if (!strcmp(actions_[i], action))
        return i;
    return -1;
  }

  bool HasAction(const char* action) const {
    return ActionIndex(action) >= 0;
  }

  void SetDrawWillHappen(bool draw_will_happen) {
    draw_will_happen_ = draw_will_happen;
  }
  void SetSwapWillHappenIfDrawHappens(bool swap_will_happen_if_draw_happens) {
    swap_will_happen_if_draw_happens_ = swap_will_happen_if_draw_happens;
  }
  void SetAutomaticSwapAck(bool automatic_swap_ack) {
    automatic_swap_ack_ = automatic_swap_ack;
  }
  // SchedulerClient implementation.
  void WillBeginImplFrame(const BeginFrameArgs& args) override {
    PushAction("WillBeginImplFrame");
  }
  void ScheduledActionSendBeginMainFrame() override {
    PushAction("ScheduledActionSendBeginMainFrame");
  }
  void ScheduledActionAnimate() override {
    PushAction("ScheduledActionAnimate");
  }
  DrawResult ScheduledActionDrawAndSwapIfPossible() override {
    PushAction("ScheduledActionDrawAndSwapIfPossible");
    num_draws_++;
    DrawResult result =
        draw_will_happen_ ? DRAW_SUCCESS : DRAW_ABORTED_CHECKERBOARD_ANIMATIONS;
    bool swap_will_happen =
        draw_will_happen_ && swap_will_happen_if_draw_happens_;
    if (swap_will_happen) {
      scheduler_->DidSwapBuffers();

      if (automatic_swap_ack_)
        scheduler_->DidSwapBuffersComplete();
    }
    return result;
  }
  DrawResult ScheduledActionDrawAndSwapForced() override {
    PushAction("ScheduledActionDrawAndSwapForced");
    return DRAW_SUCCESS;
  }
  void ScheduledActionCommit() override { PushAction("ScheduledActionCommit"); }
  void ScheduledActionActivateSyncTree() override {
    PushAction("ScheduledActionActivateSyncTree");
  }
  void ScheduledActionBeginOutputSurfaceCreation() override {
    PushAction("ScheduledActionBeginOutputSurfaceCreation");
  }
  void ScheduledActionPrepareTiles() override {
    PushAction("ScheduledActionPrepareTiles");
  }
  void DidAnticipatedDrawTimeChange(base::TimeTicks) override {
    if (log_anticipated_draw_time_change_)
      PushAction("DidAnticipatedDrawTimeChange");
  }
  base::TimeDelta DrawDurationEstimate() override { return base::TimeDelta(); }
  base::TimeDelta BeginMainFrameToCommitDurationEstimate() override {
    return base::TimeDelta();
  }
  base::TimeDelta CommitToActivateDurationEstimate() override {
    return base::TimeDelta();
  }

  void DidBeginImplFrameDeadline() override {}

  void SendBeginFramesToChildren(const BeginFrameArgs& args) override {
    begin_frame_is_sent_to_children_ = true;
  }

  void SendBeginMainFrameNotExpectedSoon() override {
    PushAction("SendBeginMainFrameNotExpectedSoon");
  }

  base::Callback<bool(void)> ImplFrameDeadlinePending(bool state) {
    return base::Bind(&FakeSchedulerClient::ImplFrameDeadlinePendingCallback,
                      base::Unretained(this),
                      state);
  }

  bool begin_frame_is_sent_to_children() const {
    return begin_frame_is_sent_to_children_;
  }

  void PushAction(const char* description) {
    actions_.push_back(description);
    states_.push_back(scheduler_->AsValue());
  }

 protected:
  bool ImplFrameDeadlinePendingCallback(bool state) {
    return scheduler_->BeginImplFrameDeadlinePending() == state;
  }

  bool draw_will_happen_;
  bool swap_will_happen_if_draw_happens_;
  bool automatic_swap_ack_;
  int num_draws_;
  bool log_anticipated_draw_time_change_;
  bool begin_frame_is_sent_to_children_;
  base::TimeTicks posted_begin_impl_frame_deadline_;
  std::vector<const char*> actions_;
  std::vector<scoped_refptr<base::trace_event::ConvertableToTraceFormat>>
      states_;
  TestScheduler* scheduler_;
};

class FakeExternalBeginFrameSource : public BeginFrameSourceMixIn {
 public:
  explicit FakeExternalBeginFrameSource(FakeSchedulerClient* client)
      : client_(client) {}
  ~FakeExternalBeginFrameSource() override {}

  void OnNeedsBeginFramesChange(bool needs_begin_frames) override {
    if (needs_begin_frames) {
      client_->PushAction("SetNeedsBeginFrames(true)");
    } else {
      client_->PushAction("SetNeedsBeginFrames(false)");
    }
  }

  void TestOnBeginFrame(const BeginFrameArgs& args) {
    return CallOnBeginFrame(args);
  }

 private:
  FakeSchedulerClient* client_;
};

class SchedulerTest : public testing::Test {
 public:
  SchedulerTest()
      : now_src_(TestNowSource::Create()),
        task_runner_(new OrderedSimpleTaskRunner(now_src_, true)),
        fake_external_begin_frame_source_(nullptr) {
    // A bunch of tests require Now() to be > BeginFrameArgs::DefaultInterval()
    now_src_->AdvanceNow(base::TimeDelta::FromMilliseconds(100));
    // Fail if we need to run 100 tasks in a row.
    task_runner_->SetRunTaskLimit(100);
  }

  ~SchedulerTest() override {}

 protected:
  TestScheduler* CreateScheduler() {
    scoped_ptr<FakeExternalBeginFrameSource> fake_external_begin_frame_source;
    if (scheduler_settings_.use_external_begin_frame_source) {
      fake_external_begin_frame_source.reset(
          new FakeExternalBeginFrameSource(client_.get()));
      fake_external_begin_frame_source_ =
          fake_external_begin_frame_source.get();
    }
    scheduler_ = TestScheduler::Create(now_src_, client_.get(),
                                       scheduler_settings_, 0, task_runner_,
                                       fake_external_begin_frame_source.Pass());
    DCHECK(scheduler_);
    client_->set_scheduler(scheduler_.get());
    return scheduler_.get();
  }

  void CreateSchedulerAndInitSurface() {
    CreateScheduler();
    EXPECT_SCOPED(InitializeOutputSurfaceAndFirstCommit());
  }

  void SetUpScheduler(bool initSurface) {
    SetUpScheduler(make_scoped_ptr(new FakeSchedulerClient), initSurface);
  }

  void SetUpScheduler(scoped_ptr<FakeSchedulerClient> client,
                      bool initSurface) {
    client_ = client.Pass();
    if (initSurface)
      CreateSchedulerAndInitSurface();
    else
      CreateScheduler();
  }

  OrderedSimpleTaskRunner& task_runner() { return *task_runner_; }
  TestNowSource* now_src() { return now_src_.get(); }

  // As this function contains EXPECT macros, to allow debugging it should be
  // called inside EXPECT_SCOPED like so;
  //   EXPECT_SCOPED(client.InitializeOutputSurfaceAndFirstCommit(scheduler));
  void InitializeOutputSurfaceAndFirstCommit() {
    TRACE_EVENT0("cc",
                 "SchedulerUnitTest::InitializeOutputSurfaceAndFirstCommit");
    DCHECK(scheduler_);

    // Check the client doesn't have any actions queued when calling this
    // function.
    EXPECT_NO_ACTION(client_);
    EXPECT_FALSE(client_->needs_begin_frames());

    // Start the initial output surface creation.
    EXPECT_FALSE(scheduler_->CanStart());
    scheduler_->SetCanStart();
    scheduler_->SetVisible(true);
    scheduler_->SetCanDraw(true);
    EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client_);

    client_->Reset();

    // We don't see anything happening until the first impl frame.
    scheduler_->DidCreateAndInitializeOutputSurface();
    scheduler_->SetNeedsCommit();
    EXPECT_TRUE(client_->needs_begin_frames());
    EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
    client_->Reset();

    {
      SCOPED_TRACE("Do first frame to commit after initialize.");
      AdvanceFrame();

      scheduler_->NotifyBeginMainFrameStarted();
      scheduler_->NotifyReadyToCommitThenActivateIfNeeded();

      // Run the posted deadline task.
      EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
      task_runner_->RunTasksWhile(client_->ImplFrameDeadlinePending(true));
      EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());

      EXPECT_FALSE(scheduler_->CommitPending());
    }

    client_->Reset();

    {
      SCOPED_TRACE(
          "Run second frame so Scheduler calls SetNeedsBeginFrame(false).");
      AdvanceFrame();

      // Run the posted deadline task.
      EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
      task_runner_->RunTasksWhile(client_->ImplFrameDeadlinePending(true));
      EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
    }

    EXPECT_FALSE(client_->needs_begin_frames());
    client_->Reset();
  }

  // As this function contains EXPECT macros, to allow debugging it should be
  // called inside EXPECT_SCOPED like so;
  //   EXPECT_SCOPED(client.AdvanceFrame());
  void AdvanceFrame() {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.frames"),
                 "FakeSchedulerClient::AdvanceFrame");
    // Consume any previous deadline first, if no deadline is currently
    // pending, ImplFrameDeadlinePending will return false straight away and we
    // will run no tasks.
    task_runner_->RunTasksWhile(client_->ImplFrameDeadlinePending(true));
    EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());

    // Send the next BeginFrame message if using an external source, otherwise
    // it will be already in the task queue.
    if (scheduler_->settings().use_external_begin_frame_source &&
        scheduler_->FrameProductionThrottled()) {
      SendNextBeginFrame();
      EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
    }

    // Then run tasks until new deadline is scheduled.
    EXPECT_TRUE(
        task_runner_->RunTasksWhile(client_->ImplFrameDeadlinePending(false)));
    EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  }

  void SendNextBeginFrame() {
    DCHECK(scheduler_->settings().use_external_begin_frame_source);
    // Creep the time forward so that any BeginFrameArgs is not equal to the
    // last one otherwise we violate the BeginFrameSource contract.
    now_src_->AdvanceNow(BeginFrameArgs::DefaultInterval());
    fake_external_begin_frame_source_->TestOnBeginFrame(
        CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, now_src()));
  }

  FakeExternalBeginFrameSource* fake_external_begin_frame_source() const {
    return fake_external_begin_frame_source_;
  }

  void MainFrameInHighLatencyMode(
      int64 begin_main_frame_to_commit_estimate_in_ms,
      int64 commit_to_activate_estimate_in_ms,
      bool impl_latency_takes_priority,
      bool should_send_begin_main_frame);
  void BeginFramesNotFromClient(bool use_external_begin_frame_source,
                                bool throttle_frame_production);
  void BeginFramesNotFromClient_SwapThrottled(
      bool use_external_begin_frame_source,
      bool throttle_frame_production);
  void DidLoseOutputSurfaceAfterBeginFrameStartedWithHighLatency(
      bool impl_side_painting);
  void DidLoseOutputSurfaceAfterReadyToCommit(bool impl_side_painting);

  scoped_refptr<TestNowSource> now_src_;
  scoped_refptr<OrderedSimpleTaskRunner> task_runner_;
  FakeExternalBeginFrameSource* fake_external_begin_frame_source_;
  SchedulerSettings scheduler_settings_;
  scoped_ptr<FakeSchedulerClient> client_;
  scoped_ptr<TestScheduler> scheduler_;
};

TEST_F(SchedulerTest, InitializeOutputSurfaceDoesNotBeginImplFrame) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(false);
  scheduler_->SetCanStart();
  scheduler_->SetVisible(true);
  scheduler_->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client_);
  client_->Reset();
  scheduler_->DidCreateAndInitializeOutputSurface();
  EXPECT_NO_ACTION(client_);
}

TEST_F(SchedulerTest, SendBeginFramesToChildren) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  EXPECT_FALSE(client_->begin_frame_is_sent_to_children());
  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);
  EXPECT_TRUE(client_->needs_begin_frames());

  scheduler_->SetChildrenNeedBeginFrames(true);

  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_TRUE(client_->begin_frame_is_sent_to_children());
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(client_->needs_begin_frames());
}

TEST_F(SchedulerTest, SendBeginFramesToChildrenWithoutCommit) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  EXPECT_FALSE(client_->needs_begin_frames());
  scheduler_->SetChildrenNeedBeginFrames(true);
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);
  EXPECT_TRUE(client_->needs_begin_frames());

  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_TRUE(client_->begin_frame_is_sent_to_children());
}

TEST_F(SchedulerTest, RequestCommit) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame.
  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);
  client_->Reset();

  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // If we don't swap on the deadline, we wait for the next BeginFrame.
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_NO_ACTION(client_);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // NotifyReadyToCommit should trigger the commit.
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // BeginImplFrame should prepare the draw.
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // BeginImplFrame deadline should draw.
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 0, 1);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // The following BeginImplFrame deadline should SetNeedsBeginFrame(false)
  // to avoid excessive toggles.
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_SINGLE_ACTION("WillBeginImplFrame", client_);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("SetNeedsBeginFrames(false)", client_, 0, 2);
  EXPECT_ACTION("SendBeginMainFrameNotExpectedSoon", client_, 1, 2);
  client_->Reset();
}

TEST_F(SchedulerTest, RequestCommitAfterSetDeferCommit) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  scheduler_->SetDeferCommits(true);

  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);

  client_->Reset();
  AdvanceFrame();
  // BeginMainFrame is not sent during the defer commit is on.
  EXPECT_SINGLE_ACTION("WillBeginImplFrame", client_);

  client_->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  // There is no posted deadline.
  EXPECT_NO_ACTION(client_);
  EXPECT_TRUE(client_->needs_begin_frames());

  client_->Reset();
  scheduler_->SetDeferCommits(false);
  EXPECT_NO_ACTION(client_);

  // Start new BeginMainFrame after defer commit is off.
  client_->Reset();
  AdvanceFrame();
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
}

TEST_F(SchedulerTest, DeferCommitWithRedraw) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  scheduler_->SetDeferCommits(true);

  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);

  client_->Reset();
  scheduler_->SetNeedsRedraw();
  EXPECT_NO_ACTION(client_);

  client_->Reset();
  AdvanceFrame();
  // BeginMainFrame is not sent during the defer commit is on.
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);

  client_->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_SINGLE_ACTION("ScheduledActionDrawAndSwapIfPossible", client_);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());

  client_->Reset();
  AdvanceFrame();
  EXPECT_SINGLE_ACTION("WillBeginImplFrame", client_);
}

TEST_F(SchedulerTest, RequestCommitAfterBeginMainFrameSent) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  // SetNeedsCommit should begin the frame.
  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);

  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // Now SetNeedsCommit again. Calling here means we need a second commit.
  scheduler_->SetNeedsCommit();
  EXPECT_EQ(client_->num_actions_(), 0);
  client_->Reset();

  // Finish the first commit.
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionAnimate", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 1, 2);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());

  // Because we just swapped, the Scheduler should also request the next
  // BeginImplFrame from the OutputSurface.
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();
  // Since another commit is needed, the next BeginImplFrame should initiate
  // the second commit.
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  // Finishing the commit before the deadline should post a new deadline task
  // to trigger the deadline early.
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionAnimate", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 1, 2);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // On the next BeginImplFrame, verify we go back to a quiescent state and
  // no longer request BeginImplFrames.
  EXPECT_SCOPED(AdvanceFrame());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_FALSE(client_->needs_begin_frames());
  client_->Reset();
}

class SchedulerClientThatsetNeedsDrawInsideDraw : public FakeSchedulerClient {
 public:
  SchedulerClientThatsetNeedsDrawInsideDraw()
      : FakeSchedulerClient(), request_redraws_(false) {}

  void ScheduledActionSendBeginMainFrame() override {}

  void SetRequestRedrawsInsideDraw(bool enable) { request_redraws_ = enable; }

  DrawResult ScheduledActionDrawAndSwapIfPossible() override {
    // Only SetNeedsRedraw the first time this is called
    if (request_redraws_) {
      scheduler_->SetNeedsRedraw();
    }
    return FakeSchedulerClient::ScheduledActionDrawAndSwapIfPossible();
  }

  DrawResult ScheduledActionDrawAndSwapForced() override {
    NOTREACHED();
    return DRAW_SUCCESS;
  }

  void ScheduledActionCommit() override {}
  void DidAnticipatedDrawTimeChange(base::TimeTicks) override {}

 private:
  bool request_redraws_;
};

// Tests for two different situations:
// 1. the scheduler dropping SetNeedsRedraw requests that happen inside
//    a ScheduledActionDrawAndSwap
// 2. the scheduler drawing twice inside a single tick
TEST_F(SchedulerTest, RequestRedrawInsideDraw) {
  SchedulerClientThatsetNeedsDrawInsideDraw* client =
      new SchedulerClientThatsetNeedsDrawInsideDraw;
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(make_scoped_ptr(client).Pass(), true);
  client->SetRequestRedrawsInsideDraw(true);

  scheduler_->SetNeedsRedraw();
  EXPECT_TRUE(scheduler_->RedrawPending());
  EXPECT_TRUE(client->needs_begin_frames());
  EXPECT_EQ(0, client->num_draws());

  EXPECT_SCOPED(AdvanceFrame());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client->num_draws());
  EXPECT_TRUE(scheduler_->RedrawPending());
  EXPECT_TRUE(client->needs_begin_frames());

  client->SetRequestRedrawsInsideDraw(false);

  EXPECT_SCOPED(AdvanceFrame());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(2, client_->num_draws());
  EXPECT_FALSE(scheduler_->RedrawPending());
  EXPECT_TRUE(client->needs_begin_frames());

  // We stop requesting BeginImplFrames after a BeginImplFrame where we don't
  // swap.
  EXPECT_SCOPED(AdvanceFrame());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(2, client->num_draws());
  EXPECT_FALSE(scheduler_->RedrawPending());
  EXPECT_FALSE(client->needs_begin_frames());
}

// Test that requesting redraw inside a failed draw doesn't lose the request.
TEST_F(SchedulerTest, RequestRedrawInsideFailedDraw) {
  SchedulerClientThatsetNeedsDrawInsideDraw* client =
      new SchedulerClientThatsetNeedsDrawInsideDraw;
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(make_scoped_ptr(client).Pass(), true);

  client->SetRequestRedrawsInsideDraw(true);
  client->SetDrawWillHappen(false);

  scheduler_->SetNeedsRedraw();
  EXPECT_TRUE(scheduler_->RedrawPending());
  EXPECT_TRUE(client->needs_begin_frames());
  EXPECT_EQ(0, client->num_draws());

  // Fail the draw.
  EXPECT_SCOPED(AdvanceFrame());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client->num_draws());

  // We have a commit pending and the draw failed, and we didn't lose the redraw
  // request.
  EXPECT_TRUE(scheduler_->CommitPending());
  EXPECT_TRUE(scheduler_->RedrawPending());
  EXPECT_TRUE(client->needs_begin_frames());

  client->SetRequestRedrawsInsideDraw(false);

  // Fail the draw again.
  EXPECT_SCOPED(AdvanceFrame());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(2, client->num_draws());
  EXPECT_TRUE(scheduler_->CommitPending());
  EXPECT_TRUE(scheduler_->RedrawPending());
  EXPECT_TRUE(client->needs_begin_frames());

  // Draw successfully.
  client->SetDrawWillHappen(true);
  EXPECT_SCOPED(AdvanceFrame());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(3, client->num_draws());
  EXPECT_TRUE(scheduler_->CommitPending());
  EXPECT_FALSE(scheduler_->RedrawPending());
  EXPECT_TRUE(client->needs_begin_frames());
}

class SchedulerClientThatSetNeedsCommitInsideDraw : public FakeSchedulerClient {
 public:
  SchedulerClientThatSetNeedsCommitInsideDraw()
      : set_needs_commit_on_next_draw_(false) {}

  void ScheduledActionSendBeginMainFrame() override {}
  DrawResult ScheduledActionDrawAndSwapIfPossible() override {
    // Only SetNeedsCommit the first time this is called
    if (set_needs_commit_on_next_draw_) {
      scheduler_->SetNeedsCommit();
      set_needs_commit_on_next_draw_ = false;
    }
    return FakeSchedulerClient::ScheduledActionDrawAndSwapIfPossible();
  }

  DrawResult ScheduledActionDrawAndSwapForced() override {
    NOTREACHED();
    return DRAW_SUCCESS;
  }

  void ScheduledActionCommit() override {}
  void DidAnticipatedDrawTimeChange(base::TimeTicks) override {}

  void SetNeedsCommitOnNextDraw() { set_needs_commit_on_next_draw_ = true; }

 private:
  bool set_needs_commit_on_next_draw_;
};

// Tests for the scheduler infinite-looping on SetNeedsCommit requests that
// happen inside a ScheduledActionDrawAndSwap
TEST_F(SchedulerTest, RequestCommitInsideDraw) {
  SchedulerClientThatSetNeedsCommitInsideDraw* client =
      new SchedulerClientThatSetNeedsCommitInsideDraw;

  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(make_scoped_ptr(client).Pass(), true);

  EXPECT_FALSE(client->needs_begin_frames());
  scheduler_->SetNeedsRedraw();
  EXPECT_TRUE(scheduler_->RedrawPending());
  EXPECT_EQ(0, client->num_draws());
  EXPECT_TRUE(client->needs_begin_frames());

  client->SetNeedsCommitOnNextDraw();
  EXPECT_SCOPED(AdvanceFrame());
  client->SetNeedsCommitOnNextDraw();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client->num_draws());
  EXPECT_TRUE(scheduler_->CommitPending());
  EXPECT_TRUE(client->needs_begin_frames());
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();

  EXPECT_SCOPED(AdvanceFrame());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(2, client->num_draws());

  EXPECT_FALSE(scheduler_->RedrawPending());
  EXPECT_FALSE(scheduler_->CommitPending());
  EXPECT_TRUE(client->needs_begin_frames());

  // We stop requesting BeginImplFrames after a BeginImplFrame where we don't
  // swap.
  EXPECT_SCOPED(AdvanceFrame());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(2, client->num_draws());
  EXPECT_FALSE(scheduler_->RedrawPending());
  EXPECT_FALSE(scheduler_->CommitPending());
  EXPECT_FALSE(client->needs_begin_frames());
}

// Tests that when a draw fails then the pending commit should not be dropped.
TEST_F(SchedulerTest, RequestCommitInsideFailedDraw) {
  SchedulerClientThatsetNeedsDrawInsideDraw* client =
      new SchedulerClientThatsetNeedsDrawInsideDraw;
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(make_scoped_ptr(client).Pass(), true);

  client->SetDrawWillHappen(false);

  scheduler_->SetNeedsRedraw();
  EXPECT_TRUE(scheduler_->RedrawPending());
  EXPECT_TRUE(client->needs_begin_frames());
  EXPECT_EQ(0, client->num_draws());

  // Fail the draw.
  EXPECT_SCOPED(AdvanceFrame());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client->num_draws());

  // We have a commit pending and the draw failed, and we didn't lose the commit
  // request.
  EXPECT_TRUE(scheduler_->CommitPending());
  EXPECT_TRUE(scheduler_->RedrawPending());
  EXPECT_TRUE(client->needs_begin_frames());

  // Fail the draw again.
  EXPECT_SCOPED(AdvanceFrame());

  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(2, client->num_draws());
  EXPECT_TRUE(scheduler_->CommitPending());
  EXPECT_TRUE(scheduler_->RedrawPending());
  EXPECT_TRUE(client->needs_begin_frames());

  // Draw successfully.
  client->SetDrawWillHappen(true);
  EXPECT_SCOPED(AdvanceFrame());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(3, client->num_draws());
  EXPECT_TRUE(scheduler_->CommitPending());
  EXPECT_FALSE(scheduler_->RedrawPending());
  EXPECT_TRUE(client->needs_begin_frames());
}

TEST_F(SchedulerTest, NoSwapWhenDrawFails) {
  SchedulerClientThatSetNeedsCommitInsideDraw* client =
      new SchedulerClientThatSetNeedsCommitInsideDraw;
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(make_scoped_ptr(client).Pass(), true);

  scheduler_->SetNeedsRedraw();
  EXPECT_TRUE(scheduler_->RedrawPending());
  EXPECT_TRUE(client->needs_begin_frames());
  EXPECT_EQ(0, client->num_draws());

  // Draw successfully, this starts a new frame.
  client->SetNeedsCommitOnNextDraw();
  EXPECT_SCOPED(AdvanceFrame());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client->num_draws());

  scheduler_->SetNeedsRedraw();
  EXPECT_TRUE(scheduler_->RedrawPending());
  EXPECT_TRUE(client->needs_begin_frames());

  // Fail to draw, this should not start a frame.
  client->SetDrawWillHappen(false);
  client->SetNeedsCommitOnNextDraw();
  EXPECT_SCOPED(AdvanceFrame());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(2, client->num_draws());
}

class SchedulerClientNeedsPrepareTilesInDraw : public FakeSchedulerClient {
 public:
  DrawResult ScheduledActionDrawAndSwapIfPossible() override {
    scheduler_->SetNeedsPrepareTiles();
    return FakeSchedulerClient::ScheduledActionDrawAndSwapIfPossible();
  }
};

// Test prepare tiles is independant of draws.
TEST_F(SchedulerTest, PrepareTiles) {
  SchedulerClientNeedsPrepareTilesInDraw* client =
      new SchedulerClientNeedsPrepareTilesInDraw;
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(make_scoped_ptr(client).Pass(), true);

  // Request both draw and prepare tiles. PrepareTiles shouldn't
  // be trigged until BeginImplFrame.
  client->Reset();
  scheduler_->SetNeedsPrepareTiles();
  scheduler_->SetNeedsRedraw();
  EXPECT_TRUE(scheduler_->RedrawPending());
  EXPECT_TRUE(scheduler_->PrepareTilesPending());
  EXPECT_TRUE(client->needs_begin_frames());
  EXPECT_EQ(0, client->num_draws());
  EXPECT_FALSE(client->HasAction("ScheduledActionPrepareTiles"));
  EXPECT_FALSE(client->HasAction("ScheduledActionDrawAndSwapIfPossible"));

  // We have no immediate actions to perform, so the BeginImplFrame should post
  // the deadline task.
  client->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  // On the deadline, he actions should have occured in the right order.
  client->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client->num_draws());
  EXPECT_TRUE(client->HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client->HasAction("ScheduledActionPrepareTiles"));
  EXPECT_LT(client->ActionIndex("ScheduledActionDrawAndSwapIfPossible"),
            client->ActionIndex("ScheduledActionPrepareTiles"));
  EXPECT_FALSE(scheduler_->RedrawPending());
  EXPECT_FALSE(scheduler_->PrepareTilesPending());
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());

  // Request a draw. We don't need a PrepareTiles yet.
  client->Reset();
  scheduler_->SetNeedsRedraw();
  EXPECT_TRUE(scheduler_->RedrawPending());
  EXPECT_FALSE(scheduler_->PrepareTilesPending());
  EXPECT_TRUE(client->needs_begin_frames());
  EXPECT_EQ(0, client->num_draws());

  // We have no immediate actions to perform, so the BeginImplFrame should post
  // the deadline task.
  client->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  // Draw. The draw will trigger SetNeedsPrepareTiles, and
  // then the PrepareTiles action will be triggered after the Draw.
  // Afterwards, neither a draw nor PrepareTiles are pending.
  client->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client->num_draws());
  EXPECT_TRUE(client->HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client->HasAction("ScheduledActionPrepareTiles"));
  EXPECT_LT(client->ActionIndex("ScheduledActionDrawAndSwapIfPossible"),
            client->ActionIndex("ScheduledActionPrepareTiles"));
  EXPECT_FALSE(scheduler_->RedrawPending());
  EXPECT_FALSE(scheduler_->PrepareTilesPending());
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());

  // We need a BeginImplFrame where we don't swap to go idle.
  client->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_SINGLE_ACTION("WillBeginImplFrame", client);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("SetNeedsBeginFrames(false)", client_, 0, 2);
  EXPECT_ACTION("SendBeginMainFrameNotExpectedSoon", client_, 1, 2);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_EQ(0, client->num_draws());

  // Now trigger a PrepareTiles outside of a draw. We will then need
  // a begin-frame for the PrepareTiles, but we don't need a draw.
  client->Reset();
  EXPECT_FALSE(client->needs_begin_frames());
  scheduler_->SetNeedsPrepareTiles();
  EXPECT_TRUE(client->needs_begin_frames());
  EXPECT_TRUE(scheduler_->PrepareTilesPending());
  EXPECT_FALSE(scheduler_->RedrawPending());

  // BeginImplFrame. There will be no draw, only PrepareTiles.
  client->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_SINGLE_ACTION("WillBeginImplFrame", client);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(0, client->num_draws());
  EXPECT_FALSE(client->HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client->HasAction("ScheduledActionPrepareTiles"));
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
}

// Test that PrepareTiles only happens once per frame.  If an external caller
// initiates it, then the state machine should not PrepareTiles on that frame.
TEST_F(SchedulerTest, PrepareTilesOncePerFrame) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  // If DidPrepareTiles during a frame, then PrepareTiles should not occur
  // again.
  scheduler_->SetNeedsPrepareTiles();
  scheduler_->SetNeedsRedraw();
  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  EXPECT_TRUE(scheduler_->PrepareTilesPending());
  scheduler_->DidPrepareTiles();  // An explicit PrepareTiles.
  EXPECT_FALSE(scheduler_->PrepareTilesPending());

  client_->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client_->num_draws());
  EXPECT_TRUE(client_->HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_FALSE(client_->HasAction("ScheduledActionPrepareTiles"));
  EXPECT_FALSE(scheduler_->RedrawPending());
  EXPECT_FALSE(scheduler_->PrepareTilesPending());
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());

  // Next frame without DidPrepareTiles should PrepareTiles with draw.
  scheduler_->SetNeedsPrepareTiles();
  scheduler_->SetNeedsRedraw();
  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  client_->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client_->num_draws());
  EXPECT_TRUE(client_->HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client_->HasAction("ScheduledActionPrepareTiles"));
  EXPECT_LT(client_->ActionIndex("ScheduledActionDrawAndSwapIfPossible"),
            client_->ActionIndex("ScheduledActionPrepareTiles"));
  EXPECT_FALSE(scheduler_->RedrawPending());
  EXPECT_FALSE(scheduler_->PrepareTilesPending());
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  scheduler_->DidPrepareTiles();  // Corresponds to ScheduledActionPrepareTiles

  // If we get another DidPrepareTiles within the same frame, we should
  // not PrepareTiles on the next frame.
  scheduler_->DidPrepareTiles();  // An explicit PrepareTiles.
  scheduler_->SetNeedsPrepareTiles();
  scheduler_->SetNeedsRedraw();
  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  EXPECT_TRUE(scheduler_->PrepareTilesPending());

  client_->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client_->num_draws());
  EXPECT_TRUE(client_->HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_FALSE(client_->HasAction("ScheduledActionPrepareTiles"));
  EXPECT_FALSE(scheduler_->RedrawPending());
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());

  // If we get another DidPrepareTiles, we should not PrepareTiles on the next
  // frame. This verifies we don't alternate calling PrepareTiles once and
  // twice.
  EXPECT_TRUE(scheduler_->PrepareTilesPending());
  scheduler_->DidPrepareTiles();  // An explicit PrepareTiles.
  EXPECT_FALSE(scheduler_->PrepareTilesPending());
  scheduler_->SetNeedsPrepareTiles();
  scheduler_->SetNeedsRedraw();
  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  EXPECT_TRUE(scheduler_->PrepareTilesPending());

  client_->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client_->num_draws());
  EXPECT_TRUE(client_->HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_FALSE(client_->HasAction("ScheduledActionPrepareTiles"));
  EXPECT_FALSE(scheduler_->RedrawPending());
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());

  // Next frame without DidPrepareTiles should PrepareTiles with draw.
  scheduler_->SetNeedsPrepareTiles();
  scheduler_->SetNeedsRedraw();
  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  client_->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(1, client_->num_draws());
  EXPECT_TRUE(client_->HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client_->HasAction("ScheduledActionPrepareTiles"));
  EXPECT_LT(client_->ActionIndex("ScheduledActionDrawAndSwapIfPossible"),
            client_->ActionIndex("ScheduledActionPrepareTiles"));
  EXPECT_FALSE(scheduler_->RedrawPending());
  EXPECT_FALSE(scheduler_->PrepareTilesPending());
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  scheduler_->DidPrepareTiles();  // Corresponds to ScheduledActionPrepareTiles
}

TEST_F(SchedulerTest, TriggerBeginFrameDeadlineEarly) {
  SchedulerClientNeedsPrepareTilesInDraw* client =
      new SchedulerClientNeedsPrepareTilesInDraw;
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(make_scoped_ptr(client).Pass(), true);

  scheduler_->SetNeedsRedraw();
  EXPECT_SCOPED(AdvanceFrame());

  // The deadline should be zero since there is no work other than drawing
  // pending.
  EXPECT_EQ(base::TimeTicks(), client->posted_begin_impl_frame_deadline());
}

class SchedulerClientWithFixedEstimates : public FakeSchedulerClient {
 public:
  SchedulerClientWithFixedEstimates(
      base::TimeDelta draw_duration,
      base::TimeDelta begin_main_frame_to_commit_duration,
      base::TimeDelta commit_to_activate_duration)
      : draw_duration_(draw_duration),
        begin_main_frame_to_commit_duration_(
            begin_main_frame_to_commit_duration),
        commit_to_activate_duration_(commit_to_activate_duration) {}

  base::TimeDelta DrawDurationEstimate() override { return draw_duration_; }
  base::TimeDelta BeginMainFrameToCommitDurationEstimate() override {
    return begin_main_frame_to_commit_duration_;
  }
  base::TimeDelta CommitToActivateDurationEstimate() override {
    return commit_to_activate_duration_;
  }

 private:
    base::TimeDelta draw_duration_;
    base::TimeDelta begin_main_frame_to_commit_duration_;
    base::TimeDelta commit_to_activate_duration_;
};

void SchedulerTest::MainFrameInHighLatencyMode(
    int64 begin_main_frame_to_commit_estimate_in_ms,
    int64 commit_to_activate_estimate_in_ms,
    bool impl_latency_takes_priority,
    bool should_send_begin_main_frame) {
  // Set up client with specified estimates (draw duration is set to 1).
  SchedulerClientWithFixedEstimates* client =
      new SchedulerClientWithFixedEstimates(
          base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromMilliseconds(
              begin_main_frame_to_commit_estimate_in_ms),
          base::TimeDelta::FromMilliseconds(commit_to_activate_estimate_in_ms));

  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(make_scoped_ptr(client).Pass(), true);

  scheduler_->SetImplLatencyTakesPriority(impl_latency_takes_priority);

  // Impl thread hits deadline before commit finishes.
  scheduler_->SetNeedsCommit();
  EXPECT_FALSE(scheduler_->MainThreadIsInHighLatencyMode());
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_FALSE(scheduler_->MainThreadIsInHighLatencyMode());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_TRUE(scheduler_->MainThreadIsInHighLatencyMode());
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_TRUE(scheduler_->MainThreadIsInHighLatencyMode());
  EXPECT_TRUE(client->HasAction("ScheduledActionSendBeginMainFrame"));

  client->Reset();
  scheduler_->SetNeedsCommit();
  EXPECT_TRUE(scheduler_->MainThreadIsInHighLatencyMode());
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_TRUE(scheduler_->MainThreadIsInHighLatencyMode());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_EQ(scheduler_->MainThreadIsInHighLatencyMode(),
            should_send_begin_main_frame);
  EXPECT_EQ(client->HasAction("ScheduledActionSendBeginMainFrame"),
            should_send_begin_main_frame);
}

TEST_F(SchedulerTest,
       SkipMainFrameIfHighLatencyAndCanCommitAndActivateBeforeDeadline) {
  // Set up client so that estimates indicate that we can commit and activate
  // before the deadline (~8ms by default).
  MainFrameInHighLatencyMode(1, 1, false, false);
}

TEST_F(SchedulerTest, NotSkipMainFrameIfHighLatencyAndCanCommitTooLong) {
  // Set up client so that estimates indicate that the commit cannot finish
  // before the deadline (~8ms by default).
  MainFrameInHighLatencyMode(10, 1, false, true);
}

TEST_F(SchedulerTest, NotSkipMainFrameIfHighLatencyAndCanActivateTooLong) {
  // Set up client so that estimates indicate that the activate cannot finish
  // before the deadline (~8ms by default).
  MainFrameInHighLatencyMode(1, 10, false, true);
}

TEST_F(SchedulerTest, NotSkipMainFrameInPreferImplLatencyMode) {
  // Set up client so that estimates indicate that we can commit and activate
  // before the deadline (~8ms by default), but also enable impl latency takes
  // priority mode.
  MainFrameInHighLatencyMode(1, 1, true, true);
}

TEST_F(SchedulerTest, PollForCommitCompletion) {
  // Since we are simulating a long commit, set up a client with draw duration
  // estimates that prevent skipping main frames to get to low latency mode.
  SchedulerClientWithFixedEstimates* client =
      new SchedulerClientWithFixedEstimates(
          base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromMilliseconds(32),
          base::TimeDelta::FromMilliseconds(32));
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(make_scoped_ptr(client).Pass(), true);

  client->set_log_anticipated_draw_time_change(true);

  BeginFrameArgs frame_args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, now_src());
  frame_args.interval = base::TimeDelta::FromMilliseconds(1000);

  // At this point, we've drawn a frame. Start another commit, but hold off on
  // the NotifyReadyToCommit for now.
  EXPECT_FALSE(scheduler_->CommitPending());
  scheduler_->SetNeedsCommit();
  fake_external_begin_frame_source()->TestOnBeginFrame(frame_args);
  EXPECT_TRUE(scheduler_->CommitPending());

  // Draw and swap the frame, but don't ack the swap to simulate the Browser
  // blocking on the renderer.
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  scheduler_->DidSwapBuffers();

  // Spin the event loop a few times and make sure we get more
  // DidAnticipateDrawTimeChange calls every time.
  int actions_so_far = client->num_actions_();

  // Does three iterations to make sure that the timer is properly repeating.
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ((frame_args.interval * 2).InMicroseconds(),
              task_runner().DelayToNextTaskTime().InMicroseconds())
        << scheduler_->AsValue()->ToString();
    task_runner().RunPendingTasks();
    EXPECT_GT(client->num_actions_(), actions_so_far);
    EXPECT_STREQ(client->Action(client->num_actions_() - 1),
                 "DidAnticipatedDrawTimeChange");
    actions_so_far = client->num_actions_();
  }

  // Do the same thing after BeginMainFrame starts but still before activation.
  scheduler_->NotifyBeginMainFrameStarted();
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ((frame_args.interval * 2).InMicroseconds(),
              task_runner().DelayToNextTaskTime().InMicroseconds())
        << scheduler_->AsValue()->ToString();
    task_runner().RunPendingTasks();
    EXPECT_GT(client->num_actions_(), actions_so_far);
    EXPECT_STREQ(client->Action(client->num_actions_() - 1),
                 "DidAnticipatedDrawTimeChange");
    actions_so_far = client->num_actions_();
  }
}

TEST_F(SchedulerTest, BeginRetroFrame) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame.
  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);
  client_->Reset();

  // Create a BeginFrame with a long deadline to avoid race conditions.
  // This is the first BeginFrame, which will be handled immediately.
  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, now_src());
  args.deadline += base::TimeDelta::FromHours(1);
  fake_external_begin_frame_source()->TestOnBeginFrame(args);
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // Queue BeginFrames while we are still handling the previous BeginFrame.
  args.frame_time += base::TimeDelta::FromSeconds(1);
  fake_external_begin_frame_source()->TestOnBeginFrame(args);
  args.frame_time += base::TimeDelta::FromSeconds(1);
  fake_external_begin_frame_source()->TestOnBeginFrame(args);

  // If we don't swap on the deadline, we wait for the next BeginImplFrame.
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_NO_ACTION(client_);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // NotifyReadyToCommit should trigger the commit.
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // BeginImplFrame should prepare the draw.
  task_runner().RunPendingTasks();  // Run posted BeginRetroFrame.
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // BeginImplFrame deadline should draw.
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 0, 1);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // The following BeginImplFrame deadline should SetNeedsBeginFrame(false)
  // to avoid excessive toggles.
  task_runner().RunPendingTasks();  // Run posted BeginRetroFrame.
  EXPECT_SINGLE_ACTION("WillBeginImplFrame", client_);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("SetNeedsBeginFrames(false)", client_, 0, 2);
  EXPECT_ACTION("SendBeginMainFrameNotExpectedSoon", client_, 1, 2);
  client_->Reset();
}

TEST_F(SchedulerTest, BeginRetroFrame_SwapThrottled) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  scheduler_->SetEstimatedParentDrawTime(base::TimeDelta::FromMicroseconds(1));

  // To test swap ack throttling, this test disables automatic swap acks.
  scheduler_->SetMaxSwapsPending(1);
  client_->SetAutomaticSwapAck(false);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame.
  client_->Reset();
  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);
  client_->Reset();

  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // Queue BeginFrame while we are still handling the previous BeginFrame.
  SendNextBeginFrame();
  EXPECT_NO_ACTION(client_);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // NotifyReadyToCommit should trigger the pending commit and draw.
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // Swapping will put us into a swap throttled state.
  // Run posted deadline.
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(true));
  EXPECT_ACTION("ScheduledActionAnimate", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 1, 2);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // While swap throttled, BeginRetroFrames should trigger BeginImplFrames
  // but not a BeginMainFrame or draw.
  scheduler_->SetNeedsCommit();
  scheduler_->SetNeedsRedraw();
  // Run posted BeginRetroFrame.
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(false));
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // Let time pass sufficiently beyond the regular deadline but not beyond the
  // late deadline.
  now_src()->AdvanceNow(BeginFrameArgs::DefaultInterval() -
                        base::TimeDelta::FromMicroseconds(1));
  task_runner().RunUntilTime(now_src()->Now());
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  // Take us out of a swap throttled state.
  scheduler_->DidSwapBuffersComplete();
  EXPECT_SINGLE_ACTION("ScheduledActionSendBeginMainFrame", client_);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  // Verify that the deadline was rescheduled.
  task_runner().RunUntilTime(now_src()->Now());
  EXPECT_SINGLE_ACTION("ScheduledActionDrawAndSwapIfPossible", client_);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();
}

TEST_F(SchedulerTest, RetroFrameDoesNotExpireTooEarly) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  scheduler_->SetNeedsCommit();
  EXPECT_TRUE(client_->needs_begin_frames());
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);

  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  client_->Reset();
  scheduler_->NotifyBeginMainFrameStarted();

  client_->Reset();
  SendNextBeginFrame();
  // This BeginFrame is queued up as a retro frame.
  EXPECT_NO_ACTION(client_);
  // The previous deadline is still pending.
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  client_->Reset();
  // This commit should schedule the (previous) deadline to trigger immediately.
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);

  client_->Reset();
  // The deadline task should trigger causing a draw.
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(true));
  EXPECT_ACTION("ScheduledActionAnimate", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 1, 2);

  // Keep animating.
  client_->Reset();
  scheduler_->SetNeedsAnimate();
  scheduler_->SetNeedsRedraw();
  EXPECT_NO_ACTION(client_);

  // Let's advance sufficiently past the next frame's deadline.
  now_src()->AdvanceNow(BeginFrameArgs::DefaultInterval() -
                        BeginFrameArgs::DefaultEstimatedParentDrawTime() +
                        base::TimeDelta::FromMicroseconds(1));

  // The retro frame hasn't expired yet.
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(false));
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  // This is an immediate deadline case.
  client_->Reset();
  task_runner().RunPendingTasks();
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_SINGLE_ACTION("ScheduledActionDrawAndSwapIfPossible", client_);
}

TEST_F(SchedulerTest, RetroFrameDoesNotExpireTooLate) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  scheduler_->SetNeedsCommit();
  EXPECT_TRUE(client_->needs_begin_frames());
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);

  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  client_->Reset();
  scheduler_->NotifyBeginMainFrameStarted();

  client_->Reset();
  SendNextBeginFrame();
  // This BeginFrame is queued up as a retro frame.
  EXPECT_NO_ACTION(client_);
  // The previous deadline is still pending.
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  client_->Reset();
  // This commit should schedule the (previous) deadline to trigger immediately.
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);

  client_->Reset();
  // The deadline task should trigger causing a draw.
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(true));
  EXPECT_ACTION("ScheduledActionAnimate", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 1, 2);

  // Keep animating.
  client_->Reset();
  scheduler_->SetNeedsAnimate();
  scheduler_->SetNeedsRedraw();
  EXPECT_NO_ACTION(client_);

  // Let's advance sufficiently past the next frame's deadline.
  now_src()->AdvanceNow(BeginFrameArgs::DefaultInterval() +
                        base::TimeDelta::FromMicroseconds(1));

  // The retro frame should've expired.
  EXPECT_NO_ACTION(client_);
}

void SchedulerTest::BeginFramesNotFromClient(
    bool use_external_begin_frame_source,
    bool throttle_frame_production) {
  scheduler_settings_.use_external_begin_frame_source =
      use_external_begin_frame_source;
  scheduler_settings_.throttle_frame_production = throttle_frame_production;
  SetUpScheduler(true);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame
  // without calling SetNeedsBeginFrame.
  scheduler_->SetNeedsCommit();
  EXPECT_NO_ACTION(client_);
  client_->Reset();

  // When the client-driven BeginFrame are disabled, the scheduler posts it's
  // own BeginFrame tasks.
  task_runner().RunPendingTasks();  // Run posted BeginFrame.
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  // If we don't swap on the deadline, we wait for the next BeginFrame.
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_NO_ACTION(client_);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  // NotifyReadyToCommit should trigger the commit.
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);
  client_->Reset();

  // BeginImplFrame should prepare the draw.
  task_runner().RunPendingTasks();  // Run posted BeginFrame.
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  // BeginImplFrame deadline should draw.
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(true));
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 0, 1);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  // The following BeginImplFrame deadline should SetNeedsBeginFrame(false)
  // to avoid excessive toggles.
  task_runner().RunPendingTasks();  // Run posted BeginFrame.
  EXPECT_SINGLE_ACTION("WillBeginImplFrame", client_);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  // Make sure SetNeedsBeginFrame isn't called on the client
  // when the BeginFrame is no longer needed.
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_SINGLE_ACTION("SendBeginMainFrameNotExpectedSoon", client_);
  client_->Reset();
}

TEST_F(SchedulerTest, SyntheticBeginFrames) {
  bool use_external_begin_frame_source = false;
  bool throttle_frame_production = true;
  BeginFramesNotFromClient(use_external_begin_frame_source,
                           throttle_frame_production);
}

TEST_F(SchedulerTest, VSyncThrottlingDisabled) {
  bool use_external_begin_frame_source = true;
  bool throttle_frame_production = false;
  BeginFramesNotFromClient(use_external_begin_frame_source,
                           throttle_frame_production);
}

TEST_F(SchedulerTest, SyntheticBeginFrames_And_VSyncThrottlingDisabled) {
  bool use_external_begin_frame_source = false;
  bool throttle_frame_production = false;
  BeginFramesNotFromClient(use_external_begin_frame_source,
                           throttle_frame_production);
}

void SchedulerTest::BeginFramesNotFromClient_SwapThrottled(
    bool use_external_begin_frame_source,
    bool throttle_frame_production) {
  scheduler_settings_.use_external_begin_frame_source =
      use_external_begin_frame_source;
  scheduler_settings_.throttle_frame_production = throttle_frame_production;
  SetUpScheduler(true);

  scheduler_->SetEstimatedParentDrawTime(base::TimeDelta::FromMicroseconds(1));

  // To test swap ack throttling, this test disables automatic swap acks.
  scheduler_->SetMaxSwapsPending(1);
  client_->SetAutomaticSwapAck(false);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame.
  client_->Reset();
  scheduler_->SetNeedsCommit();
  EXPECT_NO_ACTION(client_);
  client_->Reset();

  // Trigger the first BeginImplFrame and BeginMainFrame
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  // NotifyReadyToCommit should trigger the pending commit and draw.
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);
  client_->Reset();

  // Swapping will put us into a swap throttled state.
  // Run posted deadline.
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(true));
  EXPECT_ACTION("ScheduledActionAnimate", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 1, 2);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  // While swap throttled, BeginFrames should trigger BeginImplFrames,
  // but not a BeginMainFrame or draw.
  scheduler_->SetNeedsCommit();
  scheduler_->SetNeedsRedraw();
  EXPECT_SCOPED(AdvanceFrame());  // Run posted BeginFrame.
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  // Let time pass sufficiently beyond the regular deadline but not beyond the
  // late deadline.
  now_src()->AdvanceNow(BeginFrameArgs::DefaultInterval() -
                        base::TimeDelta::FromMicroseconds(1));
  task_runner().RunUntilTime(now_src()->Now());
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  // Take us out of a swap throttled state.
  scheduler_->DidSwapBuffersComplete();
  EXPECT_SINGLE_ACTION("ScheduledActionSendBeginMainFrame", client_);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  // Verify that the deadline was rescheduled.
  // We can't use RunUntilTime(now) here because the next frame is also
  // scheduled if throttle_frame_production = false.
  base::TimeTicks before_deadline = now_src()->Now();
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(true));
  base::TimeTicks after_deadline = now_src()->Now();
  EXPECT_EQ(after_deadline, before_deadline);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();
}

TEST_F(SchedulerTest, SyntheticBeginFrames_SwapThrottled) {
  bool use_external_begin_frame_source = false;
  bool throttle_frame_production = true;
  BeginFramesNotFromClient_SwapThrottled(use_external_begin_frame_source,
                                         throttle_frame_production);
}

TEST_F(SchedulerTest, VSyncThrottlingDisabled_SwapThrottled) {
  bool use_external_begin_frame_source = true;
  bool throttle_frame_production = false;
  BeginFramesNotFromClient_SwapThrottled(use_external_begin_frame_source,
                                         throttle_frame_production);
}

TEST_F(SchedulerTest,
       SyntheticBeginFrames_And_VSyncThrottlingDisabled_SwapThrottled) {
  bool use_external_begin_frame_source = false;
  bool throttle_frame_production = false;
  BeginFramesNotFromClient_SwapThrottled(use_external_begin_frame_source,
                                         throttle_frame_production);
}

TEST_F(SchedulerTest, DidLoseOutputSurfaceAfterOutputSurfaceIsInitialized) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(false);

  scheduler_->SetCanStart();
  scheduler_->SetVisible(true);
  scheduler_->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client_);
  client_->Reset();
  scheduler_->DidCreateAndInitializeOutputSurface();
  EXPECT_NO_ACTION(client_);

  scheduler_->DidLoseOutputSurface();
  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client_);
}

TEST_F(SchedulerTest, DidLoseOutputSurfaceAfterBeginFrameStarted) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  // SetNeedsCommit should begin the frame.
  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);

  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  client_->Reset();
  scheduler_->DidLoseOutputSurface();
  // SetNeedsBeginFrames(false) is not called until the end of the frame.
  EXPECT_NO_ACTION(client_);

  client_->Reset();
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_ACTION("ScheduledActionCommit", client_, 0, 1);

  client_->Reset();
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(true));
  EXPECT_ACTION("ScheduledActionBeginOutputSurfaceCreation", client_, 0, 3);
  EXPECT_ACTION("SetNeedsBeginFrames(false)", client_, 1, 3);
  EXPECT_ACTION("SendBeginMainFrameNotExpectedSoon", client_, 2, 3);
}

void SchedulerTest::DidLoseOutputSurfaceAfterBeginFrameStartedWithHighLatency(
    bool impl_side_painting) {
  scheduler_settings_.impl_side_painting = impl_side_painting;
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  // SetNeedsCommit should begin the frame.
  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);

  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  client_->Reset();
  scheduler_->DidLoseOutputSurface();
  // Do nothing when impl frame is in deadine pending state.
  EXPECT_NO_ACTION(client_);

  client_->Reset();
  // Run posted deadline.
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(true));
  // OnBeginImplFrameDeadline didn't schedule output surface creation because
  // main frame is not yet completed.
  EXPECT_ACTION("SetNeedsBeginFrames(false)", client_, 0, 2);
  EXPECT_ACTION("SendBeginMainFrameNotExpectedSoon", client_, 1, 2);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());

  // BeginImplFrame is not started.
  client_->Reset();
  task_runner().RunUntilTime(now_src()->Now() +
                             base::TimeDelta::FromMilliseconds(10));
  EXPECT_NO_ACTION(client_);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());

  client_->Reset();
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  if (impl_side_painting) {
    EXPECT_ACTION("ScheduledActionCommit", client_, 0, 3);
    EXPECT_ACTION("ScheduledActionActivateSyncTree", client_, 1, 3);
    EXPECT_ACTION("ScheduledActionBeginOutputSurfaceCreation", client_, 2, 3);
  } else {
    EXPECT_ACTION("ScheduledActionCommit", client_, 0, 2);
    EXPECT_ACTION("ScheduledActionBeginOutputSurfaceCreation", client_, 1, 2);
  }
}

TEST_F(SchedulerTest,
       DidLoseOutputSurfaceAfterBeginFrameStartedWithHighLatency) {
  bool impl_side_painting = false;
  DidLoseOutputSurfaceAfterBeginFrameStartedWithHighLatency(impl_side_painting);
}

TEST_F(SchedulerTest,
       DidLoseOutputSurfaceAfterBeginFrameStartedWithHighLatencyWithImplPaint) {
  bool impl_side_painting = true;
  DidLoseOutputSurfaceAfterBeginFrameStartedWithHighLatency(impl_side_painting);
}

void SchedulerTest::DidLoseOutputSurfaceAfterReadyToCommit(
    bool impl_side_painting) {
  scheduler_settings_.impl_side_painting = impl_side_painting;
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  // SetNeedsCommit should begin the frame.
  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);

  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  client_->Reset();
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);

  client_->Reset();
  scheduler_->DidLoseOutputSurface();
  // SetNeedsBeginFrames(false) is not called until the end of the frame.
  if (impl_side_painting) {
    // Sync tree should be forced to activate.
    EXPECT_SINGLE_ACTION("ScheduledActionActivateSyncTree", client_);
  } else {
    EXPECT_NO_ACTION(client_);
  }

  client_->Reset();
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(true));
  EXPECT_ACTION("ScheduledActionBeginOutputSurfaceCreation", client_, 0, 3);
  EXPECT_ACTION("SetNeedsBeginFrames(false)", client_, 1, 3);
  EXPECT_ACTION("SendBeginMainFrameNotExpectedSoon", client_, 2, 3);
}

TEST_F(SchedulerTest, DidLoseOutputSurfaceAfterReadyToCommit) {
  DidLoseOutputSurfaceAfterReadyToCommit(false);
}

TEST_F(SchedulerTest, DidLoseOutputSurfaceAfterReadyToCommitWithImplPainting) {
  DidLoseOutputSurfaceAfterReadyToCommit(true);
}

TEST_F(SchedulerTest, DidLoseOutputSurfaceAfterSetNeedsPrepareTiles) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  scheduler_->SetNeedsPrepareTiles();
  scheduler_->SetNeedsRedraw();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);

  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  client_->Reset();
  scheduler_->DidLoseOutputSurface();
  // SetNeedsBeginFrames(false) is not called until the end of the frame.
  EXPECT_NO_ACTION(client_);

  client_->Reset();
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(true));
  EXPECT_ACTION("ScheduledActionPrepareTiles", client_, 0, 4);
  EXPECT_ACTION("ScheduledActionBeginOutputSurfaceCreation", client_, 1, 4);
  EXPECT_ACTION("SetNeedsBeginFrames(false)", client_, 2, 4);
  EXPECT_ACTION("SendBeginMainFrameNotExpectedSoon", client_, 3, 4);
}

TEST_F(SchedulerTest, DidLoseOutputSurfaceAfterBeginRetroFramePosted) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame.
  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);

  // Create a BeginFrame with a long deadline to avoid race conditions.
  // This is the first BeginFrame, which will be handled immediately.
  client_->Reset();
  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, now_src());
  args.deadline += base::TimeDelta::FromHours(1);
  fake_external_begin_frame_source()->TestOnBeginFrame(args);
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());

  // Queue BeginFrames while we are still handling the previous BeginFrame.
  args.frame_time += base::TimeDelta::FromSeconds(1);
  fake_external_begin_frame_source()->TestOnBeginFrame(args);
  args.frame_time += base::TimeDelta::FromSeconds(1);
  fake_external_begin_frame_source()->TestOnBeginFrame(args);

  // If we don't swap on the deadline, we wait for the next BeginImplFrame.
  client_->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_NO_ACTION(client_);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());

  // NotifyReadyToCommit should trigger the commit.
  client_->Reset();
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);
  EXPECT_TRUE(client_->needs_begin_frames());

  client_->Reset();
  EXPECT_FALSE(scheduler_->IsBeginRetroFrameArgsEmpty());
  scheduler_->DidLoseOutputSurface();
  EXPECT_ACTION("ScheduledActionBeginOutputSurfaceCreation", client_, 0, 3);
  EXPECT_ACTION("SetNeedsBeginFrames(false)", client_, 1, 3);
  EXPECT_ACTION("SendBeginMainFrameNotExpectedSoon", client_, 2, 3);
  EXPECT_TRUE(scheduler_->IsBeginRetroFrameArgsEmpty());

  // Posted BeginRetroFrame is aborted.
  client_->Reset();
  task_runner().RunPendingTasks();
  EXPECT_NO_ACTION(client_);
}

TEST_F(SchedulerTest, DidLoseOutputSurfaceDuringBeginRetroFrameRunning) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame.
  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);

  // Create a BeginFrame with a long deadline to avoid race conditions.
  // This is the first BeginFrame, which will be handled immediately.
  client_->Reset();
  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, now_src());
  args.deadline += base::TimeDelta::FromHours(1);
  fake_external_begin_frame_source()->TestOnBeginFrame(args);
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());

  // Queue BeginFrames while we are still handling the previous BeginFrame.
  args.frame_time += base::TimeDelta::FromSeconds(1);
  fake_external_begin_frame_source()->TestOnBeginFrame(args);
  args.frame_time += base::TimeDelta::FromSeconds(1);
  fake_external_begin_frame_source()->TestOnBeginFrame(args);

  // If we don't swap on the deadline, we wait for the next BeginImplFrame.
  client_->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_NO_ACTION(client_);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());

  // NotifyReadyToCommit should trigger the commit.
  client_->Reset();
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);
  EXPECT_TRUE(client_->needs_begin_frames());

  // BeginImplFrame should prepare the draw.
  client_->Reset();
  task_runner().RunPendingTasks();  // Run posted BeginRetroFrame.
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());

  client_->Reset();
  EXPECT_FALSE(scheduler_->IsBeginRetroFrameArgsEmpty());
  scheduler_->DidLoseOutputSurface();
  EXPECT_NO_ACTION(client_);
  EXPECT_TRUE(scheduler_->IsBeginRetroFrameArgsEmpty());

  // BeginImplFrame deadline should abort drawing.
  client_->Reset();
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(true));
  EXPECT_ACTION("ScheduledActionBeginOutputSurfaceCreation", client_, 0, 3);
  EXPECT_ACTION("SetNeedsBeginFrames(false)", client_, 1, 3);
  EXPECT_ACTION("SendBeginMainFrameNotExpectedSoon", client_, 2, 3);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_FALSE(client_->needs_begin_frames());

  // No more BeginRetroFrame because BeginRetroFrame queue is cleared.
  client_->Reset();
  task_runner().RunPendingTasks();
  EXPECT_NO_ACTION(client_);
}

TEST_F(SchedulerTest, DidLoseOutputSurfaceWithSyntheticBeginFrameSource) {
  SetUpScheduler(true);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame.
  EXPECT_FALSE(scheduler_->frame_source().NeedsBeginFrames());
  scheduler_->SetNeedsCommit();
  EXPECT_TRUE(scheduler_->frame_source().NeedsBeginFrames());

  client_->Reset();
  AdvanceFrame();
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(scheduler_->frame_source().NeedsBeginFrames());

  // NotifyReadyToCommit should trigger the commit.
  client_->Reset();
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);
  EXPECT_TRUE(scheduler_->frame_source().NeedsBeginFrames());

  client_->Reset();
  scheduler_->DidLoseOutputSurface();
  // SetNeedsBeginFrames(false) is not called until the end of the frame.
  EXPECT_NO_ACTION(client_);
  EXPECT_TRUE(scheduler_->frame_source().NeedsBeginFrames());

  client_->Reset();
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(true));
  EXPECT_ACTION("ScheduledActionBeginOutputSurfaceCreation", client_, 0, 2);
  EXPECT_ACTION("SendBeginMainFrameNotExpectedSoon", client_, 1, 2);
  EXPECT_FALSE(scheduler_->frame_source().NeedsBeginFrames());
}

TEST_F(SchedulerTest, DidLoseOutputSurfaceWhenIdle) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  // SetNeedsCommit should begin the frame.
  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);

  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  client_->Reset();
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);

  client_->Reset();
  task_runner().RunTasksWhile(client_->ImplFrameDeadlinePending(true));
  EXPECT_ACTION("ScheduledActionAnimate", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 1, 2);

  // Idle time between BeginFrames.
  client_->Reset();
  scheduler_->DidLoseOutputSurface();
  EXPECT_ACTION("ScheduledActionBeginOutputSurfaceCreation", client_, 0, 3);
  EXPECT_ACTION("SetNeedsBeginFrames(false)", client_, 1, 3);
  EXPECT_ACTION("SendBeginMainFrameNotExpectedSoon", client_, 2, 3);
}

TEST_F(SchedulerTest, ScheduledActionActivateAfterBecomingInvisible) {
  scheduler_settings_.impl_side_painting = true;
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  // SetNeedsCommit should begin the frame.
  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);

  client_->Reset();
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());

  client_->Reset();
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client_);

  client_->Reset();
  scheduler_->SetVisible(false);
  // Sync tree should be forced to activate.
  EXPECT_ACTION("SetNeedsBeginFrames(false)", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionActivateSyncTree", client_, 1, 2);
}

// Tests to ensure frame sources can be successfully changed while drawing.
TEST_F(SchedulerTest, SwitchFrameSourceToUnthrottled) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  // SetNeedsRedraw should begin the frame on the next BeginImplFrame.
  scheduler_->SetNeedsRedraw();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);
  client_->Reset();

  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 0, 1);
  scheduler_->SetNeedsRedraw();

  // Switch to an unthrottled frame source.
  scheduler_->SetThrottleFrameProduction(false);
  client_->Reset();

  // Unthrottled frame source will immediately begin a new frame.
  task_runner().RunPendingTasks();  // Run posted BeginFrame.
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  // If we don't swap on the deadline, we wait for the next BeginFrame.
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 0, 1);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();
}

// Tests to ensure frame sources can be successfully changed while a frame
// deadline is pending.
TEST_F(SchedulerTest, SwitchFrameSourceToUnthrottledBeforeDeadline) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  // SetNeedsRedraw should begin the frame on the next BeginImplFrame.
  scheduler_->SetNeedsRedraw();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);
  client_->Reset();

  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);

  // Switch to an unthrottled frame source before the frame deadline is hit.
  scheduler_->SetThrottleFrameProduction(false);
  client_->Reset();

  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();

  task_runner().RunPendingTasks();  // Run posted deadline and BeginFrame.
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 0, 2);
  // Unthrottled frame source will immediately begin a new frame.
  EXPECT_ACTION("WillBeginImplFrame", client_, 1, 2);
  scheduler_->SetNeedsRedraw();
  client_->Reset();

  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionAnimate", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 1, 2);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();
}

// Tests to ensure that the active frame source can successfully be changed from
// unthrottled to throttled.
TEST_F(SchedulerTest, SwitchFrameSourceToThrottled) {
  scheduler_settings_.throttle_frame_production = false;
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  scheduler_->SetNeedsRedraw();
  EXPECT_NO_ACTION(client_);
  client_->Reset();

  task_runner().RunPendingTasks();  // Run posted BeginFrame.
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 0, 1);
  EXPECT_FALSE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  // Switch to a throttled frame source.
  scheduler_->SetThrottleFrameProduction(true);
  client_->Reset();

  // SetNeedsRedraw should begin the frame on the next BeginImplFrame.
  scheduler_->SetNeedsRedraw();
  task_runner().RunPendingTasks();
  EXPECT_NO_ACTION(client_);
  client_->Reset();

  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 2);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 1, 2);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client_->needs_begin_frames());
  client_->Reset();
  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 0, 1);
}

// Tests to ensure that we send a BeginMainFrameNotExpectedSoon when expected.
TEST_F(SchedulerTest, SendBeginMainFrameNotExpectedSoon) {
  scheduler_settings_.use_external_begin_frame_source = true;
  SetUpScheduler(true);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame.
  scheduler_->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrames(true)", client_);
  client_->Reset();

  // Trigger a frame draw.
  EXPECT_SCOPED(AdvanceFrame());
  scheduler_->NotifyBeginMainFrameStarted();
  scheduler_->NotifyReadyToCommit();
  task_runner().RunPendingTasks();
  EXPECT_ACTION("WillBeginImplFrame", client_, 0, 5);
  EXPECT_ACTION("ScheduledActionSendBeginMainFrame", client_, 1, 5);
  EXPECT_ACTION("ScheduledActionCommit", client_, 2, 5);
  EXPECT_ACTION("ScheduledActionAnimate", client_, 3, 5);
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client_, 4, 5);
  client_->Reset();

  // The following BeginImplFrame deadline should SetNeedsBeginFrame(false)
  // and send a SendBeginMainFrameNotExpectedSoon.
  EXPECT_SCOPED(AdvanceFrame());
  EXPECT_SINGLE_ACTION("WillBeginImplFrame", client_);
  EXPECT_TRUE(scheduler_->BeginImplFrameDeadlinePending());
  client_->Reset();

  task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_ACTION("SetNeedsBeginFrames(false)", client_, 0, 2);
  EXPECT_ACTION("SendBeginMainFrameNotExpectedSoon", client_, 1, 2);
  client_->Reset();
}

}  // namespace
}  // namespace cc
