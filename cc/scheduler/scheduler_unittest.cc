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
  scheduler->OnBeginFrameDeadline();
  // We need another BeginFrame so Scheduler calls SetNeedsBeginFrame(false).
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginFrameDeadline();
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

  // Scheduler Implementation.
  virtual void SetNeedsBeginFrameOnImplThread(bool enable) OVERRIDE {
    actions_.push_back("SetNeedsBeginFrameOnImplThread");
    states_.push_back(scheduler_->StateAsValue().release());
    needs_begin_frame_ = enable;
  }
  virtual void ScheduledActionSendBeginFrameToMainThread() OVERRIDE {
    actions_.push_back("ScheduledActionSendBeginFrameToMainThread");
    states_.push_back(scheduler_->StateAsValue().release());
  }
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapIfPossible()
      OVERRIDE {
    actions_.push_back("ScheduledActionDrawAndSwapIfPossible");
    states_.push_back(scheduler_->StateAsValue().release());
    num_draws_++;
    bool did_readback = false;
    return DrawSwapReadbackResult(
        draw_will_happen_,
        draw_will_happen_ && swap_will_happen_if_draw_happens_,
        did_readback);
  }
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapForced() OVERRIDE {
    actions_.push_back("ScheduledActionDrawAndSwapForced");
    states_.push_back(scheduler_->StateAsValue().release());
    bool did_draw = true;
    bool did_swap = swap_will_happen_if_draw_happens_;
    bool did_readback = false;
    return DrawSwapReadbackResult(did_draw, did_swap, did_readback);
  }
  virtual DrawSwapReadbackResult ScheduledActionDrawAndReadback() OVERRIDE {
    actions_.push_back("ScheduledActionDrawAndReadback");
    states_.push_back(scheduler_->StateAsValue().release());
    bool did_draw = true;
    bool did_swap = false;
    bool did_readback = true;
    return DrawSwapReadbackResult(did_draw, did_swap, did_readback);
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

  virtual void PostBeginFrameDeadline(const base::Closure& closure,
                                      base::TimeTicks deadline) OVERRIDE {
    actions_.push_back("PostBeginFrameDeadlineTask");
    states_.push_back(scheduler_->StateAsValue().release());
  }

  virtual void DidBeginFrameDeadlineOnImplThread() OVERRIDE {}

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

void RequestCommit(bool deadline_scheduling_enabled) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  scheduler_settings.deadline_scheduling_enabled = deadline_scheduling_enabled;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  InitializeOutputSurfaceAndFirstCommit(scheduler);

  // SetNeedsCommit should begin the frame on the next BeginFrame.
  client.Reset();
  scheduler->SetNeedsCommit();
  EXPECT_TRUE(client.needs_begin_frame());
  if (deadline_scheduling_enabled) {
    EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);
  } else {
    EXPECT_EQ(client.num_actions_(), 2);
    EXPECT_TRUE(client.HasAction("ScheduledActionSendBeginFrameToMainThread"));
    EXPECT_TRUE(client.HasAction("SetNeedsBeginFrameOnImplThread"));
  }
  client.Reset();

  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  if (deadline_scheduling_enabled) {
    EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 0, 2);
    EXPECT_ACTION("PostBeginFrameDeadlineTask", client, 1, 2);
  } else {
    EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  }
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // If we don't swap on the deadline, we need to request another BeginFrame.
  scheduler->OnBeginFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // FinishCommit should commit
  scheduler->FinishCommit();
  EXPECT_SINGLE_ACTION("ScheduledActionCommit", client);
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // BeginFrame should prepare the draw.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // BeginFrame deadline should draw.
  scheduler->OnBeginFrameDeadline();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // The following BeginFrame deadline should SetNeedsBeginFrame(false) to avoid
  // excessive toggles.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  client.Reset();

  scheduler->OnBeginFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();
}

TEST(SchedulerTest, RequestCommit) {
  bool deadline_scheduling_enabled = false;
  RequestCommit(deadline_scheduling_enabled);
}

TEST(SchedulerTest, RequestCommit_Deadline) {
  bool deadline_scheduling_enabled = true;
  RequestCommit(deadline_scheduling_enabled);
}

