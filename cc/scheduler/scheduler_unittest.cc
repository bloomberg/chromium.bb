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
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_ACTION(action, client, action_index, expected_num_actions) \
  EXPECT_EQ(expected_num_actions, client.num_actions_());                 \
  ASSERT_LT(action_index, client.num_actions_());                         \
  do {                                                                    \
    EXPECT_STREQ(action, client.Action(action_index));                    \
    for (int i = expected_num_actions; i < client.num_actions_(); ++i)    \
      ADD_FAILURE() << "Unexpected action: " << client.Action(i) <<       \
          " with state:\n" << client.StateForAction(action_index);        \
  } while (false)

#define EXPECT_SINGLE_ACTION(action, client) \
  EXPECT_ACTION(action, client, 0, 1)

namespace cc {
namespace {

class FakeSchedulerClient;

void InitializeOutputSurfaceAndFirstCommit(Scheduler* scheduler,
                                           FakeSchedulerClient* client);

class FakeSchedulerClient : public SchedulerClient {
 public:
  FakeSchedulerClient()
  : needs_begin_impl_frame_(false) {
    Reset();
  }

  void Reset() {
    actions_.clear();
    states_.clear();
    draw_will_happen_ = true;
    swap_will_happen_if_draw_happens_ = true;
    num_draws_ = 0;
    log_anticipated_draw_time_change_ = false;
  }

  Scheduler* CreateScheduler(const SchedulerSettings& settings) {
    task_runner_ = new base::TestSimpleTaskRunner;
    scheduler_ = Scheduler::Create(this, settings, 0, task_runner_);
    return scheduler_.get();
  }

  // Most tests don't care about DidAnticipatedDrawTimeChange, so only record it
  // for tests that do.
  void set_log_anticipated_draw_time_change(bool log) {
    log_anticipated_draw_time_change_ = log;
  }
  bool needs_begin_impl_frame() { return needs_begin_impl_frame_; }
  int num_draws() const { return num_draws_; }
  int num_actions_() const { return static_cast<int>(actions_.size()); }
  const char* Action(int i) const { return actions_[i]; }
  base::Value& StateForAction(int i) const { return *states_[i]; }
  base::TimeTicks posted_begin_impl_frame_deadline() const {
    return posted_begin_impl_frame_deadline_;
  }

  base::TestSimpleTaskRunner& task_runner() { return *task_runner_; }

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

