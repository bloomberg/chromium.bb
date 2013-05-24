// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler.h"

#include <string>
#include <vector>

#include "base/logging.h"
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

class FakeSchedulerClient : public SchedulerClient {
 public:
  FakeSchedulerClient() { Reset(); }
  void Reset() {
    actions_.clear();
    states_.clear();
    draw_will_happen_ = true;
    swap_will_happen_if_draw_happens_ = true;
    num_draws_ = 0;
  }

  Scheduler* CreateScheduler(
      scoped_ptr<FrameRateController> frame_rate_controller,
      const SchedulerSettings& settings) {
    scheduler_ =
        Scheduler::Create(this, frame_rate_controller.Pass(), settings);
    return scheduler_.get();
  }


  int num_draws() const { return num_draws_; }
  int num_actions_() const { return static_cast<int>(actions_.size()); }
  const char* Action(int i) const { return actions_[i]; }
  std::string StateForAction(int i) const { return states_[i]; }

  bool HasAction(const char* action) const {
    for (size_t i = 0; i < actions_.size(); i++)
      if (!strcmp(actions_[i], action))
        return true;
    return false;
  }

  void SetDrawWillHappen(bool draw_will_happen) {
    draw_will_happen_ = draw_will_happen;
  }
  void SetSwapWillHappenIfDrawHappens(bool swap_will_happen_if_draw_happens) {
    swap_will_happen_if_draw_happens_ = swap_will_happen_if_draw_happens;
  }

  // Scheduler Implementation.
  virtual void ScheduledActionSendBeginFrameToMainThread() OVERRIDE {
    actions_.push_back("ScheduledActionSendBeginFrameToMainThread");
    states_.push_back(scheduler_->StateAsStringForTesting());
  }
  virtual ScheduledActionDrawAndSwapResult
  ScheduledActionDrawAndSwapIfPossible() OVERRIDE {
    actions_.push_back("ScheduledActionDrawAndSwapIfPossible");
    states_.push_back(scheduler_->StateAsStringForTesting());
    num_draws_++;
    return ScheduledActionDrawAndSwapResult(draw_will_happen_,
                                            draw_will_happen_ &&
                                            swap_will_happen_if_draw_happens_);
  }
  virtual ScheduledActionDrawAndSwapResult ScheduledActionDrawAndSwapForced()
      OVERRIDE {
    actions_.push_back("ScheduledActionDrawAndSwapForced");
    states_.push_back(scheduler_->StateAsStringForTesting());
    return ScheduledActionDrawAndSwapResult(true,
                                            swap_will_happen_if_draw_happens_);
  }
  virtual void ScheduledActionCommit() OVERRIDE {
    actions_.push_back("ScheduledActionCommit");
    states_.push_back(scheduler_->StateAsStringForTesting());
  }
  virtual void ScheduledActionCheckForCompletedTileUploads() OVERRIDE {
    actions_.push_back("ScheduledActionCheckForCompletedTileUploads");
    states_.push_back(scheduler_->StateAsStringForTesting());
  }
  virtual void ScheduledActionActivatePendingTreeIfNeeded() OVERRIDE {
    actions_.push_back("ScheduledActionActivatePendingTreeIfNeeded");
    states_.push_back(scheduler_->StateAsStringForTesting());
  }
  virtual void ScheduledActionBeginOutputSurfaceCreation() OVERRIDE {
    actions_.push_back("ScheduledActionBeginOutputSurfaceCreation");
    states_.push_back(scheduler_->StateAsStringForTesting());
  }
  virtual void ScheduledActionAcquireLayerTexturesForMainThread() OVERRIDE {
    actions_.push_back("ScheduledActionAcquireLayerTexturesForMainThread");
    states_.push_back(scheduler_->StateAsStringForTesting());
  }
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE {}

 protected:
  bool draw_will_happen_;
  bool swap_will_happen_if_draw_happens_;
  int num_draws_;
  std::vector<const char*> actions_;
  std::vector<std::string> states_;
  scoped_ptr<Scheduler> scheduler_;
};

