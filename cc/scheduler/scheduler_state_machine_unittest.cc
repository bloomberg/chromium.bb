// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler_state_machine.h"

#include "cc/scheduler/scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_ACTION_UPDATE_STATE(action)                                   \
  EXPECT_EQ(action, state.NextAction()) << *state.AsValue();                 \
  if (action == SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE ||   \
      action == SchedulerStateMachine::ACTION_DRAW_AND_SWAP_FORCED) {        \
    EXPECT_EQ(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE, \
              state.begin_impl_frame_state())                                \
        << *state.AsValue();                                                 \
  }                                                                          \
  state.UpdateState(action);                                                 \
  if (action == SchedulerStateMachine::ACTION_NONE) {                        \
    if (state.begin_impl_frame_state() ==                                    \
        SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING)  \
      state.OnBeginImplFrameDeadlinePending();                               \
    if (state.begin_impl_frame_state() ==                                    \
        SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE)       \
      state.OnBeginImplFrameIdle();                                          \
  }

namespace cc {

namespace {

const SchedulerStateMachine::BeginImplFrameState all_begin_impl_frame_states[] =
    {SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE,
     SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING,
     SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME,
     SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE, };

const SchedulerStateMachine::CommitState all_commit_states[] = {
    SchedulerStateMachine::COMMIT_STATE_IDLE,
    SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
    SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_STARTED,
    SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT,
    SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_ACTIVATION,
    SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW};

// Exposes the protected state fields of the SchedulerStateMachine for testing
class StateMachine : public SchedulerStateMachine {
 public:
  explicit StateMachine(const SchedulerSettings& scheduler_settings)
      : SchedulerStateMachine(scheduler_settings) {}

  void CreateAndInitializeOutputSurfaceWithActivatedCommit() {
    DidCreateAndInitializeOutputSurface();
    output_surface_state_ = OUTPUT_SURFACE_ACTIVE;
  }

  void SetCommitState(CommitState cs) { commit_state_ = cs; }
  CommitState CommitState() const { return commit_state_; }

  ForcedRedrawOnTimeoutState ForcedRedrawState() const {
    return forced_redraw_state_;
  }

  void SetBeginImplFrameState(BeginImplFrameState bifs) {
    begin_impl_frame_state_ = bifs;
  }

  BeginImplFrameState begin_impl_frame_state() const {
    return begin_impl_frame_state_;
  }

  OutputSurfaceState output_surface_state() const {
    return output_surface_state_;
  }

  void SetReadbackState(SynchronousReadbackState rs) { readback_state_ = rs; }
  SynchronousReadbackState readback_state() const { return readback_state_; }

  bool NeedsCommit() const { return needs_commit_; }

  void SetNeedsRedraw(bool b) { needs_redraw_ = b; }

  void SetNeedsForcedRedrawForTimeout(bool b) {
    forced_redraw_state_ = FORCED_REDRAW_STATE_WAITING_FOR_COMMIT;
    active_tree_needs_first_draw_ = true;
  }
  bool NeedsForcedRedrawForTimeout() const {
    return forced_redraw_state_ != FORCED_REDRAW_STATE_IDLE;
  }

  void SetNeedsForcedRedrawForReadback() {
    readback_state_ = READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK;
    active_tree_needs_first_draw_ = true;
  }

  bool NeedsForcedRedrawForReadback() const {
    return readback_state_ != READBACK_STATE_IDLE;
  }

  void SetActiveTreeNeedsFirstDraw(bool needs_first_draw) {
    active_tree_needs_first_draw_ = needs_first_draw;
  }

  bool CanDraw() const { return can_draw_; }
  bool Visible() const { return visible_; }

  bool PendingActivationsShouldBeForced() const {
    return SchedulerStateMachine::PendingActivationsShouldBeForced();
  }

  void SetHasPendingTree(bool has_pending_tree) {
    has_pending_tree_ = has_pending_tree;
  }
};

TEST(SchedulerStateMachineTest, TestNextActionBeginsMainFrameIfNeeded) {
  SchedulerSettings default_scheduler_settings;

  // If no commit needed, do nothing.
  {
    StateMachine state(default_scheduler_settings);
    state.SetCanStart();
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION)
    state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
    state.SetCommitState(SchedulerStateMachine::COMMIT_STATE_IDLE);
    state.SetNeedsRedraw(false);
    state.SetVisible(true);

    EXPECT_FALSE(state.BeginFrameNeeded());

    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    EXPECT_FALSE(state.BeginFrameNeeded());
    state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());

    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    state.OnBeginImplFrameDeadline();
  }

  // If commit requested but can_start is still false, do nothing.
  {
    StateMachine state(default_scheduler_settings);
    state.SetCommitState(SchedulerStateMachine::COMMIT_STATE_IDLE);
    state.SetNeedsRedraw(false);
    state.SetVisible(true);
    state.SetNeedsCommit();

    EXPECT_FALSE(state.BeginFrameNeeded());

    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    EXPECT_FALSE(state.BeginFrameNeeded());
    state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    state.OnBeginImplFrameDeadline();
  }

  // If commit requested, begin a main frame.
  {
    StateMachine state(default_scheduler_settings);
    state.SetCommitState(SchedulerStateMachine::COMMIT_STATE_IDLE);
    state.SetCanStart();
    state.UpdateState(state.NextAction());
    state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
    state.SetNeedsRedraw(false);
    state.SetVisible(true);
    state.SetNeedsCommit();

    EXPECT_TRUE(state.BeginFrameNeeded());

    state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  }