  // SchedulerClient implementation.
  virtual void SetNeedsBeginImplFrame(bool enable) OVERRIDE {
    actions_.push_back("SetNeedsBeginImplFrame");
    states_.push_back(scheduler_->StateAsValue().release());
    needs_begin_impl_frame_ = enable;
  }
  virtual void ScheduledActionSendBeginMainFrame() OVERRIDE {
    actions_.push_back("ScheduledActionSendBeginMainFrame");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapIfPossible()
      OVERRIDE {
    actions_.push_back("ScheduledActionDrawAndSwapIfPossible");
    states_.push_back(scheduler_->StateAsValue().release());
    num_draws_++;
    bool did_readback = false;
    DrawSwapReadbackResult::DrawResult result =
        draw_will_happen_
            ? DrawSwapReadbackResult::DRAW_SUCCESS
            : DrawSwapReadbackResult::DRAW_ABORTED_CHECKERBOARD_ANIMATIONS;
    return DrawSwapReadbackResult(
        result,
        draw_will_happen_ && swap_will_happen_if_draw_happens_,
        did_readback);
  }
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapForced() OVERRIDE {
    actions_.push_back("ScheduledActionDrawAndSwapForced");
    states_.push_back(scheduler_->StateAsValue().release());
    bool did_swap = swap_will_happen_if_draw_happens_;
    bool did_readback = false;
    return DrawSwapReadbackResult(
        DrawSwapReadbackResult::DRAW_SUCCESS, did_swap, did_readback);
  }
  virtual DrawSwapReadbackResult ScheduledActionDrawAndReadback() OVERRIDE {
    actions_.push_back("ScheduledActionDrawAndReadback");
    states_.push_back(scheduler_->StateAsValue().release());
    bool did_swap = false;
    bool did_readback = true;
    return DrawSwapReadbackResult(
        DrawSwapReadbackResult::DRAW_SUCCESS, did_swap, did_readback);
  }
  virtual void ScheduledActionCommit() OVERRIDE {
    actions_.push_back("ScheduledActionCommit");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual void ScheduledActionUpdateVisibleTiles() OVERRIDE {
    actions_.push_back("ScheduledActionUpdateVisibleTiles");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual void ScheduledActionActivatePendingTree() OVERRIDE {
    actions_.push_back("ScheduledActionActivatePendingTree");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual void ScheduledActionBeginOutputSurfaceCreation() OVERRIDE {
    actions_.push_back("ScheduledActionBeginOutputSurfaceCreation");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual void ScheduledActionAcquireLayerTexturesForMainThread() OVERRIDE {
    actions_.push_back("ScheduledActionAcquireLayerTexturesForMainThread");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual void ScheduledActionManageTiles() OVERRIDE {
    actions_.push_back("ScheduledActionManageTiles");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE {
    if (log_anticipated_draw_time_change_)
      actions_.push_back("DidAnticipatedDrawTimeChange");
  }
  virtual base::TimeDelta DrawDurationEstimate() OVERRIDE {
    return base::TimeDelta();
  }
  virtual base::TimeDelta BeginMainFrameToCommitDurationEstimate() OVERRIDE {
    return base::TimeDelta();
  }
  virtual base::TimeDelta CommitToActivateDurationEstimate() OVERRIDE {
    return base::TimeDelta();
  }

  virtual void DidBeginImplFrameDeadline() OVERRIDE {}

 protected:
  bool needs_begin_impl_frame_;
  bool draw_will_happen_;
  bool swap_will_happen_if_draw_happens_;
  int num_draws_;
  bool log_anticipated_draw_time_change_;
  base::TimeTicks posted_begin_impl_frame_deadline_;
  std::vector<const char*> actions_;
  ScopedVector<base::Value> states_;
  scoped_ptr<Scheduler> scheduler_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

void InitializeOutputSurfaceAndFirstCommit(Scheduler* scheduler,
                                           FakeSchedulerClient* client) {
  scheduler->DidCreateAndInitializeOutputSurface();
  scheduler->SetNeedsCommit();
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  // Go through the motions to draw the commit.
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());

  // Run the posted deadline task.
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client->task_runner().RunPendingTasks();
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // We need another BeginImplFrame so Scheduler calls
  // SetNeedsBeginImplFrame(false).
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());

  // Run the posted deadline task.
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client->task_runner().RunPendingTasks();
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
}

TEST(SchedulerTest, InitializeOutputSurfaceDoesNotBeginImplFrame) {
  FakeSchedulerClient client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  client.Reset();
  scheduler->DidCreateAndInitializeOutputSurface();
  EXPECT_EQ(0, client.num_actions_());
}

TEST(SchedulerTest, RequestCommit) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  // SetNeedsCommit should begin the frame on the next BeginImplFrame.
  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_TRUE(client.needs_begin_impl_frame());
  EXPECT_SINGLE_ACTION("SetNeedsBeginImplFrame", client);
  client.Reset();

  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("ScheduledActionSendBeginMainFrame", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
  client.Reset();

  // If we don't swap on the deadline, we need to request another
  // BeginImplFrame.
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginImplFrame", client);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
  client.Reset();

  // NotifyReadyToCommit should trigger the commit.
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_TRUE(client.needs_begin_impl_frame());
  client.Reset();

  // BeginImplFrame should prepare the draw.
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
  client.Reset();

  // BeginImplFrame deadline should draw.
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginImplFrame", client, 1, 2);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
  client.Reset();

  // The following BeginImplFrame deadline should SetNeedsBeginImplFrame(false)
  // to avoid excessive toggles.
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();

  scheduler->OnBeginImplFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginImplFrame", client);
  EXPECT_FALSE(client.needs_begin_impl_frame());
  client.Reset();
}

TEST(SchedulerTest, RequestCommitAfterBeginMainFrameSent) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);
  client.Reset();

  // SetNeedsCommit should begin the frame.
  scheduler->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginImplFrame", client);

  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("ScheduledActionSendBeginMainFrame", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  EXPECT_TRUE(client.needs_begin_impl_frame());
  client.Reset();

  // Now SetNeedsCommit again. Calling here means we need a second commit.
  scheduler->SetNeedsCommit();
  EXPECT_EQ(client.num_actions_(), 0);
  client.Reset();

  // Finish the first commit.
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginImplFrame", client, 1, 2);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // Because we just swapped, the Scheduler should also request the next
  // BeginImplFrame from the OutputSurface.
  EXPECT_TRUE(client.needs_begin_impl_frame());
  client.Reset();

  // Since another commit is needed, the next BeginImplFrame should initiate
  // the second commit.
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("ScheduledActionSendBeginMainFrame", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();

  // Finishing the commit before the deadline should post a new deadline task
  // to trigger the deadline early.
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginImplFrame", client, 1, 2);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
  client.Reset();

  // On the next BeginImplFrame, verify we go back to a quiescent state and
  // no longer request BeginImplFrames.
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_FALSE(client.needs_begin_impl_frame());
  client.Reset();
}

TEST(SchedulerTest, TextureAcquisitionCausesCommitInsteadOfDraw) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);

  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);
  client.Reset();
  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_SINGLE_ACTION("SetNeedsBeginImplFrame", client);
  EXPECT_TRUE(client.needs_begin_impl_frame());

  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginImplFrame", client, 1, 2);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());

  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginImplFrame", client);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(client.needs_begin_impl_frame());

  client.Reset();
  scheduler->SetMainThreadNeedsLayerTextures();
  EXPECT_SINGLE_ACTION("ScheduledActionAcquireLayerTexturesForMainThread",
                       client);

  // We should request a BeginImplFrame in anticipation of a draw.
  client.Reset();
  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_SINGLE_ACTION("SetNeedsBeginImplFrame", client);
  EXPECT_TRUE(client.needs_begin_impl_frame());

  // No draw happens since the textures are acquired by the main thread.
  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginImplFrame", client);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());

  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_EQ(0, client.num_actions_());

  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("ScheduledActionSendBeginMainFrame", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  // Commit will release the texture.
  client.Reset();
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(scheduler->RedrawPending());

  // Now we can draw again after the commit happens.
  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginImplFrame", client, 1, 2);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());

  // Make sure we stop requesting BeginImplFrames if we don't swap.
  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginImplFrame", client);
  EXPECT_FALSE(client.needs_begin_impl_frame());
}

TEST(SchedulerTest, TextureAcquisitionCollision) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginImplFrame", client);

  client.Reset();
  scheduler->SetMainThreadNeedsLayerTextures();
  EXPECT_SINGLE_ACTION(
      "ScheduledActionAcquireLayerTexturesForMainThread", client);

  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("ScheduledActionSendBeginMainFrame", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginImplFrame", client);

  // Although the compositor cannot draw because textures are locked by main
  // thread, we continue requesting SetNeedsBeginImplFrame in anticipation of
  // the unlock.
  EXPECT_TRUE(client.needs_begin_impl_frame());

  // Trigger the commit.
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_TRUE(client.needs_begin_impl_frame());

  // Between commit and draw, texture acquisition for main thread delayed,
  // and main thread blocks.
  client.Reset();
  scheduler->SetMainThreadNeedsLayerTextures();
  EXPECT_EQ(0, client.num_actions_());

  // No implicit commit is expected.
  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 3);
  EXPECT_ACTION(
      "ScheduledActionAcquireLayerTexturesForMainThread", client, 1, 3);
  EXPECT_ACTION("SetNeedsBeginImplFrame", client, 2, 3);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_impl_frame());

  // The compositor should not draw because textures are locked by main
  // thread.
  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginImplFrame", client);
  EXPECT_FALSE(client.needs_begin_impl_frame());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // The impl thread need an explicit commit from the main thread to lock
  // the textures.
  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("SetNeedsBeginImplFrame", client);
  EXPECT_TRUE(client.needs_begin_impl_frame());

  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("ScheduledActionSendBeginMainFrame", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();

  // Trigger the commit, which will trigger the deadline task early.
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
  client.Reset();

  // Verify we draw on the next BeginImplFrame deadline
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginImplFrame", client, 1, 2);
  EXPECT_TRUE(client.needs_begin_impl_frame());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();
}

TEST(SchedulerTest, VisibilitySwitchWithTextureAcquisition) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  client.Reset();
  scheduler->DidCreateAndInitializeOutputSurface();

  scheduler->SetNeedsCommit();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  scheduler->SetMainThreadNeedsLayerTextures();
  scheduler->SetNeedsCommit();
  client.Reset();
  // Verify that pending texture acquisition fires when visibility
  // is lost in order to avoid a deadlock.
  scheduler->SetVisible(false);
  EXPECT_SINGLE_ACTION("ScheduledActionAcquireLayerTexturesForMainThread",
                       client);

  client.Reset();
  scheduler->SetVisible(true);
  EXPECT_EQ(0, client.num_actions_());
  EXPECT_TRUE(client.needs_begin_impl_frame());

  // Regaining visibility with textures acquired by main thread while
  // compositor is waiting for first draw should result in a request
  // for a new frame in order to escape a deadlock.
  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("ScheduledActionSendBeginMainFrame", client);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
}

class SchedulerClientThatsetNeedsDrawInsideDraw : public FakeSchedulerClient {
 public:
  virtual void ScheduledActionSendBeginMainFrame() OVERRIDE {}
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapIfPossible()
      OVERRIDE {
    // Only SetNeedsRedraw the first time this is called
    if (!num_draws_)
      scheduler_->SetNeedsRedraw();
    return FakeSchedulerClient::ScheduledActionDrawAndSwapIfPossible();
  }

  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapForced() OVERRIDE {
    NOTREACHED();
    bool did_swap = true;
    bool did_readback = false;
    return DrawSwapReadbackResult(
        DrawSwapReadbackResult::DRAW_SUCCESS, did_swap, did_readback);
  }

  virtual void ScheduledActionCommit() OVERRIDE {}
  virtual void ScheduledActionBeginOutputSurfaceCreation() OVERRIDE {}
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE {}
};

// Tests for two different situations:
// 1. the scheduler dropping SetNeedsRedraw requests that happen inside
//    a ScheduledActionDrawAndSwap
// 2. the scheduler drawing twice inside a single tick
TEST(SchedulerTest, RequestRedrawInsideDraw) {
  SchedulerClientThatsetNeedsDrawInsideDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);
  client.Reset();

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
  EXPECT_EQ(0, client.num_draws());

  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());

  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(2, client.num_draws());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());

