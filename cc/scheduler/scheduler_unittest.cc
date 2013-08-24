// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_vector.h"
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

void InitializeOutputSurfaceAndFirstCommit(Scheduler* scheduler) {
  scheduler->DidCreateAndInitializeOutputSurface();
  scheduler->SetNeedsCommit();
  scheduler->FinishCommit();
  // Go through the motions to draw the commit.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  // We need another BeginFrame so scheduler calls SetNeedsBeginFrame(false).
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
}

class FakeSchedulerClient : public SchedulerClient {
 public:
  FakeSchedulerClient()
  : needs_begin_frame_(false) {
    Reset();
  }

  void Reset() {
    actions_.clear();
    states_.clear();
    draw_will_happen_ = true;
    swap_will_happen_if_draw_happens_ = true;
    num_draws_ = 0;
  }

  Scheduler* CreateScheduler(const SchedulerSettings& settings) {
    scheduler_ = Scheduler::Create(this, settings);
    return scheduler_.get();
  }

  bool needs_begin_frame() { return needs_begin_frame_; }
  int num_draws() const { return num_draws_; }
  int num_actions_() const { return static_cast<int>(actions_.size()); }
  const char* Action(int i) const { return actions_[i]; }
  base::Value& StateForAction(int i) const { return *states_[i]; }

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
  virtual void SetNeedsBeginFrameOnImplThread(bool enable) OVERRIDE {
    actions_.push_back("SetNeedsBeginFrameOnImplThread");
    states_.push_back(scheduler_->StateAsValueForTesting().release());
    needs_begin_frame_ = enable;
  }
  virtual void ScheduledActionSendBeginFrameToMainThread() OVERRIDE {
    actions_.push_back("ScheduledActionSendBeginFrameToMainThread");
    states_.push_back(scheduler_->StateAsValueForTesting().release());
  }
  virtual ScheduledActionDrawAndSwapResult
  ScheduledActionDrawAndSwapIfPossible() OVERRIDE {
    actions_.push_back("ScheduledActionDrawAndSwapIfPossible");
    states_.push_back(scheduler_->StateAsValueForTesting().release());
    num_draws_++;
    return ScheduledActionDrawAndSwapResult(draw_will_happen_,
                                            draw_will_happen_ &&
                                            swap_will_happen_if_draw_happens_);
  }
  virtual ScheduledActionDrawAndSwapResult ScheduledActionDrawAndSwapForced()
      OVERRIDE {
    actions_.push_back("ScheduledActionDrawAndSwapForced");
    states_.push_back(scheduler_->StateAsValueForTesting().release());
    return ScheduledActionDrawAndSwapResult(true,
                                            swap_will_happen_if_draw_happens_);
  }
  virtual void ScheduledActionCommit() OVERRIDE {
    actions_.push_back("ScheduledActionCommit");
    states_.push_back(scheduler_->StateAsValueForTesting().release());
  }
  virtual void ScheduledActionUpdateVisibleTiles() OVERRIDE {
    actions_.push_back("ScheduledActionUpdateVisibleTiles");
    states_.push_back(scheduler_->StateAsValueForTesting().release());
  }
  virtual void ScheduledActionActivatePendingTreeIfNeeded() OVERRIDE {
    actions_.push_back("ScheduledActionActivatePendingTreeIfNeeded");
    states_.push_back(scheduler_->StateAsValueForTesting().release());
  }
  virtual void ScheduledActionBeginOutputSurfaceCreation() OVERRIDE {
    actions_.push_back("ScheduledActionBeginOutputSurfaceCreation");
    states_.push_back(scheduler_->StateAsValueForTesting().release());
  }
  virtual void ScheduledActionAcquireLayerTexturesForMainThread() OVERRIDE {
    actions_.push_back("ScheduledActionAcquireLayerTexturesForMainThread");
    states_.push_back(scheduler_->StateAsValueForTesting().release());
  }
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE {}
  virtual base::TimeDelta DrawDurationEstimate() OVERRIDE {
    return base::TimeDelta();
  }
  virtual base::TimeDelta BeginFrameToCommitDurationEstimate() OVERRIDE {
    return base::TimeDelta();
  }
  virtual base::TimeDelta CommitToActivateDurationEstimate() OVERRIDE {
    return base::TimeDelta();
  }