void RequestCommitAfterBeginFrameSentToMainThread(
    bool deadline_scheduling_enabled) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  scheduler_settings.deadline_scheduling_enabled = deadline_scheduling_enabled;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  InitializeOutputSurfaceAndFirstCommit(scheduler);
  client.Reset();

  // SetNeedsCommit should begin the frame.
  scheduler->SetNeedsCommit();
  if (deadline_scheduling_enabled) {
    EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);
  } else {
    EXPECT_EQ(client.num_actions_(), 2);
    EXPECT_TRUE(client.HasAction("SetNeedsBeginFrameOnImplThread"));
    EXPECT_TRUE(client.HasAction("ScheduledActionSendBeginFrameToMainThread"));
  }

  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  if (deadline_scheduling_enabled) {
    EXPECT_EQ(client.num_actions_(), 2);
    EXPECT_TRUE(client.HasAction("ScheduledActionSendBeginFrameToMainThread"));
    EXPECT_TRUE(client.HasAction("PostBeginFrameDeadlineTask"));
  } else {
    EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  }

  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // Now SetNeedsCommit again. Calling here means we need a second commit.
  scheduler->SetNeedsCommit();
  EXPECT_EQ(client.num_actions_(), 0);
  client.Reset();

  // Finish the first commit.
  scheduler->FinishCommit();
  EXPECT_ACTION("ScheduledActionCommit", client, 0, 2);
  EXPECT_ACTION("PostBeginFrameDeadlineTask", client, 1, 2);
  client.Reset();
  scheduler->OnBeginFrameDeadline();
  if (deadline_scheduling_enabled) {
    EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
    EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
  } else {
    EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 3);
    EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 1, 3);
    EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 2, 3);
  }

  // Because we just swapped, the Scheduler should also request the next
  // BeginFrame from the OutputSurface.
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // Since another commit is needed, the next BeginFrame should initiate
  // the second commit.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  if (deadline_scheduling_enabled) {
    EXPECT_EQ(client.num_actions_(), 2);
    EXPECT_TRUE(client.HasAction("ScheduledActionSendBeginFrameToMainThread"));
    EXPECT_TRUE(client.HasAction("PostBeginFrameDeadlineTask"));
  } else {
    EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  }
  client.Reset();

  // Finishing the commit before the deadline should post a new deadline task
  // to trigger the deadline early.
  scheduler->FinishCommit();
  EXPECT_ACTION("ScheduledActionCommit", client, 0, 2);
  EXPECT_ACTION("PostBeginFrameDeadlineTask", client, 1, 2);
  client.Reset();
  scheduler->OnBeginFrameDeadline();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // On the next BeginFrame, verify we go back to a quiescent state and
  // no longer request BeginFrames.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginFrameDeadline();
  EXPECT_FALSE(client.needs_begin_frame());
  client.Reset();
}

TEST(SchedulerTest, RequestCommitAfterBeginFrameSentToMainThread) {
  bool deadline_scheduling_enabled = false;
  RequestCommitAfterBeginFrameSentToMainThread(deadline_scheduling_enabled);
}

TEST(SchedulerTest, RequestCommitAfterBeginFrameSentToMainThread_Deadline) {
  bool deadline_scheduling_enabled = true;
  RequestCommitAfterBeginFrameSentToMainThread(deadline_scheduling_enabled);
}

void TextureAcquisitionCausesCommitInsteadOfDraw(
    bool deadline_scheduling_enabled) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  scheduler_settings.deadline_scheduling_enabled = deadline_scheduling_enabled;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
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
  EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  client.Reset();
  scheduler->OnBeginFrameDeadline();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  client.Reset();
  scheduler->OnBeginFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);
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
  EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  client.Reset();
  scheduler->OnBeginFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  client.Reset();
  scheduler->SetNeedsCommit();
  if (deadline_scheduling_enabled) {
    EXPECT_EQ(0, client.num_actions_());
  } else {
    EXPECT_SINGLE_ACTION("ScheduledActionSendBeginFrameToMainThread", client);
  }

  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  if (deadline_scheduling_enabled) {
    EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 0, 2);
    EXPECT_ACTION("PostBeginFrameDeadlineTask", client, 1, 2);
  } else {
    EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  }

  // Commit will release the texture.
  client.Reset();
  scheduler->FinishCommit();
  EXPECT_ACTION("ScheduledActionCommit", client, 0, 2);
  EXPECT_ACTION("PostBeginFrameDeadlineTask", client, 1, 2);
  EXPECT_TRUE(scheduler->RedrawPending());

  // Now we can draw again after the commit happens.
  client.Reset();
  scheduler->OnBeginFrameDeadline();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Make sure we stop requesting BeginFrames if we don't swap.
  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  client.Reset();
  scheduler->OnBeginFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);
  EXPECT_FALSE(client.needs_begin_frame());
}