  // We stop requesting BeginImplFrames after a BeginImplFrame where we don't
  // swap.
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(2, client.num_draws());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(client.needs_begin_impl_frame());
}

// Test that requesting redraw inside a failed draw doesn't lose the request.
TEST(SchedulerTest, RequestRedrawInsideFailedDraw) {
  SchedulerClientThatsetNeedsDrawInsideDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);
  client.Reset();

  client.SetDrawWillHappen(false);

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
  EXPECT_EQ(0, client.num_draws());

  // Fail the draw.
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(1, client.num_draws());

  // We have a commit pending and the draw failed, and we didn't lose the redraw
  // request.
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());

  // Fail the draw again.
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(2, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());

  // Draw successfully.
  client.SetDrawWillHappen(true);
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(3, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
}

class SchedulerClientThatSetNeedsCommitInsideDraw : public FakeSchedulerClient {
 public:
  SchedulerClientThatSetNeedsCommitInsideDraw()
      : set_needs_commit_on_next_draw_(false) {}

  virtual void ScheduledActionSendBeginMainFrame() OVERRIDE {}
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapIfPossible()
      OVERRIDE {
    // Only SetNeedsCommit the first time this is called
    if (set_needs_commit_on_next_draw_) {
      scheduler_->SetNeedsCommit();
      set_needs_commit_on_next_draw_ = false;
    }
    return FakeSchedulerClient::ScheduledActionDrawAndSwapIfPossible();
  }

  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapForced() OVERRIDE {
    NOTREACHED();
    bool did_swap = false;
    bool did_readback = false;
    return DrawSwapReadbackResult(
        DrawSwapReadbackResult::DRAW_SUCCESS, did_swap, did_readback);
  }