 protected:
  bool needs_begin_frame_;
  bool draw_will_happen_;
  bool swap_will_happen_if_draw_happens_;
  int num_draws_;
  std::vector<const char*> actions_;
  ScopedVector<base::Value> states_;
  scoped_ptr<Scheduler> scheduler_;
};

TEST(SchedulerTest, InitializeOutputSurfaceDoesNotBeginFrame) {
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
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  InitializeOutputSurfaceAndFirstCommit(scheduler);

  // SetNeedsCommit should begin the frame.
  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // FinishCommit should commit
  scheduler->FinishCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // BeginFrame should draw.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();
}

TEST(SchedulerTest, RequestCommitAfterBeginFrameSentToMainThread) {
  FakeSchedulerClient client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  InitializeOutputSurfaceAndFirstCommit(scheduler);
  client.Reset();

  // SetNedsCommit should begin the frame.
  scheduler->SetNeedsCommit();
  EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
  client.Reset();

  // Now SetNeedsCommit again. Calling here means we need a second frame.
  scheduler->SetNeedsCommit();

  // Finish the commit for the first frame.
  scheduler->FinishCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  client.Reset();

  // Tick should draw but then begin another frame for the second commit.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 1, 2);
  client.Reset();

  // Finish the second commit to go back to quiescent state and verify we no
  // longer request BeginFrames.
  scheduler->FinishCommit();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_FALSE(client.needs_begin_frame());
}

TEST(SchedulerTest, TextureAcquisitionCausesCommitInsteadOfDraw) {
  FakeSchedulerClient client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);

  InitializeOutputSurfaceAndFirstCommit(scheduler);
  client.Reset();
  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);
  EXPECT_TRUE(client.needs_begin_frame());

  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(client.needs_begin_frame());

  client.Reset();
  scheduler->SetMainThreadNeedsLayerTextures();
  EXPECT_SINGLE_ACTION("ScheduledActionAcquireLayerTexturesForMainThread",
                       client);

  // We should request a BeginFrame in anticipation of a draw.
  client.Reset();
  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);
  EXPECT_TRUE(client.needs_begin_frame());

  // No draw happens since the textures are acquired by the main thread.
  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  scheduler->SetNeedsCommit();
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 0, 2);
  EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 1, 2);
  EXPECT_TRUE(client.needs_begin_frame());

  // Commit will release the texture.
  client.Reset();
  scheduler->FinishCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Now we can draw again after the commit happens.
  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();
}

TEST(SchedulerTest, TextureAcquisitionCollision) {
  FakeSchedulerClient client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  InitializeOutputSurfaceAndFirstCommit(scheduler);

  client.Reset();
  scheduler->SetNeedsCommit();
  scheduler->SetMainThreadNeedsLayerTextures();
  EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 0, 3);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 3);
  EXPECT_ACTION(
      "ScheduledActionAcquireLayerTexturesForMainThread", client, 2, 3);
  client.Reset();

  // Although the compositor cannot draw because textures are locked by main
  // thread, we continue requesting SetNeedsBeginFrame in anticipation of the
  // unlock.
  EXPECT_TRUE(client.needs_begin_frame());

  // Trigger the commit
  scheduler->FinishCommit();
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // Between commit and draw, texture acquisition for main thread delayed,
  // and main thread blocks.
  scheduler->SetMainThreadNeedsLayerTextures();
  EXPECT_EQ(0, client.num_actions_());
  client.Reset();

  // No implicit commit is expected.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 3);
  EXPECT_ACTION("ScheduledActionAcquireLayerTexturesForMainThread",
                client,
                1,
                3);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 2, 3);
  client.Reset();

  // Compositor not scheduled to draw because textures are locked by main
  // thread.
  EXPECT_FALSE(client.needs_begin_frame());

  // Needs an explicit commit from the main thread.
  scheduler->SetNeedsCommit();
  EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
  client.Reset();

  // Trigger the commit
  scheduler->FinishCommit();
  EXPECT_TRUE(client.needs_begin_frame());
}