  // Begin the frame, make sure needs_commit and commit_state update correctly.
  {
    StateMachine state(default_scheduler_settings);
    state.SetCanStart();
    state.UpdateState(state.NextAction());
    state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
    state.SetVisible(true);
    state.UpdateState(SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
              state.CommitState());
    EXPECT_FALSE(state.NeedsCommit());
  }
}

// Explicitly test main_frame_before_draw_enabled = false
TEST(SchedulerStateMachineTest, MainFrameBeforeDrawDisabled) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.impl_side_painting = true;
  scheduler_settings.main_frame_before_draw_enabled = false;
  StateMachine state(scheduler_settings);
  state.SetCommitState(SchedulerStateMachine::COMMIT_STATE_IDLE);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetNeedsRedraw(false);
  state.SetVisible(true);
  state.SetCanDraw(true);
  state.SetNeedsCommit();

  EXPECT_TRUE(state.BeginFrameNeeded());

  // Commit to the pending tree.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_EQ(state.CommitState(),
            SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW);

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_EQ(state.CommitState(),
            SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW);

  // Verify that the next commit doesn't start until the previous
  // commit has been drawn.
  state.SetNeedsCommit();
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Verify NotifyReadyToActivate unblocks activation, draw, and
  // commit in that order.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_ACTIVATE_PENDING_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_EQ(state.CommitState(),
            SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW);

  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineEarly());
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  EXPECT_EQ(state.CommitState(), SchedulerStateMachine::COMMIT_STATE_IDLE);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_EQ(state.CommitState(),
            SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT);

  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_EQ(state.CommitState(),
            SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW);
}

// Explicitly test main_frame_before_activation_enabled = true
TEST(SchedulerStateMachineTest, MainFrameBeforeActivationEnabled) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.impl_side_painting = true;
  scheduler_settings.main_frame_before_activation_enabled = true;
  StateMachine state(scheduler_settings);
  state.SetCommitState(SchedulerStateMachine::COMMIT_STATE_IDLE);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetNeedsRedraw(false);
  state.SetVisible(true);
  state.SetCanDraw(true);
  state.SetNeedsCommit();

  EXPECT_TRUE(state.BeginFrameNeeded());

  // Commit to the pending tree.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_EQ(state.CommitState(), SchedulerStateMachine::COMMIT_STATE_IDLE);

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Verify that the next commit starts while there is still a pending tree.
  state.SetNeedsCommit();
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Verify the pending commit doesn't overwrite the pending
  // tree until the pending tree has been activated.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Verify NotifyReadyToActivate unblocks activation, draw, and
  // commit in that order.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_ACTIVATE_PENDING_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineEarly());
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_EQ(state.CommitState(), SchedulerStateMachine::COMMIT_STATE_IDLE);
}

TEST(SchedulerStateMachineTest,
     TestFailedDrawForAnimationCheckerboardSetsNeedsCommitAndDoesNotDrawAgain) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);
  state.SetNeedsRedraw(true);
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();

  // We're drawing now.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  EXPECT_FALSE(state.RedrawPending());
  EXPECT_FALSE(state.CommitPending());

  // Failing the draw makes us require a commit.
  state.DidDrawIfPossibleCompleted(
      DrawSwapReadbackResult::DRAW_ABORTED_CHECKERBOARD_ANIMATIONS);
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_TRUE(state.CommitPending());
}

TEST(SchedulerStateMachineTest, TestFailedDrawForMissingHighResNeedsCommit) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);
  state.SetNeedsRedraw(true);
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_TRUE(state.BeginFrameNeeded());

  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.RedrawPending());
  EXPECT_FALSE(state.CommitPending());

  // Missing high res content requires a commit (but not a redraw)
  state.DidDrawIfPossibleCompleted(
      DrawSwapReadbackResult::DRAW_ABORTED_MISSING_HIGH_RES_CONTENT);
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_FALSE(state.RedrawPending());
  EXPECT_TRUE(state.CommitPending());
}

TEST(SchedulerStateMachineTest,
     TestsetNeedsRedrawDuringFailedDrawDoesNotRemoveNeedsRedraw) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();

  state.SetVisible(true);
  state.SetCanDraw(true);
  state.SetNeedsRedraw(true);
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();

  // We're drawing now.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.RedrawPending());
  EXPECT_FALSE(state.CommitPending());

  // While still in the same BeginMainFrame callback on the main thread,
  // set needs redraw again. This should not redraw.
  state.SetNeedsRedraw(true);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Failing the draw for animation checkerboards makes us require a commit.
  state.DidDrawIfPossibleCompleted(
      DrawSwapReadbackResult::DRAW_ABORTED_CHECKERBOARD_ANIMATIONS);
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_TRUE(state.RedrawPending());
}

void TestFailedDrawsEventuallyForceDrawAfterNextCommit(
    bool main_frame_before_draw_enabled) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.main_frame_before_draw_enabled =
      main_frame_before_draw_enabled;
  scheduler_settings.maximum_number_of_failed_draws_before_draw_is_forced_ = 1;
  StateMachine state(scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // Start a commit.
  state.SetNeedsCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.CommitPending());

  // Then initiate a draw.
  state.SetNeedsRedraw(true);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);

  // Fail the draw.
  state.DidDrawIfPossibleCompleted(
      DrawSwapReadbackResult::DRAW_ABORTED_CHECKERBOARD_ANIMATIONS);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.BeginFrameNeeded());
  EXPECT_TRUE(state.RedrawPending());
  // But the commit is ongoing.
  EXPECT_TRUE(state.CommitPending());

  // Finish the commit. Note, we should not yet be forcing a draw, but should
  // continue the commit as usual.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.RedrawPending());

  // The redraw should be forced at the end of the next BeginImplFrame.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  if (main_frame_before_draw_enabled) {
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  }
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_FORCED);
}