TEST(SchedulerTest, InitializeOutputSurfaceDoesNotBeginFrame) {
  FakeSchedulerClient client;
  scoped_refptr<FakeTimeSource> time_source(new FakeTimeSource());
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(
      make_scoped_ptr(new FrameRateController(time_source)),
      default_scheduler_settings);
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
  scoped_refptr<FakeTimeSource> time_source(new FakeTimeSource());
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(
      make_scoped_ptr(new FrameRateController(time_source)),
      default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  client.Reset();
  scheduler->DidCreateAndInitializeOutputSurface();

  // SetNeedsCommit should begin the frame.
  scheduler->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionSendBeginFrameToMainThread", client);
  EXPECT_FALSE(time_source->Active());
  client.Reset();

  // FinishCommit should commit
  scheduler->FinishCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_TRUE(time_source->Active());
  client.Reset();

  // Tick should draw.
  time_source->Tick();
  EXPECT_SINGLE_ACTION("ScheduledActionDrawAndSwapIfPossible", client);
  EXPECT_FALSE(time_source->Active());
  client.Reset();

  // Timer should be off.
  EXPECT_FALSE(time_source->Active());
}

TEST(SchedulerTest, RequestCommitAfterBeginFrameSentToMainThread) {
  FakeSchedulerClient client;
  scoped_refptr<FakeTimeSource> time_source(new FakeTimeSource());
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(
      make_scoped_ptr(new FrameRateController(time_source)),
      default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  client.Reset();
  scheduler->DidCreateAndInitializeOutputSurface();

  // SetNedsCommit should begin the frame.
  scheduler->SetNeedsCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionSendBeginFrameToMainThread", client);
  client.Reset();

  // Now SetNeedsCommit again. Calling here means we need a second frame.
  scheduler->SetNeedsCommit();

  // Since another commit is needed, FinishCommit should commit,
  // then begin another frame.
  scheduler->FinishCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  client.Reset();

  // Tick should draw but then begin another frame.
  time_source->Tick();
  EXPECT_FALSE(time_source->Active());
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 1, 2);
  client.Reset();
}

TEST(SchedulerTest, TextureAcquisitionCausesCommitInsteadOfDraw) {
  FakeSchedulerClient client;
  scoped_refptr<FakeTimeSource> time_source(new FakeTimeSource());
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(
      make_scoped_ptr(new FrameRateController(time_source)),
      default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  client.Reset();
  scheduler->DidCreateAndInitializeOutputSurface();

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(time_source->Active());

  time_source->Tick();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 1);
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(time_source->Active());
  client.Reset();

  scheduler->SetMainThreadNeedsLayerTextures();
  EXPECT_ACTION("ScheduledActionAcquireLayerTexturesForMainThread",
                client,
                0,
                2);
  // A commit was started by SetMainThreadNeedsLayerTextures().
  EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 1, 2);
  client.Reset();

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(time_source->Active());

  // No draw happens since the textures are acquired by the main thread.
  time_source->Tick();
  EXPECT_EQ(0, client.num_actions_());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(time_source->Active());

  scheduler->FinishCommit();
  EXPECT_ACTION("ScheduledActionCommit", client, 0, 1);
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(time_source->Active());
  client.Reset();

  // Now we can draw again after the commit happens.
  time_source->Tick();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 1);
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(time_source->Active());
  client.Reset();
}

TEST(SchedulerTest, TextureAcquisitionCollision) {
  FakeSchedulerClient client;
  scoped_refptr<FakeTimeSource> time_source(new FakeTimeSource());
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(
      make_scoped_ptr(new FrameRateController(time_source)),
      default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  client.Reset();
  scheduler->DidCreateAndInitializeOutputSurface();

  scheduler->SetNeedsCommit();
  scheduler->SetMainThreadNeedsLayerTextures();
  EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 0, 2);
  EXPECT_ACTION("ScheduledActionAcquireLayerTexturesForMainThread",
                client,
                1,
                2);
  client.Reset();

  // Compositor not scheduled to draw because textures are locked by main thread
  EXPECT_FALSE(time_source->Active());

  // Trigger the commit
  scheduler->FinishCommit();
  EXPECT_TRUE(time_source->Active());
  client.Reset();

  // Between commit and draw, texture acquisition for main thread delayed,
  // and main thread blocks.
  scheduler->SetMainThreadNeedsLayerTextures();
  EXPECT_EQ(0, client.num_actions_());
  client.Reset();

  // Once compositor draw complete, the delayed texture acquisition fires.
  time_source->Tick();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 3);
  EXPECT_ACTION("ScheduledActionAcquireLayerTexturesForMainThread",
                client,
                1,
                3);
  EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 2, 3);
  client.Reset();
}