TEST(SchedulerTest, VisibilitySwitchWithTextureAcquisition) {
  FakeSchedulerClient client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  client.Reset();
  scheduler->DidCreateAndInitializeOutputSurface();

  scheduler->SetNeedsCommit();
  scheduler->FinishCommit();
  scheduler->SetMainThreadNeedsLayerTextures();
  scheduler->SetNeedsCommit();
  client.Reset();
  // Verify that pending texture acquisition fires when visibility
  // is lost in order to avoid a deadlock.
  scheduler->SetVisible(false);
  EXPECT_SINGLE_ACTION("ScheduledActionAcquireLayerTexturesForMainThread",
                       client);
  client.Reset();

  // Already sent a begin frame on this current frame, so wait.
  scheduler->SetVisible(true);
  EXPECT_EQ(0, client.num_actions_());
  client.Reset();

  // Regaining visibility with textures acquired by main thread while
  // compositor is waiting for first draw should result in a request
  // for a new frame in order to escape a deadlock.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
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
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler);
  client.Reset();

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_EQ(0, client.num_draws());

  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(2, client.num_draws());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(client.needs_begin_frame());
}

// Test that requesting redraw inside a failed draw doesn't lose the request.
TEST(SchedulerTest, RequestRedrawInsideFailedDraw) {
  SchedulerClientThatsetNeedsDrawInsideDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler);
  client.Reset();

  client.SetDrawWillHappen(false);

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_EQ(0, client.num_draws());

  // Fail the draw.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(1, client.num_draws());

  // We have a commit pending and the draw failed, and we didn't lose the redraw
  // request.
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Fail the draw again.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(2, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Draw successfully.
  client.SetDrawWillHappen(true);
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(3, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());
}

class SchedulerClientThatSetNeedsCommitInsideDraw : public FakeSchedulerClient {
 public:
  SchedulerClientThatSetNeedsCommitInsideDraw()
      : set_needs_commit_on_next_draw_(false) {}

  virtual void ScheduledActionSendBeginFrameToMainThread() OVERRIDE {}
  virtual ScheduledActionDrawAndSwapResult
  ScheduledActionDrawAndSwapIfPossible() OVERRIDE {
    // Only SetNeedsCommit the first time this is called
    if (set_needs_commit_on_next_draw_) {
      scheduler_->SetNeedsCommit();
      set_needs_commit_on_next_draw_ = false;
    }
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
  InitializeOutputSurfaceAndFirstCommit(scheduler);
  client.Reset();

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_EQ(0, client.num_draws());
  EXPECT_TRUE(client.needs_begin_frame());

  client.SetNeedsCommitOnNextDraw();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(client.needs_begin_frame());
  scheduler->FinishCommit();

  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(2, client.num_draws());;
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->CommitPending());
  EXPECT_FALSE(client.needs_begin_frame());
}

// Tests that when a draw fails then the pending commit should not be dropped.
TEST(SchedulerTest, RequestCommitInsideFailedDraw) {
  SchedulerClientThatsetNeedsDrawInsideDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler);
  client.Reset();

  client.SetDrawWillHappen(false);

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_EQ(0, client.num_draws());

  // Fail the draw.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(1, client.num_draws());

  // We have a commit pending and the draw failed, and we didn't lose the commit
  // request.
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Fail the draw again.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(2, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Draw successfully.
  client.SetDrawWillHappen(true);
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(3, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());
}

TEST(SchedulerTest, NoSwapWhenDrawFails) {
  SchedulerClientThatSetNeedsCommitInsideDraw client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);
  InitializeOutputSurfaceAndFirstCommit(scheduler);
  client.Reset();

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_EQ(0, client.num_draws());

  // Draw successfully, this starts a new frame.
  client.SetNeedsCommitOnNextDraw();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(1, client.num_draws());

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Fail to draw, this should not start a frame.
  client.SetDrawWillHappen(false);
  client.SetNeedsCommitOnNextDraw();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(2, client.num_draws());
}

TEST(SchedulerTest, NoSwapWhenSwapFailsDuringForcedCommit) {
  FakeSchedulerClient client;
  SchedulerSettings default_scheduler_settings;
  Scheduler* scheduler = client.CreateScheduler(default_scheduler_settings);

  // Tell the client that it will fail to swap.
  client.SetDrawWillHappen(true);
  client.SetSwapWillHappenIfDrawHappens(false);

  // Get the compositor to do a ScheduledActionDrawAndSwapForced.
  scheduler->SetCanDraw(true);
  scheduler->SetNeedsRedraw();
  scheduler->SetNeedsForcedRedraw();
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapForced"));
}

}  // namespace
}  // namespace cc