TEST(SchedulerStateMachineTest,
     TestFailedDrawsEventuallyForceDrawAfterNextCommit) {
  bool main_frame_before_draw_enabled = false;
  TestFailedDrawsEventuallyForceDrawAfterNextCommit(
      main_frame_before_draw_enabled);
}

TEST(SchedulerStateMachineTest,
     TestFailedDrawsEventuallyForceDrawAfterNextCommit_CommitBeforeDraw) {
  bool main_frame_before_draw_enabled = true;
  TestFailedDrawsEventuallyForceDrawAfterNextCommit(
      main_frame_before_draw_enabled);
}

TEST(SchedulerStateMachineTest, TestFailedDrawsDoNotRestartForcedDraw) {
  SchedulerSettings scheduler_settings;
  int draw_limit = 1;
  scheduler_settings.maximum_number_of_failed_draws_before_draw_is_forced_ =
      draw_limit;
  scheduler_settings.impl_side_painting = true;
  StateMachine state(scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // Start a commit.
  state.SetNeedsCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.CommitPending());

  // Then initiate a draw.
  state.SetNeedsRedraw(true);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);

  // Fail the draw enough times to force a redraw,
  // then once more for good measure.
  for (int i = 0; i < draw_limit + 1; ++i) {
    state.DidDrawIfPossibleCompleted(
        DrawSwapReadbackResult::DRAW_ABORTED_CHECKERBOARD_ANIMATIONS);
  }
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.BeginFrameNeeded());
  EXPECT_TRUE(state.RedrawPending());
  // But the commit is ongoing.
  EXPECT_TRUE(state.CommitPending());
  EXPECT_TRUE(state.ForcedRedrawState() ==
              SchedulerStateMachine::FORCED_REDRAW_STATE_WAITING_FOR_COMMIT);

  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_FALSE(state.CommitPending());

  // Now force redraw should be in waiting for activation
  EXPECT_TRUE(state.ForcedRedrawState() ==
    SchedulerStateMachine::FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION);

  // After failing additional draws, we should still be in a forced
  // redraw, but not back in WAITING_FOR_COMMIT.
  for (int i = 0; i < draw_limit + 1; ++i) {
    state.DidDrawIfPossibleCompleted(
        DrawSwapReadbackResult::DRAW_ABORTED_CHECKERBOARD_ANIMATIONS);
  }
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_TRUE(state.ForcedRedrawState() ==
    SchedulerStateMachine::FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION);
}

TEST(SchedulerStateMachineTest, TestFailedDrawIsRetriedInNextBeginImplFrame) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // Start a draw.
  state.SetNeedsRedraw(true);
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);

  // Failing the draw for animation checkerboards makes us require a commit.
  state.DidDrawIfPossibleCompleted(
      DrawSwapReadbackResult::DRAW_ABORTED_CHECKERBOARD_ANIMATIONS);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.RedrawPending());

  // We should not be trying to draw again now, but we have a commit pending.
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // We should try to draw again at the end of the next BeginImplFrame on
  // the impl thread.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestDoestDrawTwiceInSameFrame) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);
  state.SetNeedsRedraw(true);

  // Draw the first frame.
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  state.DidDrawIfPossibleCompleted(DrawSwapReadbackResult::DRAW_SUCCESS);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Before the next BeginImplFrame, set needs redraw again.
  // This should not redraw until the next BeginImplFrame.
  state.SetNeedsRedraw(true);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Move to another frame. This should now draw.
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());

  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  state.DidDrawIfPossibleCompleted(DrawSwapReadbackResult::DRAW_SUCCESS);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // We just swapped, so we should proactively request another BeginImplFrame.
  EXPECT_TRUE(state.BeginFrameNeeded());
}