TEST(SchedulerTest, TextureAcquisitionCausesCommitInsteadOfDraw) {
  bool deadline_scheduling_enabled = false;
  TextureAcquisitionCausesCommitInsteadOfDraw(deadline_scheduling_enabled);
}

TEST(SchedulerTest, TextureAcquisitionCausesCommitInsteadOfDraw_Deadline) {
  bool deadline_scheduling_enabled = true;
  TextureAcquisitionCausesCommitInsteadOfDraw(deadline_scheduling_enabled);
}

void TextureAcquisitionCollision(bool deadline_scheduling_enabled) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  scheduler_settings.deadline_scheduling_enabled = deadline_scheduling_enabled;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  InitializeOutputSurfaceAndFirstCommit(scheduler);

  client.Reset();
  scheduler->SetNeedsCommit();
if (deadline_scheduling_enabled) {
    EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);
  } else {
    EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 0, 2);
    EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
  }

  client.Reset();
  scheduler->SetMainThreadNeedsLayerTextures();
  EXPECT_SINGLE_ACTION(
      "ScheduledActionAcquireLayerTexturesForMainThread", client);

  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  if (deadline_scheduling_enabled) {
    EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 0, 2);
    EXPECT_ACTION("PostBeginFrameDeadlineTask", client, 1, 2);
  } else {
    EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  }

  client.Reset();
  scheduler->OnBeginFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);

  // Although the compositor cannot draw because textures are locked by main
  // thread, we continue requesting SetNeedsBeginFrame in anticipation of the
  // unlock.
  EXPECT_TRUE(client.needs_begin_frame());

  // Trigger the commit
  scheduler->FinishCommit();
  EXPECT_TRUE(client.needs_begin_frame());

  // Between commit and draw, texture acquisition for main thread delayed,
  // and main thread blocks.
  client.Reset();
  scheduler->SetMainThreadNeedsLayerTextures();
  EXPECT_EQ(0, client.num_actions_());

  // No implicit commit is expected.
  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);

  client.Reset();
  scheduler->OnBeginFrameDeadline();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 3);
  EXPECT_ACTION(
      "ScheduledActionAcquireLayerTexturesForMainThread", client, 1, 3);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 2, 3);
  EXPECT_TRUE(client.needs_begin_frame());

  // The compositor should not draw because textures are locked by main
  // thread.
  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  client.Reset();
  scheduler->OnBeginFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);
  EXPECT_FALSE(client.needs_begin_frame());

  // The impl thread need an explicit commit from the main thread to lock
  // the textures.
  client.Reset();
  scheduler->SetNeedsCommit();
  if (deadline_scheduling_enabled) {
    EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);
  } else {
    EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 0, 2);
    EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
  }
  EXPECT_TRUE(client.needs_begin_frame());

  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  if (deadline_scheduling_enabled) {
    EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 0, 2);
    EXPECT_ACTION("PostBeginFrameDeadlineTask", client, 1, 2);
  } else {
    EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  }
  client.Reset();

  // Trigger the commit, which will trigger the deadline task early.
  scheduler->FinishCommit();
  EXPECT_ACTION("ScheduledActionCommit", client, 0, 2);
  EXPECT_ACTION("PostBeginFrameDeadlineTask", client, 1, 2);
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();

  // Verify we draw on the next BeginFrame deadline
  scheduler->OnBeginFrameDeadline();
  EXPECT_ACTION("ScheduledActionDrawAndSwapIfPossible", client, 0, 2);
  EXPECT_ACTION("SetNeedsBeginFrameOnImplThread", client, 1, 2);
  EXPECT_TRUE(client.needs_begin_frame());
  client.Reset();
}