  virtual void ScheduledActionCommit() OVERRIDE {}
  virtual void ScheduledActionBeginOutputSurfaceCreation() OVERRIDE {}
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE {}

  void SetNeedsCommitOnNextDraw() { set_needs_commit_on_next_draw_ = true; }

 private:
  bool set_needs_commit_on_next_draw_;
};

// Tests for the scheduler infinite-looping on SetNeedsCommit requests that
// happen inside a ScheduledActionDrawAndSwap
TEST(SchedulerTest, RequestCommitInsideDraw) {
  SchedulerClientThatSetNeedsCommitInsideDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);
  client.Reset();

  EXPECT_FALSE(client.needs_begin_impl_frame());
  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_EQ(0, client.num_draws());
  EXPECT_TRUE(client.needs_begin_impl_frame());

  client.SetNeedsCommitOnNextDraw();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  client.SetNeedsCommitOnNextDraw();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();

  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(2, client.num_draws());

  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->CommitPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());

  // We stop requesting BeginImplFrames after a BeginImplFrame where we don't
  // swap.
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(2, client.num_draws());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->CommitPending());
  EXPECT_FALSE(client.needs_begin_impl_frame());
}

// Tests that when a draw fails then the pending commit should not be dropped.
TEST(SchedulerTest, RequestCommitInsideFailedDraw) {
  SchedulerClientThatsetNeedsDrawInsideDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);
  client.Reset();

  client.SetDrawWillHappen(false);

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
  EXPECT_EQ(0, client.num_draws());

  // Fail the draw.
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(1, client.num_draws());

  // We have a commit pending and the draw failed, and we didn't lose the commit
  // request.
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());

  // Fail the draw again.
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(2, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());

  // Draw successfully.
  client.SetDrawWillHappen(true);
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(3, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
}