TEST(SchedulerStateMachineTest, TestNextActionDrawsOnBeginImplFrame) {
  SchedulerSettings default_scheduler_settings;

  // When not in BeginImplFrame deadline, or in BeginImplFrame deadline
  // but not visible, don't draw.
  size_t num_commit_states =
      sizeof(all_commit_states) / sizeof(SchedulerStateMachine::CommitState);
  size_t num_begin_impl_frame_states =
      sizeof(all_begin_impl_frame_states) /
      sizeof(SchedulerStateMachine::BeginImplFrameState);
  for (size_t i = 0; i < num_commit_states; ++i) {
    for (size_t j = 0; j < num_begin_impl_frame_states; ++j) {
      StateMachine state(default_scheduler_settings);
      state.SetCanStart();
      state.UpdateState(state.NextAction());
      state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
      state.SetCommitState(all_commit_states[i]);
      state.SetBeginImplFrameState(all_begin_impl_frame_states[j]);
      bool visible =
          (all_begin_impl_frame_states[j] !=
           SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE);
      state.SetVisible(visible);

      // Case 1: needs_commit=false
      EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE,
                state.NextAction());

      // Case 2: needs_commit=true
      state.SetNeedsCommit();
      EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE,
                state.NextAction())
          << *state.AsValue();
    }
  }

  // When in BeginImplFrame deadline we should always draw for SetNeedsRedraw
  // except if we're ready to commit, in which case we expect a commit first.
  // SetNeedsForcedRedrawForReadback should take precedence over all and
  // issue a readback.
  for (size_t i = 0; i < num_commit_states; ++i) {
    for (size_t j = 0; j < 2; ++j) {
      bool request_readback = j;

      StateMachine state(default_scheduler_settings);
      state.SetCanStart();
      state.UpdateState(state.NextAction());
      state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
      state.SetCanDraw(true);
      state.SetCommitState(all_commit_states[i]);
      state.SetBeginImplFrameState(
          SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE);
      if (request_readback) {
        state.SetNeedsForcedRedrawForReadback();
      } else {
        state.SetNeedsRedraw(true);
        state.SetVisible(true);
      }

      SchedulerStateMachine::Action expected_action;
      if (request_readback) {
        expected_action = SchedulerStateMachine::ACTION_DRAW_AND_READBACK;
      } else if (all_commit_states[i] ==
                 SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT) {
        expected_action = SchedulerStateMachine::ACTION_COMMIT;
      } else {
        expected_action =
            SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE;
      }

      // Case 1: needs_commit=false.
      EXPECT_NE(state.BeginFrameNeeded(), request_readback) << *state.AsValue();
      EXPECT_EQ(state.NextAction(), expected_action) << *state.AsValue();

      // Case 2: needs_commit=true.
      state.SetNeedsCommit();
      EXPECT_NE(state.BeginFrameNeeded(), request_readback) << *state.AsValue();
      EXPECT_EQ(state.NextAction(), expected_action) << *state.AsValue();
    }
  }
}

TEST(SchedulerStateMachineTest, TestNoCommitStatesRedrawWhenInvisible) {
  SchedulerSettings default_scheduler_settings;

  size_t num_commit_states =
      sizeof(all_commit_states) / sizeof(SchedulerStateMachine::CommitState);
  for (size_t i = 0; i < num_commit_states; ++i) {
    // There shouldn't be any drawing regardless of BeginImplFrame.
    for (size_t j = 0; j < 2; ++j) {
      StateMachine state(default_scheduler_settings);
      state.SetCanStart();
      state.UpdateState(state.NextAction());
      state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
      state.SetCommitState(all_commit_states[i]);
      state.SetVisible(false);
      state.SetNeedsRedraw(true);
      if (j == 1) {
        state.SetBeginImplFrameState(
            SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE);
      }

      // Case 1: needs_commit=false.
      EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE,
                state.NextAction());

      // Case 2: needs_commit=true.
      state.SetNeedsCommit();
      EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE,
                state.NextAction())
          << *state.AsValue();
    }
  }
}

TEST(SchedulerStateMachineTest, TestCanRedraw_StopsDraw) {
  SchedulerSettings default_scheduler_settings;

  size_t num_commit_states =
      sizeof(all_commit_states) / sizeof(SchedulerStateMachine::CommitState);
  for (size_t i = 0; i < num_commit_states; ++i) {
    // There shouldn't be any drawing regardless of BeginImplFrame.
    for (size_t j = 0; j < 2; ++j) {
      StateMachine state(default_scheduler_settings);
      state.SetCanStart();
      state.UpdateState(state.NextAction());
      state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
      state.SetCommitState(all_commit_states[i]);
      state.SetVisible(false);
      state.SetNeedsRedraw(true);
      if (j == 1)
        state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());

      state.SetCanDraw(false);
      EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE,
                state.NextAction());
    }
  }
}

TEST(SchedulerStateMachineTest,
     TestCanRedrawWithWaitingForFirstDrawMakesProgress) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();

  state.SetActiveTreeNeedsFirstDraw(true);
  state.SetNeedsCommit();
  state.SetNeedsRedraw(true);
  state.SetVisible(true);
  state.SetCanDraw(false);
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_ABORT);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_ABORT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

void TestSetNeedsCommitIsNotLost(bool main_frame_before_draw_enabled) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.main_frame_before_draw_enabled =
      main_frame_before_draw_enabled;
  StateMachine state(scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetNeedsCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  EXPECT_TRUE(state.BeginFrameNeeded());

  // Begin the frame.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
            state.CommitState());

  // Now, while the frame is in progress, set another commit.
  state.SetNeedsCommit();
  EXPECT_TRUE(state.NeedsCommit());

  // Let the frame finish.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT,
            state.CommitState());

  // Expect to commit regardless of BeginImplFrame state.
  EXPECT_EQ(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING,
            state.begin_impl_frame_state());
  EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.NextAction());

  state.OnBeginImplFrameDeadlinePending();
  EXPECT_EQ(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME,
            state.begin_impl_frame_state());
  EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.NextAction());

  state.OnBeginImplFrameDeadline();
  EXPECT_EQ(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE,
            state.begin_impl_frame_state());
  EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.NextAction());

  state.OnBeginImplFrameIdle();
  EXPECT_EQ(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE,
            state.begin_impl_frame_state());
  EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.NextAction());

  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING,
            state.begin_impl_frame_state());
  EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.NextAction());

  // Finish the commit, then make sure we start the next commit immediately
  // and draw on the next BeginImplFrame.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  if (main_frame_before_draw_enabled) {
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  }
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.OnBeginImplFrameDeadline();

  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  state.DidDrawIfPossibleCompleted(DrawSwapReadbackResult::DRAW_SUCCESS);
  if (!main_frame_before_draw_enabled) {
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  }
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestSetNeedsCommitIsNotLost) {
  bool main_frame_before_draw_enabled = false;
  TestSetNeedsCommitIsNotLost(main_frame_before_draw_enabled);
}