TEST(SchedulerTest, TextureAcquisitionCollision) {
  bool deadline_scheduling_enabled = false;
  TextureAcquisitionCollision(deadline_scheduling_enabled);
}

TEST(SchedulerTest, TextureAcquisitionCollision_Deadline) {
  bool deadline_scheduling_enabled = true;
  TextureAcquisitionCollision(deadline_scheduling_enabled);
}

void VisibilitySwitchWithTextureAcquisition(bool deadline_scheduling_enabled) {
  FakeSchedulerClient client;
  SchedulerSettings scheduler_settings;
  scheduler_settings.deadline_scheduling_enabled = deadline_scheduling_enabled;
  Scheduler* scheduler = client.CreateScheduler(scheduler_settings);
  scheduler->SetCanStart();
  scheduler->SetVisible(true);
  scheduler->SetCanDraw(true);

  EXPECT_SINGLE_ACTION("ScheduledActionBeginOutputSurfaceCreation", client);
  client.Reset();
  scheduler->DidCreateAndInitializeOutputSurface();

  scheduler->SetNeedsCommit();
  if (deadline_scheduling_enabled) {
    scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
    scheduler->OnBeginFrameDeadline();
  }
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
  scheduler->SetVisible(true);
  EXPECT_EQ(0, client.num_actions_());
  EXPECT_TRUE(client.needs_begin_frame());

  // Regaining visibility with textures acquired by main thread while
  // compositor is waiting for first draw should result in a request
  // for a new frame in order to escape a deadlock.
  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION("ScheduledActionSendBeginFrameToMainThread", client, 0, 2);
  EXPECT_ACTION("PostBeginFrameDeadlineTask", client, 1, 2);
}

TEST(SchedulerTest, VisibilitySwitchWithTextureAcquisition) {
  bool deadline_scheduling_enabled = false;
  VisibilitySwitchWithTextureAcquisition(deadline_scheduling_enabled);
}

TEST(SchedulerTest, VisibilitySwitchWithTextureAcquisition_Deadline) {
  bool deadline_scheduling_enabled = true;
  VisibilitySwitchWithTextureAcquisition(deadline_scheduling_enabled);
}

class SchedulerClientThatsetNeedsDrawInsideDraw : public FakeSchedulerClient {
 public:
  virtual void ScheduledActionSendBeginFrameToMainThread() OVERRIDE {}
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapIfPossible()
      OVERRIDE {
    // Only SetNeedsRedraw the first time this is called
    if (!num_draws_)
      scheduler_->SetNeedsRedraw();
    return FakeSchedulerClient::ScheduledActionDrawAndSwapIfPossible();
  }

  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapForced() OVERRIDE {
    NOTREACHED();
    bool did_draw = true;
    bool did_swap = true;
    bool did_readback = false;
    return DrawSwapReadbackResult(did_draw, did_swap, did_readback);
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
  scheduler->OnBeginFrameDeadline();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginFrameDeadline();
  EXPECT_EQ(2, client.num_draws());
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // We stop requesting BeginFrames after a BeginFrame where we don't swap.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginFrameDeadline();
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
  scheduler->OnBeginFrameDeadline();
  EXPECT_EQ(1, client.num_draws());

  // We have a commit pending and the draw failed, and we didn't lose the redraw
  // request.
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Fail the draw again.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginFrameDeadline();
  EXPECT_EQ(2, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Draw successfully.
  client.SetDrawWillHappen(true);
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginFrameDeadline();
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
    bool did_draw = true;
    bool did_swap = false;
    bool did_readback = false;
    return DrawSwapReadbackResult(did_draw, did_swap, did_readback);
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

  EXPECT_FALSE(client.needs_begin_frame());
  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_EQ(0, client.num_draws());
  EXPECT_TRUE(client.needs_begin_frame());

  client.SetNeedsCommitOnNextDraw();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  client.SetNeedsCommitOnNextDraw();
  scheduler->OnBeginFrameDeadline();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(client.needs_begin_frame());
  scheduler->FinishCommit();

  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginFrameDeadline();
  EXPECT_EQ(2, client.num_draws());

  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->CommitPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // We stop requesting BeginFrames after a BeginFrame where we don't swap.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginFrameDeadline();
  EXPECT_EQ(2, client.num_draws());
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
  scheduler->OnBeginFrameDeadline();
  EXPECT_EQ(1, client.num_draws());

  // We have a commit pending and the draw failed, and we didn't lose the commit
  // request.
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Fail the draw again.
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginFrameDeadline();
  EXPECT_EQ(2, client.num_draws());
  EXPECT_TRUE(scheduler->CommitPending());
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Draw successfully.
  client.SetDrawWillHappen(true);
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginFrameDeadline();
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
  scheduler->OnBeginFrameDeadline();
  EXPECT_EQ(1, client.num_draws());

  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(client.needs_begin_frame());

  // Fail to draw, this should not start a frame.
  client.SetDrawWillHappen(false);
  client.SetNeedsCommitOnNextDraw();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  scheduler->OnBeginFrameDeadline();
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
  scheduler->FinishCommit();
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
  scheduler->FinishCommit();
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndReadback"));

  client.Reset();
  scheduler->SetNeedsForcedCommitForReadback();
  scheduler->FinishCommit();
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndReadback"));

  // The replacement commit comes in after 2 readbacks.
  client.Reset();
  scheduler->FinishCommit();
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
  InitializeOutputSurfaceAndFirstCommit(scheduler);

  // Request both draw and manage tiles. ManageTiles shouldn't
  // be trigged until BeginFrame.
  client.Reset();
  scheduler->SetNeedsManageTiles();
  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_TRUE(scheduler->ManageTilesPending());
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_EQ(0, client.num_draws());
  EXPECT_FALSE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_FALSE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));