TEST(SchedulerTest, NoSwapWhenDrawFails) {
  SchedulerClientThatSetNeedsCommitInsideDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);
  client.Reset();

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
  EXPECT_EQ(0, client.num_draws());

  // Draw successfully, this starts a new frame.
  client.SetNeedsCommitOnNextDraw();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(1, client.num_draws());

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());

  // Fail to draw, this should not start a frame.
  client.SetDrawWillHappen(false);
  client.SetNeedsCommitOnNextDraw();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(2, client.num_draws());
}

TEST(SchedulerTest, NoSwapWhenSwapFailsDuringForcedCommit) {
  FakeSchedulerClient client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);

  // Tell the client that it will fail to swap.
  client.SetDrawWillHappen(true);
  client.SetSwapWillHappenIfDrawHappens(false);

  // Get the compositor to do a ScheduledActionDrawAndReadback.
  scheduler->SetCanDraw(true);
  scheduler->SetNeedsRedraw();
  scheduler->SetNeedsForcedCommitForReadback();
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndReadback"));
}

TEST(SchedulerTest, BackToBackReadbackAllowed) {
  // Some clients call readbacks twice in a row before the replacement
  // commit comes in.  Make sure it is allowed.
  FakeSchedulerClient client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);

  // Get the compositor to do 2 ScheduledActionDrawAndReadbacks before
  // the replacement commit comes in.
  scheduler->SetCanDraw(true);
  scheduler->SetNeedsRedraw();
  scheduler->SetNeedsForcedCommitForReadback();
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndReadback"));

  client.Reset();
  scheduler->SetNeedsForcedCommitForReadback();
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndReadback"));

  // The replacement commit comes in after 2 readbacks.
  client.Reset();
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
}