TEST(SchedulerStateMachineTest, TestSetNeedsCommitIsNotLost_CommitBeforeDraw) {
  bool main_frame_before_draw_enabled = true;
  TestSetNeedsCommitIsNotLost(main_frame_before_draw_enabled);
}

TEST(SchedulerStateMachineTest, TestFullCycle) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // Start clean and set commit.
  state.SetNeedsCommit();

  // Begin the frame.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
            state.CommitState());
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Tell the scheduler the frame finished.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT,
            state.CommitState());

  // Commit.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_TRUE(state.needs_redraw());

  // Expect to do nothing until BeginImplFrame deadline
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // At BeginImplFrame deadline, draw.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  state.DidDrawIfPossibleCompleted(DrawSwapReadbackResult::DRAW_SUCCESS);

  // Should be synchronized, no draw needed, no action needed.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_IDLE, state.CommitState());
  EXPECT_FALSE(state.needs_redraw());
}

TEST(SchedulerStateMachineTest, TestFullCycleWithCommitRequestInbetween) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // Start clean and set commit.
  state.SetNeedsCommit();

  // Begin the frame.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
            state.CommitState());
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Request another commit while the commit is in flight.
  state.SetNeedsCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Tell the scheduler the frame finished.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT,
            state.CommitState());

  // First commit.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_TRUE(state.needs_redraw());

  // Expect to do nothing until BeginImplFrame deadline.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // At BeginImplFrame deadline, draw.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  state.DidDrawIfPossibleCompleted(DrawSwapReadbackResult::DRAW_SUCCESS);

  // Should be synchronized, no draw needed, no action needed.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_IDLE, state.CommitState());
  EXPECT_FALSE(state.needs_redraw());

  // Next BeginImplFrame should initiate second commit.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest, TestRequestCommitInvisible) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetNeedsCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestGoesInvisibleBeforeFinishCommit) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // Start clean and set commit.
  state.SetNeedsCommit();

  // Begin the frame while visible.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
            state.CommitState());
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Become invisible and abort BeginMainFrame.
  state.SetVisible(false);
  state.BeginMainFrameAborted(false);

  // We should now be back in the idle state as if we never started the frame.
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_IDLE, state.CommitState());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // We shouldn't do anything on the BeginImplFrame deadline.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Become visible again.
  state.SetVisible(true);

  // Although we have aborted on this frame and haven't cancelled the commit
  // (i.e. need another), don't send another BeginMainFrame yet.
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_IDLE, state.CommitState());
  EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.NextAction());
  EXPECT_TRUE(state.NeedsCommit());

  // Start a new frame.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);

  // We should be starting the commit now.
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
            state.CommitState());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, AbortBeginMainFrameAndCancelCommit) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.DidCreateAndInitializeOutputSurface();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // Get into a begin frame / commit state.
  state.SetNeedsCommit();

  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
            state.CommitState());
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.NextAction());

  // Abort the commit, cancelling future commits.
  state.BeginMainFrameAborted(true);

  // Verify that another commit doesn't start on the same frame.
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_IDLE, state.CommitState());
  EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.NextAction());
  EXPECT_FALSE(state.NeedsCommit());

  // Start a new frame; draw because this is the first frame since output
  // surface init'd.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);

  // Verify another commit doesn't start on another frame either.
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_IDLE, state.CommitState());
  EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.NextAction());
  EXPECT_FALSE(state.NeedsCommit());

  // Verify another commit can start if requested, though.
  state.SetNeedsCommit();
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_IDLE, state.CommitState());
  EXPECT_EQ(SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME,
            state.NextAction());
}

TEST(SchedulerStateMachineTest, TestFirstContextCreation) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.SetVisible(true);
  state.SetCanDraw(true);

  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION);
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Check that the first init does not SetNeedsCommit.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Check that a needs commit initiates a BeginMainFrame.
  state.SetNeedsCommit();
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest, TestContextLostWhenCompletelyIdle) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();

  state.SetVisible(true);
  state.SetCanDraw(true);

  EXPECT_NE(SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION,
            state.NextAction());
  state.DidLoseOutputSurface();

  EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION,
            state.NextAction());
  state.UpdateState(state.NextAction());

  // Once context recreation begins, nothing should happen.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Recreate the context.
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();

  // When the context is recreated, we should begin a commit.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest,
     TestContextLostWhenIdleAndCommitRequestedWhileRecreating) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  EXPECT_NE(SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION,
            state.NextAction());
  state.DidLoseOutputSurface();

  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Once context recreation begins, nothing should happen.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // While context is recreating, commits shouldn't begin.
  state.SetNeedsCommit();
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Recreate the context
  state.DidCreateAndInitializeOutputSurface();
  EXPECT_FALSE(state.RedrawPending());

  // When the context is recreated, we should begin a commit
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
            state.CommitState());

  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  // Finishing the first commit after initializing an output surface should
  // automatically cause a redraw.
  EXPECT_TRUE(state.RedrawPending());

  // Once the context is recreated, whether we draw should be based on
  // SetCanDraw.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE,
            state.NextAction());
  state.SetCanDraw(false);
  EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_ABORT,
            state.NextAction());
  state.SetCanDraw(true);
  EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE,
            state.NextAction());
}