  // We have no immediate actions to perform, so the BeginFrame should post
  // the deadline task.
  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);

  // On the deadline, he actions should have occured in the right order.
  client.Reset();
  scheduler->OnBeginFrameDeadline();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_LT(client.ActionIndex("ScheduledActionDrawAndSwapIfPossible"),
            client.ActionIndex("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());

  // Request a draw. We don't need a ManageTiles yet.
  client.Reset();
  scheduler->SetNeedsRedraw();
  EXPECT_TRUE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_EQ(0, client.num_draws());

  // We have no immediate actions to perform, so the BeginFrame should post
  // the deadline task.
  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);

  // Draw. The draw will trigger SetNeedsManageTiles, and
  // then the ManageTiles action will be triggered after the Draw.
  // Afterwards, neither a draw nor ManageTiles are pending.
  client.Reset();
  scheduler->OnBeginFrameDeadline();
  EXPECT_EQ(1, client.num_draws());
  EXPECT_TRUE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client.HasAction("ScheduledActionManageTiles"));
  EXPECT_LT(client.ActionIndex("ScheduledActionDrawAndSwapIfPossible"),
            client.ActionIndex("ScheduledActionManageTiles"));
  EXPECT_FALSE(scheduler->RedrawPending());
  EXPECT_FALSE(scheduler->ManageTilesPending());

  // We need a BeginFrame where we don't swap to go idle.
  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  client.Reset();
  scheduler->OnBeginFrameDeadline();
  EXPECT_SINGLE_ACTION("SetNeedsBeginFrameOnImplThread", client);;
  EXPECT_EQ(0, client.num_draws());

  // Now trigger a ManageTiles outside of a draw. We will then need
  // a begin-frame for the ManageTiles, but we don't need a draw.
  client.Reset();
  EXPECT_FALSE(client.needs_begin_frame());
  scheduler->SetNeedsManageTiles();
  EXPECT_TRUE(client.needs_begin_frame());
  EXPECT_TRUE(scheduler->ManageTilesPending());
  EXPECT_FALSE(scheduler->RedrawPending());

  // BeginFrame. There will be no draw, only ManageTiles.
  client.Reset();
  scheduler->BeginFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_SINGLE_ACTION("PostBeginFrameDeadlineTask", client);
  client.Reset();
  scheduler->OnBeginFrameDeadline();
  EXPECT_EQ(0, client.num_draws());
  EXPECT_FALSE(client.HasAction("ScheduledActionDrawAndSwapIfPossible"));
  EXPECT_TRUE(client.HasAction("ScheduledActionManageTiles"));
}

}  // namespace
}  // namespace cc