class SchedulerClientNeedsManageTilesInDraw : public FakeSchedulerClient {
 public:
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapIfPossible()
      OVERRIDE {
    scheduler_->SetNeedsManageTiles();
    return FakeSchedulerClient::ScheduledActionDrawAndSwapIfPossible();
  }
};

// Test manage tiles is independant of draws.
TEST(SchedulerTest, ManageTiles) {
  SchedulerClientNeedsManageTilesInDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  // Request both draw and manage tiles. ManageTiles shouldn't
  // be trigged until BeginImplFrame.
  client.Reset();
  scheduler->SetNeedsManageTiles();
  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(scheduler->ManageTilesPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
  EXPECT_EQ(0, client.num_draws());
  EXPECT_FALSE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_FALSE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));

  // We have no immediate actions to perform, so the BeginImplFrame should post
  // the deadline task.
  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  // On the deadline, he actions should have occured in the right order.
  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_LT(client.ActionIndex("ScheduledActionDrawAndSwapIfPossible"),
            client.ActionIndex("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // Request a draw. We don't need a ManageTiles yet.
  client.Reset();
  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());
  EXPECT_TRUE(client.needs_begin_impl_frame());
  EXPECT_EQ(0, client.num_draws());

  // We have no immediate actions to perform, so the BeginImplFrame should post
  // the deadline task.
  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  // Draw. The draw will trigger SetNeedsManageTiles, and
  // then the ManageTiles action will be triggered after the Draw.
  // Afterwards, neither a draw nor ManageTiles are pending.
  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_LT(client.ActionIndex("ScheduledActionDrawAndSwapIfPossible"),
            client.ActionIndex("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // We need a BeginImplFrame where we don't swap to go idle.
  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginImplFrame", client);
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  EXPECT_EQ(0, client.num_draws());

  // Now trigger a ManageTiles outside of a draw. We will then need
  // a begin-frame for the ManageTiles, but we don't need a draw.
  client.Reset();
  EXPECT_FALSE(client.needs_begin_impl_frame());
  scheduler->SetNeedsManageTiles();
  EXPECT_TRUE(client.needs_begin_impl_frame());
  EXPECT_TRUE(scheduler->ManageTilesPending());
  EXPECT_FALSE(scheduler->RedrawPending());

  // BeginImplFrame. There will be no draw, only ManageTiles.
  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(0, client.num_draws());
  EXPECT_FALSE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
}

// Test that ManageTiles only happens once per frame.  If an external caller
// initiates it, then the state machine should not ManageTiles on that frame.
TEST(SchedulerTest, ManageTilesOncePerFrame) {
  FakeSchedulerClient client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  // If DidManageTiles during a frame, then ManageTiles should not occur again.
  scheduler->SetNeedsManageTiles();
  scheduler->SetNeedsRedraw();
  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  EXPECT_TRUE(scheduler->ManageTilesPending());
  scheduler->DidManageTiles();  // An explicit ManageTiles.
  EXPECT_FALSE(scheduler->ManageTilesPending());

  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_FALSE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // Next frame without DidManageTiles should ManageTiles with draw.
  scheduler->SetNeedsManageTiles();
  scheduler->SetNeedsRedraw();
  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_LT(client.ActionIndex("ScheduledActionDrawAndSwapIfPossible"),
            client.ActionIndex("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  scheduler->DidManageTiles();  // Corresponds to ScheduledActionManageTiles

  // If we get another DidManageTiles within the same frame, we should
  // not ManageTiles on the next frame.
  scheduler->DidManageTiles();  // An explicit ManageTiles.
  scheduler->SetNeedsManageTiles();
  scheduler->SetNeedsRedraw();
  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  EXPECT_TRUE(scheduler->ManageTilesPending());

  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_FALSE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // If we get another DidManageTiles, we should not ManageTiles on the next
  // frame. This verifies we don't alternate calling ManageTiles once and twice.
  EXPECT_TRUE(scheduler->ManageTilesPending());
  scheduler->DidManageTiles();  // An explicit ManageTiles.
  EXPECT_FALSE(scheduler->ManageTilesPending());
  scheduler->SetNeedsManageTiles();
  scheduler->SetNeedsRedraw();
  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  EXPECT_TRUE(scheduler->ManageTilesPending());

  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_FALSE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // Next frame without DidManageTiles should ManageTiles with draw.
  scheduler->SetNeedsManageTiles();
  scheduler->SetNeedsRedraw();
  client.Reset();
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(client.num_actions_(), 0);
  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());

  client.Reset();
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_LT(client.ActionIndex("ScheduledActionDrawAndSwapIfPossible"),
            client.ActionIndex("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());
  scheduler->DidManageTiles();  // Corresponds to ScheduledActionManageTiles
}

TEST(SchedulerTest, TriggerBeginFrameDeadlineEarly) {
  SchedulerClientNeedsManageTilesInDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  client.Reset();
  BeginFrameArgs impl_frame_args = BeginFrameArgs::CreateForTesting();
  scheduler->SetNeedsRedraw();
  scheduler->BeginImplFrame(impl_frame_args);

  // The deadline should be zero since there is no work other than drawing
  // pending.
  EXPECT_EQ(base::TimeTicks(), client.posted_begin_impl_frame_deadline());
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

  virtual base::TimeDelta DrawDurationEstimate() OVERRIDE {
    return draw_duration_;
  }
  virtual base::TimeDelta BeginMainFrameToCommitDurationEstimate() OVERRIDE {
    return begin_main_frame_to_commit_duration_;
  }
  virtual base::TimeDelta CommitToActivateDurationEstimate() OVERRIDE {
    return commit_to_activate_duration_;
  }

 private:
    base::TimeDelta draw_duration_;
    base::TimeDelta begin_main_frame_to_commit_duration_;
    base::TimeDelta commit_to_activate_duration_;
};

void MainFrameInHighLatencyMode(int64 begin_main_frame_to_commit_estimate_in_ms,
                                int64 commit_to_activate_estimate_in_ms,
                                bool smoothness_takes_priority,
                                bool should_send_begin_main_frame) {
  // Set up client with specified estimates (draw duration is set to 1).
  SchedulerClientWithFixedEstimates client(
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(
          begin_main_frame_to_commit_estimate_in_ms),
      base::TimeDelta::FromMilliseconds(commit_to_activate_estimate_in_ms));
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  scheduler->SetSmoothnessTakesPriority(smoothness_takes_priority);
  InitializeOutputSurfaceAndFirstCommit(scheduler, &client);

  // Impl thread hits deadline before commit finishes.
  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_FALSE(scheduler->MainThreadIsInHighLatencyMode());
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_FALSE(scheduler->MainThreadIsInHighLatencyMode());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_TRUE(scheduler->MainThreadIsInHighLatencyMode());
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  EXPECT_TRUE(scheduler->MainThreadIsInHighLatencyMode());
  EXPECT_TRUE(client.HasAction("ScheduledActionSendBeginMainFrame"));

  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_TRUE(scheduler->MainThreadIsInHighLatencyMode());
  scheduler->BeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_TRUE(scheduler->MainThreadIsInHighLatencyMode());
  scheduler->OnBeginImplFrameDeadline();
  EXPECT_EQ(scheduler->MainThreadIsInHighLatencyMode(),
            should_send_begin_main_frame);
  EXPECT_EQ(client.HasAction("ScheduledActionSendBeginMainFrame"),
            should_send_begin_main_frame);
}

TEST(SchedulerTest,
    SkipMainFrameIfHighLatencyAndCanCommitAndActivateBeforeDeadline) {
  // Set up client so that estimates indicate that we can commit and activate
  // before the deadline (~8ms by default).
  MainFrameInHighLatencyMode(1, 1, false, false);
}

TEST(SchedulerTest, NotSkipMainFrameIfHighLatencyAndCanCommitTooLong) {
  // Set up client so that estimates indicate that the commit cannot finish
  // before the deadline (~8ms by default).
  MainFrameInHighLatencyMode(10, 1, false, true);
}

TEST(SchedulerTest, NotSkipMainFrameIfHighLatencyAndCanActivateTooLong) {
  // Set up client so that estimates indicate that the activate cannot finish
  // before the deadline (~8ms by default).
  MainFrameInHighLatencyMode(1, 10, false, true);
}

TEST(SchedulerTest, NotSkipMainFrameInPreferSmoothnessMode) {
  // Set up client so that estimates indicate that we can commit and activate
  // before the deadline (~8ms by default), but also enable smoothness takes
  // priority mode.
  MainFrameInHighLatencyMode(1, 1, true, true);
}

TEST(SchedulerTest, PollForCommitCompletion) {
  // Since we are simulating a long commit, set up a client with draw duration
  // estimates that prevent skipping main frames to get to low latency mode.
  SchedulerClientWithFixedEstimates client(
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMilliseconds(32),
      base::TimeDelta::FromMilliseconds(32));
  client.set_log_anticipated_draw_time_change(true);
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);

  scheduler->SetCanDraw(true);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->DidCreateAndInitializeOutputSurface();

  scheduler->SetNeedsCommit();
  EXPECT_TRUE(scheduler->CommitPending());
  scheduler->NotifyBeginMainFrameStarted();
  scheduler->NotifyReadyToCommit();
  scheduler->SetNeedsRedraw();

  BeginFrameArgs impl_frame_args = BeginFrameArgs::CreateForTesting();
  impl_frame_args.interval = base::TimeDelta::FromMilliseconds(1000);
  scheduler->BeginImplFrame(impl_frame_args);

  EXPECT_TRUE(scheduler->BeginImplFrameDeadlinePending());
  client.task_runner().RunPendingTasks();  // Run posted deadline.
  EXPECT_FALSE(scheduler->BeginImplFrameDeadlinePending());

  // At this point, we've drawn a frame. Start another commit, but hold off on
  // the NotifyReadyToCommit for now.
  EXPECT_FALSE(scheduler->CommitPending());
  scheduler->SetNeedsCommit();
  scheduler->BeginImplFrame(impl_frame_args);
  EXPECT_TRUE(scheduler->CommitPending());

  // Spin the event loop a few times and make sure we get more
  // DidAnticipateDrawTimeChange calls every time.
  int actions_so_far = client.num_actions_();

  // Does three iterations to make sure that the timer is properly repeating.
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ((impl_frame_args.interval * 2).InMicroseconds(),
              client.task_runner().NextPendingTaskDelay().InMicroseconds())
        << *scheduler->StateAsValue();
    client.task_runner().RunPendingTasks();
    EXPECT_GT(client.num_actions_(), actions_so_far);
    EXPECT_STREQ(client.Action(client.num_actions_() - 1),
                 "DidAnticipatedDrawTimeChange");
    actions_so_far = client.num_actions_();
  }

  // Do the same thing after BeginMainFrame starts but still before activation.
  scheduler->NotifyBeginMainFrameStarted();
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ((impl_frame_args.interval * 2).InMicroseconds(),
              client.task_runner().NextPendingTaskDelay().InMicroseconds())
        << *scheduler->StateAsValue();
    client.task_runner().RunPendingTasks();
    EXPECT_GT(client.num_actions_(), actions_so_far);
    EXPECT_STREQ(client.Action(client.num_actions_() - 1),
                 "DidAnticipatedDrawTimeChange");
    actions_so_far = client.num_actions_();
  }
}

}  // namespace
}  // namespace cc