TEST(SchedulerStateMachineTest, TestContextLostWhileCommitInProgress) {
  SchedulerSettings scheduler_settings;
  StateMachine state(scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // Get a commit in flight.
  state.SetNeedsCommit();

  // Set damage and expect a draw.
  state.SetNeedsRedraw(true);
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Cause a lost context while the BeginMainFrame is in flight.
  state.DidLoseOutputSurface();

  // Ask for another draw. Expect nothing happens.
  state.SetNeedsRedraw(true);
  EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.NextAction());

  // Finish the frame, and commit.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);

  // We will abort the draw when the output surface is lost if we are
  // waiting for the first draw to unblock the main thread.
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_ABORT);

  // Expect to begin context recreation only in BEGIN_IMPL_FRAME_STATE_IDLE
  EXPECT_EQ(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE,
            state.begin_impl_frame_state());
  EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION,
            state.NextAction());

  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING,
            state.begin_impl_frame_state());
  EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.NextAction());

  state.OnBeginImplFrameDeadlinePending();
  EXPECT_EQ(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME,
            state.begin_impl_frame_state());
  EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.NextAction());

  state.OnBeginImplFrameDeadline();
  EXPECT_EQ(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE,
            state.begin_impl_frame_state());
  EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.NextAction());
}

TEST(SchedulerStateMachineTest,
     TestContextLostWhileCommitInProgressAndAnotherCommitRequested) {
  SchedulerSettings scheduler_settings;
  StateMachine state(scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // Get a commit in flight.
  state.SetNeedsCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Set damage and expect a draw.
  state.SetNeedsRedraw(true);
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Cause a lost context while the BeginMainFrame is in flight.
  state.DidLoseOutputSurface();

  // Ask for another draw and also set needs commit. Expect nothing happens.
  state.SetNeedsRedraw(true);
  state.SetNeedsCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Finish the frame, and commit.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_TRUE(state.active_tree_needs_first_draw());

  // Because the output surface is missing, we expect the draw to abort.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_ABORT);

  // Expect to begin context recreation only in BEGIN_IMPL_FRAME_STATE_IDLE
  EXPECT_EQ(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE,
            state.begin_impl_frame_state());
  EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION,
            state.NextAction());

  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING,
            state.begin_impl_frame_state());
  EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.NextAction());

  state.OnBeginImplFrameDeadlinePending();
  EXPECT_EQ(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME,
            state.begin_impl_frame_state());
  EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.NextAction());

  state.OnBeginImplFrameDeadline();
  EXPECT_EQ(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE,
            state.begin_impl_frame_state());
  EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.NextAction());

  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION);

  // After we get a new output surface, the commit flow should start.
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestFinishAllRenderingWhileContextLost) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // Cause a lost context lost.
  state.DidLoseOutputSurface();

  // Ask a forced redraw for readback and verify it ocurrs.
  state.SetNeedsForcedRedrawForReadback();
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_READBACK);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Forced redraws for readbacks need to be followed by a new commit
  // to replace the readback commit.
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
            state.CommitState());
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);

  // We don't yet have an output surface, so we the draw and swap should abort.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_ABORT);

  // Expect to begin context recreation only in BEGIN_IMPL_FRAME_STATE_IDLE
  EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.NextAction());

  state.OnBeginImplFrameDeadline();
  EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.NextAction());

  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION);

  // Ask a readback and verify it occurs.
  state.SetNeedsForcedRedrawForReadback();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_READBACK);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, DontDrawBeforeCommitAfterLostOutputSurface) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  state.SetNeedsRedraw(true);

  // Cause a lost output surface, and restore it.
  state.DidLoseOutputSurface();
  EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION,
            state.NextAction());
  state.UpdateState(state.NextAction());
  state.DidCreateAndInitializeOutputSurface();

  EXPECT_FALSE(state.RedrawPending());
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_EQ(SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME,
            state.NextAction());
}

TEST(SchedulerStateMachineTest,
     TestPendingActivationsShouldBeForcedAfterLostOutputSurface) {
  SchedulerSettings settings;
  settings.impl_side_painting = true;
  StateMachine state(settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  state.SetCommitState(
      SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT);

  // Cause a lost context.
  state.DidLoseOutputSurface();

  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);

  EXPECT_TRUE(state.PendingActivationsShouldBeForced());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_ACTIVATE_PENDING_TREE);

  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_ABORT);
}

TEST(SchedulerStateMachineTest,
     TestSendBeginMainFrameWhenInvisibleAndForceCommit) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(false);
  state.SetNeedsForcedCommitForReadback();
  EXPECT_EQ(SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME,
            state.NextAction());
}

TEST(SchedulerStateMachineTest,
     TestSendBeginMainFrameWhenCanStartFalseAndForceCommit) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  state.SetCanDraw(true);
  state.SetNeedsForcedCommitForReadback();
  EXPECT_EQ(SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME,
            state.NextAction());
}