TEST(SchedulerTest, VisibilitySwitchWithTextureAcquisition) {
  FakeSchedulerClient client;
  scoped_refptr<FakeTimeSource> time_source(new FakeTimeSource());
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(
      make_scoped_ptr(new FrameRateController(time_source)),
      default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  client.Reset();
  scheduler->DidCreateAndInitializeOutputSurface();

  scheduler->SetNeedsCommit();
  scheduler->FinishCommit();
  scheduler->SetMainThreadNeedsLayerTextures();
  client.Reset();
  // Verify that pending texture acquisition fires when visibility
  // is lost in order to avoid a deadlock.
  scheduler->SetVisible(false);
  EXPECT_SINGLE_ACTION("ScheduledActionAcquireLayerTexturesForMainThread",
                       client);
  client.Reset();

  // Regaining visibility with textures acquired by main thread while
  // compositor is waiting for first draw should result in a request
  // for a new frame in order to escape a deadlock.
  scheduler->SetVisible(true);
  EXPECT_SINGLE_ACTION("ScheduledActionSendBeginFrameToMainThread", client);
  client.Reset();
}

class SchedulerClientThatsetNeedsDrawInsideDraw : public FakeSchedulerClient {
 public:
  virtual void ScheduledActionSendBeginFrameToMainThread() OVERRIDE {}
  virtual ScheduledActionDrawAndSwapResult
  ScheduledActionDrawAndSwapIfPossible() OVERRIDE {
    // Only SetNeedsRedraw the first time this is called
    if (!num_draws_)
      scheduler_->SetNeedsRedraw();
    return FakeSchedulerClient::ScheduledActionDrawAndSwapIfPossible();
  }

  virtual ScheduledActionDrawAndSwapResult ScheduledActionDrawAndSwapForced()
      OVERRIDE {
    NOTREACHED();
    return ScheduledActionDrawAndSwapResult(true, true);
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
  scoped_refptr<FakeTimeSource> time_source(new FakeTimeSource());
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(
      make_scoped_ptr(new FrameRateController(time_source)),
      default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  scheduler->DidCreateAndInitializeOutputSurface();

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(time_source->Active());
  EXPECT_EQ(0, client.num_draws());

  time_source->Tick();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(time_source->Active());

  time_source->Tick();
  EXPECT_EQ(2, client.num_draws());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(time_source->Active());
}

// Test that requesting redraw inside a failed draw doesn't lose the request.
TEST(SchedulerTest, RequestRedrawInsideFailedDraw) {
  SchedulerClientThatsetNeedsDrawInsideDraw client;
  scoped_refptr<FakeTimeSource> time_source(new FakeTimeSource());
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(
      make_scoped_ptr(new FrameRateController(time_source)),
      default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  scheduler->DidCreateAndInitializeOutputSurface();

  client.SetDrawWillHappen(false);

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(time_source->Active());
  EXPECT_EQ(0, client.num_draws());

  // Fail the draw.
  time_source->Tick();
  EXPECT_EQ(1, client.num_draws());

  // We have a commit pending and the draw failed, and we didn't lose the redraw
  // request.
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(time_source->Active());

  // Fail the draw again.
  time_source->Tick();
  EXPECT_EQ(2, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(time_source->Active());

  // Draw successfully.
  client.SetDrawWillHappen(true);
  time_source->Tick();
  EXPECT_EQ(3, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(time_source->Active());
}

class SchedulerClientThatsetNeedsCommitInsideDraw : public FakeSchedulerClient {
 public:
  virtual void ScheduledActionSendBeginFrameToMainThread() OVERRIDE {}
  virtual ScheduledActionDrawAndSwapResult
  ScheduledActionDrawAndSwapIfPossible() OVERRIDE {
    // Only SetNeedsCommit the first time this is called
    if (!num_draws_)
      scheduler_->SetNeedsCommit();
    return FakeSchedulerClient::ScheduledActionDrawAndSwapIfPossible();
  }

  virtual ScheduledActionDrawAndSwapResult ScheduledActionDrawAndSwapForced()
      OVERRIDE {
    NOTREACHED();
    return ScheduledActionDrawAndSwapResult(true, true);
  }

  virtual void ScheduledActionCommit() OVERRIDE {}
  virtual void ScheduledActionBeginOutputSurfaceCreation() OVERRIDE {}
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE {}
};

// Tests for the scheduler infinite-looping on SetNeedsCommit requests that
// happen inside a ScheduledActionDrawAndSwap
TEST(SchedulerTest, RequestCommitInsideDraw) {
  SchedulerClientThatsetNeedsCommitInsideDraw client;
  scoped_refptr<FakeTimeSource> time_source(new FakeTimeSource());
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(
      make_scoped_ptr(new FrameRateController(time_source)),
      default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  scheduler->DidCreateAndInitializeOutputSurface();

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_EQ(0, client.num_draws());
  EXPECT_TRUE(time_source->Active());

  time_source->Tick();
  EXPECT_FALSE(time_source->Active());
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  scheduler->FinishCommit();

  time_source->Tick();
  EXPECT_EQ(2, client.num_draws());
  EXPECT_FALSE(time_source->Active());
  EXPECT_FALSE(scheduler->RedrawPending());
}

// Tests that when a draw fails then the pending commit should not be dropped.
TEST(SchedulerTest, RequestCommitInsideFailedDraw) {
  SchedulerClientThatsetNeedsDrawInsideDraw client;
  scoped_refptr<FakeTimeSource> time_source(new FakeTimeSource());
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(
      make_scoped_ptr(new FrameRateController(time_source)),
      default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  scheduler->DidCreateAndInitializeOutputSurface();

  client.SetDrawWillHappen(false);

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(time_source->Active());
  EXPECT_EQ(0, client.num_draws());

  // Fail the draw.
  time_source->Tick();
  EXPECT_EQ(1, client.num_draws());

  // We have a commit pending and the draw failed, and we didn't lose the commit
  // request.
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(time_source->Active());

  // Fail the draw again.
  time_source->Tick();
  EXPECT_EQ(2, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(time_source->Active());

  // Draw successfully.
  client.SetDrawWillHappen(true);
  time_source->Tick();
  EXPECT_EQ(3, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(time_source->Active());
}

TEST(SchedulerTest, NoSwapWhenDrawFails) {
  scoped_refptr<FakeTimeSource> time_source(new FakeTimeSource());
  SchedulerClientThatsetNeedsCommitInsideDraw client;
  scoped_ptr<FakeFrameRateController> controller(
      new FakeFrameRateController(time_source));
  FakeFrameRateController* controller_ptr = controller.get();
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(
      controller.PassAs<FrameRateController>(),
      default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  scheduler->DidCreateAndInitializeOutputSurface();

  EXPECT_EQ(0, controller_ptr->NumFramesPending());

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(time_source->Active());
  EXPECT_EQ(0, client.num_draws());

  // Draw successfully, this starts a new frame.
  time_source->Tick();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_EQ(1, controller_ptr->NumFramesPending());
  scheduler->DidSwapBuffersComplete();
  EXPECT_EQ(0, controller_ptr->NumFramesPending());

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(time_source->Active());

  // Fail to draw, this should not start a frame.
  client.SetDrawWillHappen(false);
  time_source->Tick();
  EXPECT_EQ(2, client.num_draws());
  EXPECT_EQ(0, controller_ptr->NumFramesPending());
}

TEST(SchedulerTest, NoSwapWhenSwapFailsDuringForcedCommit) {
  scoped_refptr<FakeTimeSource> time_source(new FakeTimeSource());
  FakeSchedulerClient client;
  scoped_ptr<FakeFrameRateController> controller(
      new FakeFrameRateController(time_source));
  FakeFrameRateController* controller_ptr = controller.get();
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(
      controller.PassAs<FrameRateController>(),
      default_scheduler_settings);

  EXPECT_EQ(0, controller_ptr->NumFramesPending());

  // Tell the client that it will fail to swap.
  client.SetDrawWillHappen(true);
  client.SetSwapWillHappenIfDrawHappens(false);

  // Get the compositor to do a ScheduledActionDrawAndSwapForced.
  scheduler->SetNeedsRedraw();
  scheduler->SetNeedsForcedRedraw();
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapForced"));

  // We should not have told the frame rate controller that we began a frame.
  EXPECT_EQ(0, controller_ptr->NumFramesPending());
}

TEST(SchedulerTest, RecreateOutputSurfaceClearsPendingDrawCount) {
  scoped_refptr<FakeTimeSource> time_source(new FakeTimeSource());
  FakeSchedulerClient client;
  scoped_ptr<FakeFrameRateController> controller(
      new FakeFrameRateController(time_source));
  FakeFrameRateController* controller_ptr = controller.get();
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(
      controller.PassAs<FrameRateController>(),
      default_scheduler_settings);

  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  scheduler->DidCreateAndInitializeOutputSurface();

  // Draw successfully, this starts a new frame.
  scheduler->SetNeedsRedraw();
  time_source->Tick();
  EXPECT_EQ(1, controller_ptr->NumFramesPending());

  scheduler->DidLoseOutputSurface();
  // Verifying that it's 1 so that we know that it's reset on recreate.
  EXPECT_EQ(1, controller_ptr->NumFramesPending());

  scheduler->DidCreateAndInitializeOutputSurface();
  EXPECT_EQ(0, controller_ptr->NumFramesPending());
}

}  // namespace
}  // namespace cc