// If new commit is not requested explicitly after starting forced commit,
// new commit should not scheduled after drawing the replacement commit.
TEST(SchedulerStateMachineTest, DontMakeNewCommitAfterDrawingReplaceCommit) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // There is a scheduled commit.
  state.SetCommitState(
      SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT);

  // Request a forced commit.
  state.SetNeedsForcedCommitForReadback();

  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_AND_READBACK,
            state.NextAction());
  state.UpdateState(state.NextAction());
  EXPECT_EQ(
      SchedulerStateMachine::READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT,
      state.readback_state());
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
            state.CommitState());

  // Finish the replacement commit.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);

  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  EXPECT_EQ(SchedulerStateMachine::READBACK_STATE_IDLE, state.readback_state());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestFinishCommitWhenCommitInProgress) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(false);
  state.SetCommitState(
      SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT);
  state.SetNeedsCommit();

  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.NextAction());
  state.UpdateState(state.NextAction());

  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_SWAP_ABORT);
}

TEST(SchedulerStateMachineTest, TestFinishCommitWhenForcedCommitInProgress) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(false);
  state.SetCommitState(
      SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT);
  state.SetNeedsForcedCommitForReadback();

  // The commit for readback interupts the normal commit.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);

  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_READBACK);

  // When the readback interrupts the normal commit, we should not get
  // another BeginMainFrame when the readback completes.
  EXPECT_NE(SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME,
            state.NextAction());

  // The normal commit can then proceed.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
}

void TestForceCommitWhenReplacementActivationInProgress(
    bool main_frame_before_draw_enabled) {
  SchedulerSettings settings;
  settings.impl_side_painting = true;
  settings.main_frame_before_draw_enabled = main_frame_before_draw_enabled;
  StateMachine state(settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // Impl-side painting of replacement commit is in-progress.
  if (settings.main_frame_before_draw_enabled) {
    state.SetCommitState(
        SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_ACTIVATION);
  } else {
    state.SetCommitState(
        SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW);
  }
  state.SetReadbackState(
      SchedulerStateMachine::READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION);
  state.SetHasPendingTree(true);

  // Forced commit is requested during the impl-side painting.
  state.SetNeedsForcedCommitForReadback();
  EXPECT_FALSE(state.NeedsCommit());

  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_ACTIVATE_PENDING_TREE);
  // New replacement commit is needed for incoming forced commit.
  EXPECT_EQ(SchedulerStateMachine::READBACK_STATE_NEEDS_BEGIN_MAIN_FRAME,
            state.readback_state());
  EXPECT_TRUE(state.NeedsCommit());
  if (settings.main_frame_before_draw_enabled) {
    // New replacement commit is scheduled.
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
    // Forced commit is started.
    EXPECT_EQ(SchedulerStateMachine::READBACK_STATE_WAITING_FOR_COMMIT,
              state.readback_state());
  }
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  // Perform the draw & swap of replacement commit.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  if (!settings.main_frame_before_draw_enabled) {
    // New replacement commit is scheduled.
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
    // Forced commit is started.
    EXPECT_EQ(SchedulerStateMachine::READBACK_STATE_WAITING_FOR_COMMIT,
              state.readback_state());
  }
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_NONE);

  // Finish the forced commit and draw it.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_ACTIVATE_PENDING_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_READBACK)
  EXPECT_EQ(
      SchedulerStateMachine::READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT,
      state.readback_state());
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
            state.CommitState());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Finish the replacement commit and draw it.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_ACTIVATE_PENDING_TREE);
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  EXPECT_EQ(SchedulerStateMachine::READBACK_STATE_IDLE, state.readback_state());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

// Explicitly test when main_frame_before_draw_enabled = true.
TEST(SchedulerStateMachineTest,
     ForceCommitWhenReplacementActivationInProgressAndMainFrameEnabled) {
  bool main_frame_before_draw_enabled = true;
  TestForceCommitWhenReplacementActivationInProgress(
      main_frame_before_draw_enabled);
}

// Explicitly test when main_frame_before_draw_enabled = false.
TEST(SchedulerStateMachineTest,
     ForceCommitWhenReplacementActivationInProgressAndMainFrameDisabled) {
  bool main_frame_before_draw_enabled = false;
  TestForceCommitWhenReplacementActivationInProgress(
      main_frame_before_draw_enabled);
}

// Test with main_frame_before_activation_enable = true;
TEST(SchedulerStateMachineTest,
     ForceCommitWhenReplacementActivationInProgressWithMFBA) {
  SchedulerSettings settings;
  settings.impl_side_painting = true;
  settings.main_frame_before_activation_enabled = true;
  StateMachine state(settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // When impl-side painting of replacement commit is in-progress, commit state
  // is idle because main_frame_before_activation is enabled.
  state.SetCommitState(
      SchedulerStateMachine::COMMIT_STATE_IDLE);
  state.SetReadbackState(
      SchedulerStateMachine::READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION);
  state.SetHasPendingTree(true);

  // New commit is requested and scheduled when impl-side painting is in
  // progress.
  state.SetNeedsCommit();
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Forced commit is requested during the impl-side painting.
  state.SetNeedsForcedCommitForReadback();
  EXPECT_FALSE(state.NeedsCommit());

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_ACTIVATE_PENDING_TREE);
  // Replacement commit for requested forced commit is already scheduled.
  EXPECT_EQ(SchedulerStateMachine::READBACK_STATE_WAITING_FOR_COMMIT,
            state.readback_state());
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  // Perform the draw & swap of replacement commit.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_NONE);

  // forced commit is started.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
}

TEST(SchedulerStateMachineTest, TestInitialActionsWhenContextLost) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);
  state.SetNeedsCommit();
  state.DidLoseOutputSurface();

  // When we are visible, we normally want to begin output surface creation
  // as soon as possible.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION);

  state.DidCreateAndInitializeOutputSurface();
  EXPECT_EQ(state.output_surface_state(),
            SchedulerStateMachine::OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT);

  // We should not send a BeginMainFrame when we are invisible, even if we've
  // lost the output surface and are trying to get the first commit, since the
  // main thread will just abort anyway.
  state.SetVisible(false);
  EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.NextAction())
      << *state.AsValue();

  // If there is a forced commit, however, we could be blocking a readback
  // on the main thread, so we need to unblock it before we can get our
  // output surface, even if we are not visible.
  state.SetNeedsForcedCommitForReadback();
  EXPECT_EQ(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME, state.NextAction())
      << *state.AsValue();
}

TEST(SchedulerStateMachineTest, TestImmediateFinishCommit) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // Schedule a readback, commit it, draw it.
  state.SetNeedsForcedCommitForReadback();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);

  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();

  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT,
            state.CommitState());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);

  EXPECT_TRUE(state.active_tree_needs_first_draw());

  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_READBACK);
  state.DidDrawIfPossibleCompleted(DrawSwapReadbackResult::DRAW_SUCCESS);

  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Should be waiting for the normal BeginMainFrame.
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
            state.CommitState());
}

TEST(SchedulerStateMachineTest, TestImmediateFinishCommitDuringCommit) {
  SchedulerSettings scheduler_settings;
  StateMachine state(scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // Start a normal commit.
  state.SetNeedsCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Schedule a readback, commit it, draw it.
  state.SetNeedsForcedCommitForReadback();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT,
            state.CommitState());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);

  EXPECT_TRUE(state.active_tree_needs_first_draw());

  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_READBACK);
  state.DidDrawIfPossibleCompleted(DrawSwapReadbackResult::DRAW_SUCCESS);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Should be waiting for the normal BeginMainFrame.
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
            state.CommitState())
      << *state.AsValue();
}

TEST(SchedulerStateMachineTest, ImmediateBeginMainFrameAbortedWhileInvisible) {
  SchedulerSettings scheduler_settings;
  StateMachine state(scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  state.SetNeedsCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.SetNeedsForcedCommitForReadback();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();

  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT,
            state.CommitState());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);

  EXPECT_TRUE(state.active_tree_needs_first_draw());

  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_READBACK);
  state.DidDrawIfPossibleCompleted(DrawSwapReadbackResult::DRAW_SUCCESS);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Should be waiting for BeginMainFrame.
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
            state.CommitState())
      << *state.AsValue();

  // Become invisible and abort BeginMainFrame.
  state.SetVisible(false);
  state.BeginMainFrameAborted(false);

  // Should be back in the idle state, but needing a commit.
  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_IDLE, state.CommitState());
  EXPECT_TRUE(state.NeedsCommit());
}

TEST(SchedulerStateMachineTest, ImmediateFinishCommitWhileCantDraw) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(false);

  state.SetNeedsCommit();
  state.UpdateState(state.NextAction());

  state.SetNeedsForcedCommitForReadback();
  state.UpdateState(state.NextAction());
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();

  EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT,
            state.CommitState());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);

  EXPECT_TRUE(state.active_tree_needs_first_draw());

  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_AND_READBACK);
  state.DidDrawIfPossibleCompleted(DrawSwapReadbackResult::DRAW_SUCCESS);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, ReportIfNotDrawing) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();

  state.SetCanDraw(true);
  state.SetVisible(true);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());

  state.SetCanDraw(false);
  state.SetVisible(true);
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetCanDraw(true);
  state.SetVisible(false);
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetCanDraw(false);
  state.SetVisible(false);
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetCanDraw(true);
  state.SetVisible(true);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());
}

TEST(SchedulerStateMachineTest, TestTriggerDeadlineEarlyAfterAbortedCommit) {
  SchedulerSettings settings;
  settings.impl_side_painting = true;
  StateMachine state(settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // This test mirrors what happens during the first frame of a scroll gesture.
  // First we get the input event and a BeginFrame.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());

  // As a response the compositor requests a redraw and a commit to tell the
  // main thread about the new scroll offset.
  state.SetNeedsRedraw(true);
  state.SetNeedsCommit();

  // We should start the commit normally.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Since only the scroll offset changed, the main thread will abort the
  // commit.
  state.BeginMainFrameAborted(true);

  // Since the commit was aborted, we should draw right away instead of waiting
  // for the deadline.
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineEarly());
}

TEST(SchedulerStateMachineTest, TestTriggerDeadlineEarlyForSmoothness) {
  SchedulerSettings settings;
  settings.impl_side_painting = true;
  StateMachine state(settings);
  state.SetCanStart();
  state.UpdateState(state.NextAction());
  state.CreateAndInitializeOutputSurfaceWithActivatedCommit();
  state.SetVisible(true);
  state.SetCanDraw(true);

  // This test ensures that impl-draws are prioritized over main thread updates
  // in prefer smoothness mode.
  state.OnBeginImplFrame(BeginFrameArgs::CreateForTesting());
  state.SetNeedsRedraw(true);
  state.SetNeedsCommit();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // The deadline is not triggered early until we enter prefer smoothness mode.
  EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineEarly());
  state.SetSmoothnessTakesPriority(true);
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineEarly());
}

}  // namespace
}  // namespace cc
