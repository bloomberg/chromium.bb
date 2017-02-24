// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler_state_machine.h"

#include <stddef.h>

#include "base/trace_event/trace_event.h"
#include "cc/output/begin_frame_args.h"
#include "cc/scheduler/scheduler.h"
#include "cc/test/begin_frame_args_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// Macro to compare two enum values and get nice output.
// Without:
//   Value of: actual()      Actual: 7
//   Expected: expected()  Which is: 0
// With:
//   Value of: actual()      Actual: "ACTION_DRAW"
//   Expected: expected()  Which is: "ACTION_NONE"
#define EXPECT_ENUM_EQ(enum_tostring, expected, actual)        \
  EXPECT_STREQ(SchedulerStateMachine::enum_tostring(expected), \
               SchedulerStateMachine::enum_tostring(actual))

#define EXPECT_SEQUENCE_NUMBERS(current, main_frame, pending_tree,            \
                                active_tree, compositor_frame)                \
  EXPECT_EQ(current, state.begin_frame_sequence_number());                    \
  EXPECT_EQ(main_frame,                                                       \
            state.last_begin_frame_sequence_number_begin_main_frame_sent());  \
  EXPECT_EQ(pending_tree,                                                     \
            state.last_begin_frame_sequence_number_pending_tree_was_fresh()); \
  EXPECT_EQ(active_tree,                                                      \
            state.last_begin_frame_sequence_number_active_tree_was_fresh());  \
  EXPECT_EQ(                                                                  \
      compositor_frame,                                                       \
      state.last_begin_frame_sequence_number_compositor_frame_was_fresh())

#define EXPECT_IMPL_FRAME_STATE(expected)               \
  EXPECT_ENUM_EQ(BeginImplFrameStateToString, expected, \
                 state.begin_impl_frame_state())        \
      << state.AsValue()->ToString()

#define EXPECT_MAIN_FRAME_STATE(expected)               \
  EXPECT_ENUM_EQ(BeginMainFrameStateToString, expected, \
                 state.BeginMainFrameState())

#define EXPECT_ACTION(expected)                                \
  EXPECT_ENUM_EQ(ActionToString, expected, state.NextAction()) \
      << state.AsValue()->ToString()

#define EXPECT_ACTION_UPDATE_STATE(action)                              \
  EXPECT_ACTION(action);                                                \
  if (action == SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE ||       \
      action == SchedulerStateMachine::ACTION_DRAW_FORCED) {            \
    EXPECT_IMPL_FRAME_STATE(                                            \
        SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE); \
  }                                                                     \
  PerformAction(&state, action);                                        \
  if (action == SchedulerStateMachine::ACTION_NONE) {                   \
    if (state.begin_impl_frame_state() ==                               \
        SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE)  \
      state.OnBeginImplFrameIdle();                                     \
  }

#define SET_UP_STATE(state)                                                \
  state.SetVisible(true);                                                  \
  EXPECT_ACTION_UPDATE_STATE(                                              \
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION); \
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);          \
  state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();       \
  state.SetCanDraw(true);

namespace cc {

namespace {

const SchedulerStateMachine::BeginImplFrameState all_begin_impl_frame_states[] =
    {
        SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE,
        SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME,
        SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE,
};

const SchedulerStateMachine::BeginMainFrameState begin_main_frame_states[] = {
    SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE,
    SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT,
    SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_STARTED,
    SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_READY_TO_COMMIT};

// Exposes the protected state fields of the SchedulerStateMachine for testing
class StateMachine : public SchedulerStateMachine {
 public:
  explicit StateMachine(const SchedulerSettings& scheduler_settings)
      : SchedulerStateMachine(scheduler_settings),
        draw_result_for_test_(DRAW_SUCCESS) {}

  void CreateAndInitializeCompositorFrameSinkWithActivatedCommit() {
    DidCreateAndInitializeCompositorFrameSink();
    compositor_frame_sink_state_ = COMPOSITOR_FRAME_SINK_ACTIVE;
  }

  void IssueNextBeginImplFrame() {
    OnBeginImplFrame(0, next_begin_frame_number_++);
  }

  void SetBeginMainFrameState(BeginMainFrameState cs) {
    begin_main_frame_state_ = cs;
  }
  BeginMainFrameState BeginMainFrameState() const {
    return begin_main_frame_state_;
  }

  ForcedRedrawOnTimeoutState ForcedRedrawState() const {
    return forced_redraw_state_;
  }

  void SetBeginImplFrameState(BeginImplFrameState bifs) {
    begin_impl_frame_state_ = bifs;
  }

  BeginImplFrameState begin_impl_frame_state() const {
    return begin_impl_frame_state_;
  }

  CompositorFrameSinkState compositor_frame_sink_state() const {
    return compositor_frame_sink_state_;
  }

  void SetNeedsBeginMainFrameForTest(bool needs_begin_main_frame) {
    needs_begin_main_frame_ = needs_begin_main_frame;
  }

  bool NeedsCommit() const { return needs_begin_main_frame_; }

  void SetNeedsOneBeginImplFrame(bool needs_frame) {
    needs_one_begin_impl_frame_ = needs_frame;
  }

  void SetNeedsRedraw(bool needs_redraw) { needs_redraw_ = needs_redraw; }

  void SetDrawResultForTest(DrawResult draw_result) {
    draw_result_for_test_ = draw_result;
  }
  DrawResult draw_result_for_test() { return draw_result_for_test_; }

  void SetNeedsForcedRedrawForTimeout(bool b) {
    forced_redraw_state_ = FORCED_REDRAW_STATE_WAITING_FOR_COMMIT;
    active_tree_needs_first_draw_ = true;
  }
  bool NeedsForcedRedrawForTimeout() const {
    return forced_redraw_state_ != FORCED_REDRAW_STATE_IDLE;
  }

  void SetActiveTreeNeedsFirstDraw(bool needs_first_draw) {
    active_tree_needs_first_draw_ = needs_first_draw;
  }

  bool CanDraw() const { return can_draw_; }
  bool Visible() const { return visible_; }

  bool PendingActivationsShouldBeForced() const {
    return SchedulerStateMachine::PendingActivationsShouldBeForced();
  }

  bool has_pending_tree() const { return has_pending_tree_; }
  void SetHasPendingTree(bool has_pending_tree) {
    has_pending_tree_ = has_pending_tree;
  }

  bool needs_impl_side_invalidation() const {
    return needs_impl_side_invalidation_;
  }

  uint64_t begin_frame_sequence_number() const {
    return begin_frame_sequence_number_;
  }
  uint64_t last_begin_frame_sequence_number_begin_main_frame_sent() const {
    return last_begin_frame_sequence_number_begin_main_frame_sent_;
  }
  uint64_t last_begin_frame_sequence_number_pending_tree_was_fresh() const {
    return last_begin_frame_sequence_number_pending_tree_was_fresh_;
  }
  uint64_t last_begin_frame_sequence_number_active_tree_was_fresh() const {
    return last_begin_frame_sequence_number_active_tree_was_fresh_;
  }

  using SchedulerStateMachine::ShouldTriggerBeginImplFrameDeadlineImmediately;
  using SchedulerStateMachine::ProactiveBeginFrameWanted;
  using SchedulerStateMachine::WillCommit;

 protected:
  DrawResult draw_result_for_test_;
  uint64_t next_begin_frame_number_ = BeginFrameArgs::kStartingFrameNumber;
};

void PerformAction(StateMachine* sm, SchedulerStateMachine::Action action) {
  switch (action) {
    case SchedulerStateMachine::ACTION_NONE:
      return;

    case SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE:
      sm->WillActivate();
      return;

    case SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME:
      sm->WillSendBeginMainFrame();
      return;

    case SchedulerStateMachine::ACTION_COMMIT: {
      bool commit_has_no_updates = false;
      sm->WillCommit(commit_has_no_updates);
      return;
    }

    case SchedulerStateMachine::ACTION_DRAW_FORCED:
    case SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE: {
      sm->WillDraw();
      sm->DidDraw(sm->draw_result_for_test());
      return;
    }

    case SchedulerStateMachine::ACTION_DRAW_ABORT: {
      sm->AbortDraw();
      return;
    }

    case SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION:
      sm->WillBeginCompositorFrameSinkCreation();
      return;

    case SchedulerStateMachine::ACTION_PREPARE_TILES:
      sm->WillPrepareTiles();
      return;

    case SchedulerStateMachine::ACTION_INVALIDATE_COMPOSITOR_FRAME_SINK:
      sm->WillInvalidateCompositorFrameSink();
      return;

    case SchedulerStateMachine::ACTION_PERFORM_IMPL_SIDE_INVALIDATION:
      sm->WillPerformImplSideInvalidation();
      return;
  }
}

TEST(SchedulerStateMachineTest, BeginFrameNeeded) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
  state.SetBeginMainFrameState(
      SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);

  // Don't request BeginFrames if we are idle.
  state.SetNeedsRedraw(false);
  state.SetNeedsOneBeginImplFrame(false);
  EXPECT_FALSE(state.BeginFrameNeeded());

  // Request BeginFrames if we one is needed.
  state.SetNeedsRedraw(false);
  state.SetNeedsOneBeginImplFrame(true);
  EXPECT_TRUE(state.BeginFrameNeeded());

  // Request BeginFrames if we are ready to draw.
  state.SetVisible(true);
  state.SetNeedsRedraw(true);
  state.SetNeedsOneBeginImplFrame(false);
  EXPECT_TRUE(state.BeginFrameNeeded());

  // Don't background tick for needs_redraw.
  state.SetVisible(false);
  state.SetNeedsRedraw(true);
  state.SetNeedsOneBeginImplFrame(false);
  EXPECT_FALSE(state.BeginFrameNeeded());

  // Proactively request BeginFrames when commit is pending.
  state.SetVisible(true);
  state.SetNeedsRedraw(false);
  state.SetNeedsOneBeginImplFrame(false);
  state.SetNeedsBeginMainFrameForTest(true);
  EXPECT_TRUE(state.BeginFrameNeeded());

  // Don't request BeginFrames when commit is pending if
  // we are currently deferring commits.
  state.SetVisible(true);
  state.SetNeedsRedraw(false);
  state.SetNeedsOneBeginImplFrame(false);
  state.SetNeedsBeginMainFrameForTest(true);
  state.SetDeferCommits(true);
  EXPECT_FALSE(state.BeginFrameNeeded());
}

TEST(SchedulerStateMachineTest, TestNextActionBeginsMainFrameIfNeeded) {
  SchedulerSettings default_scheduler_settings;

  // If no commit needed, do nothing.
  {
    StateMachine state(default_scheduler_settings);
    state.SetVisible(true);
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
    state.SetBeginMainFrameState(
        SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);
    state.SetNeedsRedraw(false);

    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    EXPECT_FALSE(state.NeedsCommit());

    state.IssueNextBeginImplFrame();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

    state.OnBeginImplFrameDeadline();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    EXPECT_FALSE(state.NeedsCommit());
  }

  // If commit requested but not visible yet, do nothing.
  {
    StateMachine state(default_scheduler_settings);
    state.SetBeginMainFrameState(
        SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);
    state.SetNeedsRedraw(false);
    state.SetNeedsBeginMainFrame();

    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    EXPECT_TRUE(state.NeedsCommit());

    state.IssueNextBeginImplFrame();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

    state.OnBeginImplFrameDeadline();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    EXPECT_TRUE(state.NeedsCommit());
  }

  // If commit requested, begin a main frame.
  {
    StateMachine state(default_scheduler_settings);
    state.SetBeginMainFrameState(
        SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);
    state.SetVisible(true);
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
    state.SetNeedsRedraw(false);
    state.SetNeedsBeginMainFrame();

    // Expect nothing to happen until after OnBeginImplFrame.
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);
    EXPECT_IMPL_FRAME_STATE(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);
    EXPECT_TRUE(state.NeedsCommit());
    EXPECT_TRUE(state.BeginFrameNeeded());

    state.IssueNextBeginImplFrame();
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
    EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT);
    EXPECT_FALSE(state.NeedsCommit());
  }

  // If commit requested and can't draw, still begin a main frame.
  {
    StateMachine state(default_scheduler_settings);
    state.SetBeginMainFrameState(
        SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);
    state.SetVisible(true);
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
    state.SetNeedsRedraw(false);
    state.SetNeedsBeginMainFrame();
    state.SetCanDraw(false);

    // Expect nothing to happen until after OnBeginImplFrame.
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);
    EXPECT_IMPL_FRAME_STATE(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);
    EXPECT_TRUE(state.BeginFrameNeeded());

    state.IssueNextBeginImplFrame();
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
    EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT);
    EXPECT_FALSE(state.NeedsCommit());
  }
}

// Explicitly test main_frame_before_activation_enabled = true
TEST(SchedulerStateMachineTest, MainFrameBeforeActivationEnabled) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.main_frame_before_activation_enabled = true;
  StateMachine state(scheduler_settings);
  state.SetBeginMainFrameState(
      SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);
  SET_UP_STATE(state)
  state.SetNeedsRedraw(false);
  state.SetNeedsBeginMainFrame();

  EXPECT_TRUE(state.BeginFrameNeeded());

  // Commit to the pending tree.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Verify that the next commit starts while there is still a pending tree.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Verify the pending commit doesn't overwrite the pending
  // tree until the pending tree has been activated.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Verify NotifyReadyToActivate unblocks activation, commit, and
  // draw in that order.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);
}

TEST(SchedulerStateMachineTest,
     FailedDrawForAnimationCheckerboardSetsNeedsCommitAndRetriesDraw) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)
  state.SetNeedsRedraw(true);
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_TRUE(state.BeginFrameNeeded());

  // Start a frame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.CommitPending());

  // Failing a draw triggers request for a new BeginMainFrame.
  state.OnBeginImplFrameDeadline();
  state.SetDrawResultForTest(DRAW_ABORTED_CHECKERBOARD_ANIMATIONS);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // It's okay to attempt more draws just in case additional raster
  // finishes and the requested commit wasn't actually necessary.
  EXPECT_TRUE(state.CommitPending());
  EXPECT_TRUE(state.RedrawPending());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  state.SetDrawResultForTest(DRAW_ABORTED_CHECKERBOARD_ANIMATIONS);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, FailedDrawForMissingHighResNeedsCommit) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)
  state.SetNeedsRedraw(true);
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_TRUE(state.BeginFrameNeeded());

  // Start a frame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.CommitPending());

  // Failing a draw triggers because of high res tiles missing
  // request for a new BeginMainFrame.
  state.OnBeginImplFrameDeadline();
  state.SetDrawResultForTest(DRAW_ABORTED_MISSING_HIGH_RES_CONTENT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // It doesn't request a draw until we get a new commit though.
  EXPECT_TRUE(state.CommitPending());
  EXPECT_FALSE(state.RedrawPending());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Finish the commit and activation.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.RedrawPending());

  // Verify we draw with the new frame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  state.SetDrawResultForTest(DRAW_SUCCESS);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest,
     TestFailedDrawsEventuallyForceDrawAfterNextCommit) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.maximum_number_of_failed_draws_before_draw_is_forced = 1;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state)

  // Start a commit.
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.CommitPending());

  // Then initiate a draw that fails.
  state.SetNeedsRedraw(true);
  state.OnBeginImplFrameDeadline();
  state.SetDrawResultForTest(DRAW_ABORTED_CHECKERBOARD_ANIMATIONS);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.BeginFrameNeeded());
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_TRUE(state.CommitPending());

  // Finish the commit. Note, we should not yet be forcing a draw, but should
  // continue the commit as usual.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.RedrawPending());

  // Activate so we're ready for a new main frame.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.RedrawPending());

  // The redraw should be forced at the end of the next BeginImplFrame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  state.SetDrawResultForTest(DRAW_SUCCESS);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_FORCED);
  state.DidSubmitCompositorFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestFailedDrawsDoNotRestartForcedDraw) {
  SchedulerSettings scheduler_settings;
  int draw_limit = 2;
  scheduler_settings.maximum_number_of_failed_draws_before_draw_is_forced =
      draw_limit;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state)

  // Start a commit.
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.CommitPending());

  // Then initiate a draw.
  state.SetNeedsRedraw(true);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);

  // Fail the draw enough times to force a redraw.
  for (int i = 0; i < draw_limit; ++i) {
    state.SetNeedsRedraw(true);
    state.IssueNextBeginImplFrame();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    state.OnBeginImplFrameDeadline();
    state.SetDrawResultForTest(DRAW_ABORTED_CHECKERBOARD_ANIMATIONS);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    state.OnBeginImplFrameIdle();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  }

  EXPECT_TRUE(state.BeginFrameNeeded());
  EXPECT_TRUE(state.RedrawPending());
  // But the commit is ongoing.
  EXPECT_TRUE(state.CommitPending());
  EXPECT_TRUE(state.ForcedRedrawState() ==
              SchedulerStateMachine::FORCED_REDRAW_STATE_WAITING_FOR_COMMIT);

  // After failing additional draws, we should still be in a forced
  // redraw, but not back in IDLE.
  for (int i = 0; i < draw_limit; ++i) {
    state.SetNeedsRedraw(true);
    state.IssueNextBeginImplFrame();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    state.OnBeginImplFrameDeadline();
    state.SetDrawResultForTest(DRAW_ABORTED_CHECKERBOARD_ANIMATIONS);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    state.OnBeginImplFrameIdle();
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  }
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_TRUE(state.ForcedRedrawState() ==
              SchedulerStateMachine::FORCED_REDRAW_STATE_WAITING_FOR_COMMIT);
}

TEST(SchedulerStateMachineTest, TestFailedDrawIsRetriedInNextBeginImplFrame) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  // Start a draw.
  state.SetNeedsRedraw(true);
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_TRUE(state.RedrawPending());
  state.SetDrawResultForTest(DRAW_ABORTED_CHECKERBOARD_ANIMATIONS);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);

  // Failing the draw for animation checkerboards makes us require a commit.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.RedrawPending());

  // We should not be trying to draw again now, but we have a commit pending.
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // We should try to draw again at the end of the next BeginImplFrame on
  // the impl thread.
  state.OnBeginImplFrameDeadline();
  state.SetDrawResultForTest(DRAW_SUCCESS);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestDoestDrawTwiceInSameFrame) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)
  state.SetNeedsRedraw(true);

  // Draw the first frame.
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Before the next BeginImplFrame, set needs redraw again.
  // This should not redraw until the next BeginImplFrame.
  state.SetNeedsRedraw(true);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Move to another frame. This should now draw.
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // We just submitted, so we should proactively request another BeginImplFrame.
  EXPECT_TRUE(state.BeginFrameNeeded());
}

TEST(SchedulerStateMachineTest, TestNextActionDrawsOnBeginImplFrame) {
  SchedulerSettings default_scheduler_settings;

  // When not in BeginImplFrame deadline, or in BeginImplFrame deadline
  // but not visible, don't draw.
  size_t num_begin_main_frame_states =
      sizeof(begin_main_frame_states) /
      sizeof(SchedulerStateMachine::BeginMainFrameState);
  size_t num_begin_impl_frame_states =
      sizeof(all_begin_impl_frame_states) /
      sizeof(SchedulerStateMachine::BeginImplFrameState);
  for (size_t i = 0; i < num_begin_main_frame_states; ++i) {
    for (size_t j = 0; j < num_begin_impl_frame_states; ++j) {
      StateMachine state(default_scheduler_settings);
      state.SetVisible(true);
      EXPECT_ACTION_UPDATE_STATE(
          SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
      EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
      state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
      state.SetBeginMainFrameState(begin_main_frame_states[i]);
      state.SetBeginImplFrameState(all_begin_impl_frame_states[j]);
      bool visible =
          (all_begin_impl_frame_states[j] !=
           SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE);
      state.SetVisible(visible);

      // Case 1: needs_begin_main_frame=false
      EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE,
                state.NextAction());

      // Case 2: needs_begin_main_frame=true
      state.SetNeedsBeginMainFrame();
      EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE,
                state.NextAction())
          << state.AsValue()->ToString();
    }
  }

  // When in BeginImplFrame deadline we should always draw for SetNeedsRedraw
  // except if we're ready to commit, in which case we expect a commit first.
  for (size_t i = 0; i < num_begin_main_frame_states; ++i) {
    StateMachine state(default_scheduler_settings);
    state.SetVisible(true);
    EXPECT_ACTION_UPDATE_STATE(
        SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
    EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
    state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
    state.SetCanDraw(true);
    state.SetBeginMainFrameState(begin_main_frame_states[i]);
    state.SetBeginImplFrameState(
        SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE);

    state.SetNeedsRedraw(true);

    SchedulerStateMachine::Action expected_action;
    if (begin_main_frame_states[i] ==
        SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_READY_TO_COMMIT) {
      expected_action = SchedulerStateMachine::ACTION_COMMIT;
    } else {
      expected_action = SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE;
    }

    // Case 1: needs_begin_main_frame=false.
    EXPECT_ACTION(expected_action);

    // Case 2: needs_begin_main_frame=true.
    state.SetNeedsBeginMainFrame();
    EXPECT_ACTION(expected_action);
  }
}

TEST(SchedulerStateMachineTest, TestNoBeginMainFrameStatesRedrawWhenInvisible) {
  SchedulerSettings default_scheduler_settings;

  size_t num_begin_main_frame_states =
      sizeof(begin_main_frame_states) /
      sizeof(SchedulerStateMachine::BeginMainFrameState);
  for (size_t i = 0; i < num_begin_main_frame_states; ++i) {
    // There shouldn't be any drawing regardless of BeginImplFrame.
    for (size_t j = 0; j < 2; ++j) {
      StateMachine state(default_scheduler_settings);
      state.SetVisible(true);
      EXPECT_ACTION_UPDATE_STATE(
          SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
      EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
      state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
      state.SetBeginMainFrameState(begin_main_frame_states[i]);
      state.SetVisible(false);
      state.SetNeedsRedraw(true);
      if (j == 1) {
        state.SetBeginImplFrameState(
            SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE);
      }

      // Case 1: needs_begin_main_frame=false.
      EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE,
                state.NextAction());

      // Case 2: needs_begin_main_frame=true.
      state.SetNeedsBeginMainFrame();
      EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE,
                state.NextAction())
          << state.AsValue()->ToString();
    }
  }
}

TEST(SchedulerStateMachineTest, TestCanRedraw_StopsDraw) {
  SchedulerSettings default_scheduler_settings;

  size_t num_begin_main_frame_states =
      sizeof(begin_main_frame_states) /
      sizeof(SchedulerStateMachine::BeginMainFrameState);
  for (size_t i = 0; i < num_begin_main_frame_states; ++i) {
    // There shouldn't be any drawing regardless of BeginImplFrame.
    for (size_t j = 0; j < 2; ++j) {
      StateMachine state(default_scheduler_settings);
      state.SetVisible(true);
      EXPECT_ACTION_UPDATE_STATE(
          SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
      EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
      state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
      state.SetBeginMainFrameState(begin_main_frame_states[i]);
      state.SetVisible(false);
      state.SetNeedsRedraw(true);
      if (j == 1)
        state.IssueNextBeginImplFrame();

      state.SetCanDraw(false);
      EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE,
                state.NextAction());
    }
  }
}

TEST(SchedulerStateMachineTest,
     TestCanRedrawWithWaitingForFirstDrawMakesProgress) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();

  state.SetActiveTreeNeedsFirstDraw(true);
  state.SetNeedsBeginMainFrame();
  state.SetNeedsRedraw(true);
  state.SetCanDraw(false);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_ABORT);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_ABORT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestSetNeedsBeginMainFrameIsNotLost) {
  SchedulerSettings scheduler_settings;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state)
  state.SetNeedsBeginMainFrame();

  EXPECT_TRUE(state.BeginFrameNeeded());

  // Begin the frame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT);

  // Now, while the frame is in progress, set another commit.
  state.SetNeedsBeginMainFrame();
  EXPECT_TRUE(state.NeedsCommit());

  // Let the frame finish.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_READY_TO_COMMIT);

  // Expect to commit regardless of BeginImplFrame state.
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME);
  EXPECT_ACTION(SchedulerStateMachine::ACTION_COMMIT);

  state.OnBeginImplFrameDeadline();
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE);
  EXPECT_ACTION(SchedulerStateMachine::ACTION_COMMIT);

  state.OnBeginImplFrameIdle();
  EXPECT_IMPL_FRAME_STATE(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);
  EXPECT_ACTION(SchedulerStateMachine::ACTION_COMMIT);

  state.IssueNextBeginImplFrame();
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME);
  EXPECT_ACTION(SchedulerStateMachine::ACTION_COMMIT);

  // Finish the commit and activate, then make sure we start the next commit
  // immediately and draw on the next BeginImplFrame.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.OnBeginImplFrameDeadline();

  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestFullCycle) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  // Start clean and set commit.
  state.SetNeedsBeginMainFrame();

  // Begin the frame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT);
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Tell the scheduler the frame finished.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_READY_TO_COMMIT);

  // Commit.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);

  // Activate.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_TRUE(state.needs_redraw());

  // Expect to do nothing until BeginImplFrame deadline
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // At BeginImplFrame deadline, draw.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();

  // Should be synchronized, no draw needed, no action needed.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);
  EXPECT_FALSE(state.needs_redraw());
}

TEST(SchedulerStateMachineTest, CommitWithoutDrawWithPendingTree) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  // Start clean and set commit.
  state.SetNeedsBeginMainFrame();

  // Make a main frame, commit and activate it. But don't draw it.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);

  // Try to make a new main frame before drawing. Since we will commit it to a
  // pending tree and not clobber the active tree, we're able to start a new
  // begin frame and commit it.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
}

TEST(SchedulerStateMachineTest, DontCommitWithoutDrawWithoutPendingTree) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.commit_to_active_tree = true;
  scheduler_settings.main_frame_before_activation_enabled = false;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state)

  // Start clean and set commit.
  state.SetNeedsBeginMainFrame();

  // Make a main frame, commit and activate it. But don't draw it.
  state.OnBeginImplFrame(0, 10);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, 10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, 10u, 10u,
                          BeginFrameArgs::kInvalidFrameNumber);

  // Try to make a new main frame before drawing, but since we would clobber the
  // active tree, we will not do so.
  state.SetNeedsBeginMainFrame();
  state.OnBeginImplFrame(0, 11);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(11u, 10u, 10u, 10u,
                          BeginFrameArgs::kInvalidFrameNumber);
}

TEST(SchedulerStateMachineTest, AbortedMainFrameDoesNotResetPendingTree) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.main_frame_before_activation_enabled = true;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state);

  // Perform a commit so that we have an active tree.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.has_pending_tree());
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Ask for another commit but abort it. Verify that we didn't reset pending
  // tree state.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.has_pending_tree());
  state.NotifyBeginMainFrameStarted();
  state.BeginMainFrameAborted(CommitEarlyOutReason::FINISHED_NO_UPDATES);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.has_pending_tree());
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Ask for another commit that doesn't abort.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.has_pending_tree());

  // Verify that commit is delayed until the pending tree is activated.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_FALSE(state.has_pending_tree());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.has_pending_tree());
}

TEST(SchedulerStateMachineTest, TestFullCycleWithCommitToActive) {
  SchedulerSettings scheduler_settings;
  scheduler_settings.commit_to_active_tree = true;
  scheduler_settings.main_frame_before_activation_enabled = false;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state)

  // Start clean and set commit.
  state.SetNeedsBeginMainFrame();

  // Begin the frame.
  state.OnBeginImplFrame(0, 10);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT);
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);

  // Tell the scheduler the frame finished.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_READY_TO_COMMIT);
  // Commit.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, 10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);
  // Commit always calls NotifyReadyToActivate in this mode.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, 10u, 10u,
                          BeginFrameArgs::kInvalidFrameNumber);

  // No draw because we haven't received NotifyReadyToDraw yet.
  state.OnBeginImplFrameDeadline();
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_TRUE(state.needs_redraw());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, 10u, 10u,
                          BeginFrameArgs::kInvalidFrameNumber);

  // Can't BeginMainFrame yet since last commit hasn't been drawn yet.
  state.SetNeedsBeginMainFrame();
  state.OnBeginImplFrame(0, 11);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(11u, 10u, 10u, 10u,
                          BeginFrameArgs::kInvalidFrameNumber);

  // Now call ready to draw which will allow the draw to happen and
  // BeginMainFrame to be sent.
  state.NotifyReadyToDraw();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  EXPECT_SEQUENCE_NUMBERS(11u, 10u, 10u, 10u, 10u);
  // Submit throttled from this point.
  state.DidSubmitCompositorFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Can't BeginMainFrame yet since we're submit-frame throttled.
  state.OnBeginImplFrame(0, 12);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(12u, 10u, 10u, 10u, 10u);

  // CompositorFrameAck unblocks BeginMainFrame.
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_SEQUENCE_NUMBERS(12u, 12u, 10u, 10u, 10u);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_SEQUENCE_NUMBERS(12u, 12u, 12u, 10u, 10u);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(12u, 12u, 12u, 12u, 10u);

  // Draw the newly activated tree.
  state.NotifyReadyToDraw();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(12u, 12u, 12u, 12u, 12u);
}

TEST(SchedulerStateMachineTest, TestFullCycleWithCommitRequestInbetween) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  // Start clean and set commit.
  state.SetNeedsBeginMainFrame();

  // Begin the frame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT);
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Request another commit while the commit is in flight.
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Tell the scheduler the frame finished.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_MAIN_FRAME_STATE(
      SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_READY_TO_COMMIT);

  // First commit and activate.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_TRUE(state.needs_redraw());

  // Expect to do nothing until BeginImplFrame deadline.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // At BeginImplFrame deadline, draw.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();

  // Should be synchronized, no draw needed, no action needed.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);
  EXPECT_FALSE(state.needs_redraw());

  // Next BeginImplFrame should initiate second commit.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest, TestNoRequestCommitWhenInvisible) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
  state.SetVisible(false);
  state.SetNeedsBeginMainFrame();
  EXPECT_FALSE(state.CouldSendBeginMainFrame());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestNoRequestCommitWhenBeginFrameSourcePaused) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
  state.SetBeginFrameSourcePaused(true);
  state.SetNeedsBeginMainFrame();
  EXPECT_FALSE(state.CouldSendBeginMainFrame());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestNoRequestCompositorFrameSinkWhenInvisible) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  // We should not request an CompositorFrameSink when we are still invisible.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
  state.SetVisible(false);
  state.DidLoseCompositorFrameSink();
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
}

// See ProxyMain::BeginMainFrame "EarlyOut_NotVisible" /
// "EarlyOut_CompositorFrameSinkLost" cases.
TEST(SchedulerStateMachineTest, TestAbortBeginMainFrameBecauseInvisible) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  // Start clean and set commit.
  state.SetNeedsBeginMainFrame();

  // Begin the frame while visible.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT);
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Become invisible and abort BeginMainFrame.
  state.SetVisible(false);
  state.NotifyBeginMainFrameStarted();
  state.BeginMainFrameAborted(CommitEarlyOutReason::ABORTED_NOT_VISIBLE);

  // NeedsCommit should now be true again because we never actually did a
  // commit.
  EXPECT_TRUE(state.NeedsCommit());

  // We should now be back in the idle state as if we never started the frame.
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // We shouldn't do anything on the BeginImplFrame deadline.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Become visible again.
  state.SetVisible(true);

  // Although we have aborted on this frame and haven't cancelled the commit
  // (i.e. need another), don't send another BeginMainFrame yet.
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);
  EXPECT_ACTION(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.NeedsCommit());

  // Start a new frame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);

  // We should be starting the commit now.
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

// See ProxyMain::BeginMainFrame "EarlyOut_NoUpdates" case.
TEST(SchedulerStateMachineTest, TestAbortBeginMainFrameBecauseCommitNotNeeded) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.DidCreateAndInitializeCompositorFrameSink();
  state.SetCanDraw(true);

  // Get into a begin frame / commit state.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT);
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_ACTION(SchedulerStateMachine::ACTION_NONE);

  // Abort the commit, true means that the BeginMainFrame was sent but there
  // was no work to do on the main thread.
  state.NotifyBeginMainFrameStarted();
  state.BeginMainFrameAborted(CommitEarlyOutReason::FINISHED_NO_UPDATES);

  // NeedsCommit should now be false because the commit was actually handled.
  EXPECT_FALSE(state.NeedsCommit());

  // Since the commit was aborted, we don't need to try and draw.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Verify another commit doesn't start on another frame either.
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);

  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Verify another commit can start if requested, though.
  state.SetNeedsBeginMainFrame();
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_IDLE);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION(SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest, TestFirstContextCreation) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  state.SetCanDraw(true);

  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Check that the first init does not SetNeedsBeginMainFrame.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Check that a needs commit initiates a BeginMainFrame.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest, TestContextLostWhenCompletelyIdle) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  EXPECT_NE(SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION,
            state.NextAction());
  state.DidLoseCompositorFrameSink();

  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Once context recreation begins, nothing should happen.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Recreate the context.
  state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();

  // When the context is recreated, we should begin a commit.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest,
     TestContextLostWhenIdleAndCommitRequestedWhileRecreating) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  EXPECT_NE(SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION,
            state.NextAction());
  state.DidLoseCompositorFrameSink();
  EXPECT_EQ(state.compositor_frame_sink_state(),
            SchedulerStateMachine::COMPOSITOR_FRAME_SINK_NONE);

  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Once context recreation begins, nothing should happen.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // While context is recreating, commits shouldn't begin.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Recreate the context
  state.DidCreateAndInitializeCompositorFrameSink();
  EXPECT_EQ(
      state.compositor_frame_sink_state(),
      SchedulerStateMachine::COMPOSITOR_FRAME_SINK_WAITING_FOR_FIRST_COMMIT);
  EXPECT_FALSE(state.RedrawPending());

  // When the context is recreated, we wait until the next BeginImplFrame
  // before starting.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // When the BeginFrame comes in we should begin a commit
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_MAIN_FRAME_STATE(SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT);

  // Until that commit finishes, we shouldn't be drawing.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Finish the commit, which should make the surface active.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_EQ(state.compositor_frame_sink_state(),
            SchedulerStateMachine::
                COMPOSITOR_FRAME_SINK_WAITING_FOR_FIRST_ACTIVATION);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_EQ(state.compositor_frame_sink_state(),
            SchedulerStateMachine::COMPOSITOR_FRAME_SINK_ACTIVE);

  // Finishing the first commit after initializing an CompositorFrameSink should
  // automatically cause a redraw.
  EXPECT_TRUE(state.RedrawPending());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.RedrawPending());

  // Next frame as no work to do.
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Once the context is recreated, whether we draw should be based on
  // SetCanDraw if waiting on first draw after activate.
  state.SetNeedsRedraw(true);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.SetCanDraw(false);
  EXPECT_ACTION(SchedulerStateMachine::ACTION_NONE);
  state.SetCanDraw(true);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Once the context is recreated, whether we draw should be based on
  // SetCanDraw if waiting on first draw after activate.
  state.SetNeedsRedraw(true);
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  // Activate so we need the first draw
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_TRUE(state.needs_redraw());

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.SetCanDraw(false);
  EXPECT_ACTION(SchedulerStateMachine::ACTION_DRAW_ABORT);
  state.SetCanDraw(true);
  EXPECT_ACTION(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
}

TEST(SchedulerStateMachineTest, TestContextLostWhileCommitInProgress) {
  SchedulerSettings scheduler_settings;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state)

  // Get a commit in flight.
  state.SetNeedsBeginMainFrame();

  // Set damage and expect a draw.
  state.SetNeedsRedraw(true);
  state.OnBeginImplFrame(0, 10);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);

  // Cause a lost context while the BeginMainFrame is in flight.
  state.DidLoseCompositorFrameSink();
  EXPECT_FALSE(state.BeginFrameNeeded());

  // Ask for another draw. Expect nothing happens.
  state.SetNeedsRedraw(true);
  EXPECT_ACTION(SchedulerStateMachine::ACTION_NONE);

  // Finish the frame, commit and activate.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, 10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, 10u, 10u,
                          BeginFrameArgs::kInvalidFrameNumber);

  // We will abort the draw when the CompositorFrameSink is lost if we are
  // waiting for the first draw to unblock the main thread.
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_ABORT);
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, 10u, 10u,
                          BeginFrameArgs::kInvalidFrameNumber);

  // Expect to begin context recreation only in BEGIN_IMPL_FRAME_STATE_IDLE
  EXPECT_IMPL_FRAME_STATE(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);
  EXPECT_ACTION(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);

  state.OnBeginImplFrame(0, 11);
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME);
  EXPECT_ACTION(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(11u, 10u, 10u, 10u,
                          BeginFrameArgs::kInvalidFrameNumber);

  state.OnBeginImplFrameDeadline();
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE);
  EXPECT_ACTION(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(11u, 10u, 10u, 11u, 11u);
}

TEST(SchedulerStateMachineTest,
     TestContextLostWhileCommitInProgressAndAnotherCommitRequested) {
  SchedulerSettings scheduler_settings;
  StateMachine state(scheduler_settings);
  SET_UP_STATE(state)

  // Get a commit in flight.
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Set damage and expect a draw.
  state.SetNeedsRedraw(true);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Cause a lost context while the BeginMainFrame is in flight.
  state.DidLoseCompositorFrameSink();

  // Ask for another draw and also set needs commit. Expect nothing happens.
  state.SetNeedsRedraw(true);
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Finish the frame, and commit and activate.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_TRUE(state.active_tree_needs_first_draw());

  // Because the CompositorFrameSink is missing, we expect the draw to abort.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_ABORT);

  // Expect to begin context recreation only in BEGIN_IMPL_FRAME_STATE_IDLE
  EXPECT_IMPL_FRAME_STATE(SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);
  EXPECT_ACTION(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);

  state.IssueNextBeginImplFrame();
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME);
  EXPECT_ACTION(SchedulerStateMachine::ACTION_NONE);

  state.OnBeginImplFrameDeadline();
  EXPECT_IMPL_FRAME_STATE(
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE);
  EXPECT_ACTION(SchedulerStateMachine::ACTION_NONE);

  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);

  // After we get a new CompositorFrameSink, the commit flow should start.
  state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest,
     DontDrawBeforeCommitAfterLostCompositorFrameSink) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  state.SetNeedsRedraw(true);

  // Cause a lost CompositorFrameSink, and restore it.
  state.DidLoseCompositorFrameSink();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.DidCreateAndInitializeCompositorFrameSink();

  EXPECT_FALSE(state.RedrawPending());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION(SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest,
     TestPendingActivationsShouldBeForcedAfterLostCompositorFrameSink) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  state.SetBeginMainFrameState(
      SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT);

  // Cause a lost context.
  state.DidLoseCompositorFrameSink();

  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);

  EXPECT_TRUE(state.PendingActivationsShouldBeForced());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);

  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_ABORT);
}

TEST(SchedulerStateMachineTest, TestNoBeginFrameNeededWhenInvisible) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();

  EXPECT_FALSE(state.BeginFrameNeeded());
  state.SetNeedsRedraw(true);
  EXPECT_TRUE(state.BeginFrameNeeded());

  state.SetVisible(false);
  EXPECT_FALSE(state.BeginFrameNeeded());

  state.SetVisible(true);
  EXPECT_TRUE(state.BeginFrameNeeded());
}

TEST(SchedulerStateMachineTest, TestNoBeginMainFrameWhenInvisible) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
  state.SetVisible(false);
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.BeginFrameNeeded());

  // When become visible again, the needs commit should still be pending.
  state.SetVisible(true);
  EXPECT_TRUE(state.BeginFrameNeeded());
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest, TestFinishCommitWhenCommitInProgress) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
  state.SetVisible(false);
  state.SetBeginMainFrameState(
      SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT);
  state.SetNeedsBeginMainFrame();

  // After the commit completes, activation and draw happen immediately
  // because we are not visible.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_ABORT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest,
     TestFinishCommitWhenCommitInProgressAndBeginFrameSourcePaused) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  state.SetVisible(true);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.CreateAndInitializeCompositorFrameSinkWithActivatedCommit();
  state.SetBeginFrameSourcePaused(true);
  state.SetBeginMainFrameState(
      SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT);
  state.SetNeedsBeginMainFrame();

  // After the commit completes, activation and draw happen immediately
  // because we are not visible.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_TRUE(state.PendingActivationsShouldBeForced());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_TRUE(state.active_tree_needs_first_draw());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_ABORT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestInitialActionsWhenContextLost) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)
  state.SetNeedsBeginMainFrame();
  state.DidLoseCompositorFrameSink();

  // When we are visible, we normally want to begin CompositorFrameSink creation
  // as soon as possible.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);

  state.DidCreateAndInitializeCompositorFrameSink();
  EXPECT_EQ(
      state.compositor_frame_sink_state(),
      SchedulerStateMachine::COMPOSITOR_FRAME_SINK_WAITING_FOR_FIRST_COMMIT);

  // We should not send a BeginMainFrame when we are invisible, even if we've
  // lost the CompositorFrameSink and are trying to get the first commit, since
  // the
  // main thread will just abort anyway.
  state.SetVisible(false);
  EXPECT_ACTION(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, ReportIfNotDrawing) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)
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
  state.SetBeginFrameSourcePaused(true);
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetBeginFrameSourcePaused(false);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());
}

TEST(SchedulerStateMachineTest, ForceDrawForResourcelessSoftwareDraw) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)
  state.SetResourcelessSoftwareDraw(true);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());

  state.SetVisible(false);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());

  state.SetResourcelessSoftwareDraw(false);
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetResourcelessSoftwareDraw(true);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());
  state.SetVisible(true);

  state.SetBeginFrameSourcePaused(true);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());

  state.SetResourcelessSoftwareDraw(false);
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetResourcelessSoftwareDraw(true);
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());
  state.SetBeginFrameSourcePaused(false);

  state.SetVisible(false);
  state.DidLoseCompositorFrameSink();

  state.SetCanDraw(false);
  state.WillBeginCompositorFrameSinkCreation();
  state.DidCreateAndInitializeCompositorFrameSink();
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetCanDraw(true);
  state.DidLoseCompositorFrameSink();
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());

  state.SetCanDraw(true);
  state.WillBeginCompositorFrameSinkCreation();
  state.DidCreateAndInitializeCompositorFrameSink();
  EXPECT_FALSE(state.PendingDrawsShouldBeAborted());

  state.SetCanDraw(false);
  state.DidLoseCompositorFrameSink();
  EXPECT_TRUE(state.PendingDrawsShouldBeAborted());
}

TEST(SchedulerStateMachineTest,
     TestTriggerDeadlineImmediatelyAfterAbortedCommit) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  // This test mirrors what happens during the first frame of a scroll gesture.
  // First we get the input event and a BeginFrame.
  state.IssueNextBeginImplFrame();

  // As a response the compositor requests a redraw and a commit to tell the
  // main thread about the new scroll offset.
  state.SetNeedsRedraw(true);
  state.SetNeedsBeginMainFrame();

  // We should start the commit normally.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Since only the scroll offset changed, the main thread will abort the
  // commit.
  state.NotifyBeginMainFrameStarted();
  state.BeginMainFrameAborted(CommitEarlyOutReason::FINISHED_NO_UPDATES);

  // Since the commit was aborted, we should draw right away instead of waiting
  // for the deadline.
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
}

void FinishPreviousCommitAndDrawWithoutExitingDeadline(
    StateMachine* state_ptr) {
  // Gross, but allows us to use macros below.
  StateMachine& state = *state_ptr;

  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
}

TEST(SchedulerStateMachineTest, TestImplLatencyTakesPriority) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  // This test ensures that impl-draws are prioritized over main thread updates
  // in prefer impl latency mode.
  state.SetNeedsRedraw(true);
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Verify the deadline is not triggered early until we enter
  // prefer impl latency mode.
  EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  state.SetTreePrioritiesAndScrollState(
      SMOOTHNESS_TAKES_PRIORITY,
      ScrollHandlerState::SCROLL_DOES_NOT_AFFECT_SCROLL_HANDLER);
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());

  // Trigger the deadline.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.DidReceiveCompositorFrameAck();

  // Request a new commit and finish the previous one.
  state.SetNeedsBeginMainFrame();
  FinishPreviousCommitAndDrawWithoutExitingDeadline(&state);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Finish the previous commit and draw it.
  FinishPreviousCommitAndDrawWithoutExitingDeadline(&state);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Verify we do not send another BeginMainFrame if was are submit-frame
  // throttled and did not just submit one.
  state.SetNeedsBeginMainFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest,
     TestTriggerDeadlineImmediatelyOnLostCompositorFrameSink) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  state.SetNeedsBeginMainFrame();

  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());

  state.DidLoseCompositorFrameSink();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  // The deadline should be triggered immediately when CompositorFrameSink is
  // lost.
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
}

TEST(SchedulerStateMachineTest, TestTriggerDeadlineImmediatelyWhenInvisible) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  state.SetNeedsBeginMainFrame();

  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());

  state.SetVisible(false);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.PendingActivationsShouldBeForced());
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
}

TEST(SchedulerStateMachineTest,
     TestTriggerDeadlineImmediatelyWhenBeginFrameSourcePaused) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  state.SetNeedsBeginMainFrame();

  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());

  state.SetBeginFrameSourcePaused(true);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_TRUE(state.PendingActivationsShouldBeForced());
  EXPECT_TRUE(state.ShouldTriggerBeginImplFrameDeadlineImmediately());
}

TEST(SchedulerStateMachineTest, TestDeferCommit) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state)

  state.SetDeferCommits(true);

  state.SetNeedsBeginMainFrame();
  EXPECT_FALSE(state.BeginFrameNeeded());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  state.SetDeferCommits(false);
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
}

TEST(SchedulerStateMachineTest, EarlyOutCommitWantsProactiveBeginFrame) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  EXPECT_FALSE(state.ProactiveBeginFrameWanted());
  bool commit_has_no_updates = true;
  state.WillCommit(commit_has_no_updates);
  EXPECT_TRUE(state.ProactiveBeginFrameWanted());
  state.IssueNextBeginImplFrame();
  EXPECT_FALSE(state.ProactiveBeginFrameWanted());
}

TEST(SchedulerStateMachineTest,
     NoCompositorFrameSinkCreationWhileCommitPending) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Set up the request for a commit and start a frame.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  PerformAction(&state, SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);

  // Lose the CompositorFrameSink.
  state.DidLoseCompositorFrameSink();

  // The scheduler shouldn't trigger the CompositorFrameSink creation till the
  // previous commit has been cleared.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Trigger the deadline and ensure that the scheduler does not trigger any
  // actions until we receive a response for the pending commit.
  state.OnBeginImplFrameDeadline();
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Abort the commit, since that is what we expect the main thread to do if the
  // CompositorFrameSink was lost due to a synchronous call from the main thread
  // to release the CompositorFrameSink.
  state.NotifyBeginMainFrameStarted();
  state.BeginMainFrameAborted(
      CommitEarlyOutReason::ABORTED_COMPOSITOR_FRAME_SINK_LOST);

  // The scheduler should begin the CompositorFrameSink creation now.
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
}

TEST(SchedulerStateMachineTest, NoImplSideInvalidationsWhileInvisible) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // No impl-side invalidations should be performed while we are not visible.
  state.SetVisible(false);
  state.SetNeedsImplSideInvalidation();
  state.IssueNextBeginImplFrame();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest,
     NoImplSideInvalidationsWhenBeginFrameSourcePaused) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // No impl-side invalidations should be performed while we can not make impl
  // frames.
  state.SetBeginFrameSourcePaused(true);
  state.SetNeedsImplSideInvalidation();
  state.IssueNextBeginImplFrame();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, ImplSideInvalidationOnlyInsideDeadline) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  state.SetNeedsImplSideInvalidation();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_PERFORM_IMPL_SIDE_INVALIDATION);
}

TEST(SchedulerStateMachineTest,
     NoImplSideInvalidationWithoutCompositorFrameSink) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Impl-side invalidations should not be triggered till the frame sink is
  // initialized.
  state.DidLoseCompositorFrameSink();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // No impl-side invalidations should be performed during frame sink creation.
  state.SetNeedsImplSideInvalidation();
  state.IssueNextBeginImplFrame();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Initializing the CompositorFrameSink puts us in a state waiting for the
  // first commit.
  state.DidCreateAndInitializeCompositorFrameSink();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  state.NotifyBeginMainFrameStarted();
  state.BeginMainFrameAborted(CommitEarlyOutReason::FINISHED_NO_UPDATES);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_PERFORM_IMPL_SIDE_INVALIDATION);
}

TEST(SchedulerStateMachineTest, ImplSideInvalidationWhenPendingTreeExists) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Set up request for the main frame, commit and create the pending tree.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);

  // Request an impl-side invalidation after the commit. The request should wait
  // till the current pending tree is activated.
  state.SetNeedsImplSideInvalidation();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Activate the pending tree. Since the commit fills the impl-side
  // invalidation funnel as well, the request should wait until the next
  // BeginFrame.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Since there is no main frame request, this should perform impl-side
  // invalidations.
  state.IssueNextBeginImplFrame();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_PERFORM_IMPL_SIDE_INVALIDATION);
}

TEST(SchedulerStateMachineTest, ImplSideInvalidationWhileReadyToCommit) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Set up request for the main frame.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();

  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Request an impl-side invalidation. The request should wait till a response
  // is received from the main thread.
  state.SetNeedsImplSideInvalidation();
  EXPECT_TRUE(state.needs_impl_side_invalidation());
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Perform a commit, the impl-side invalidation request should be reset since
  // they will be merged with the commit.
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_FALSE(state.needs_impl_side_invalidation());

  // Deadline.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest,
     ConsecutiveImplSideInvalidationsWaitForBeginFrame) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Set up a request for impl-side invalidation.
  state.SetNeedsImplSideInvalidation();
  state.IssueNextBeginImplFrame();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_PERFORM_IMPL_SIDE_INVALIDATION);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Request another invalidation, which should wait until the pending tree is
  // activated *and* we start the next BeginFrame.
  state.SetNeedsImplSideInvalidation();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Now start the next frame, which will first draw the active tree and then
  // perform the pending impl-side invalidation request.
  state.IssueNextBeginImplFrame();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_PERFORM_IMPL_SIDE_INVALIDATION);
}

TEST(SchedulerStateMachineTest, ImplSideInvalidationsThrottledOnDraw) {
  // In commit_to_active_tree mode, performing the next invalidation should be
  // throttled on the active tree being drawn.
  SchedulerSettings settings;
  settings.commit_to_active_tree = true;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Commit to the sync tree, activate and draw.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  state.NotifyBeginMainFrameStarted();
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  state.NotifyReadyToDraw();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  state.DidSubmitCompositorFrame();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Request impl-side invalidation and start a new frame, which should be
  // blocked on the ack for the previous frame.
  state.SetNeedsImplSideInvalidation();
  state.IssueNextBeginImplFrame();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Ack the previous frame and begin impl frame, which should perform the
  // invalidation now.
  state.DidReceiveCompositorFrameAck();
  state.IssueNextBeginImplFrame();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_PERFORM_IMPL_SIDE_INVALIDATION);
}

TEST(SchedulerStateMachineTest,
     ImplSideInvalidationsWhenMainFrameRequestIsPending) {
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Set up request for the main frame.
  state.SetNeedsBeginMainFrame();
  state.IssueNextBeginImplFrame();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Request an impl-side invalidation and trigger the deadline, the
  // invalidation should run if the request is still pending when we enter the
  // deadline.
  state.SetNeedsImplSideInvalidation();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_PERFORM_IMPL_SIDE_INVALIDATION);
}

TEST(SchedulerStateMachineTest, PrepareTilesWaitForImplSideInvalidation) {
  // PrepareTiles
  SchedulerSettings settings;
  StateMachine state(settings);
  SET_UP_STATE(state);

  // Request a PrepareTiles and impl-side invalidation. The impl-side
  // invalidation should run first, since it will perform PrepareTiles as well.
  state.SetNeedsImplSideInvalidation();
  state.SetNeedsPrepareTiles();
  state.IssueNextBeginImplFrame();
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_PERFORM_IMPL_SIDE_INVALIDATION);
  state.DidPrepareTiles();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestBeginFrameFreshnessInitialState) {
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)

  // Initially, we report invalid frame numbers.
  EXPECT_SEQUENCE_NUMBERS(
      BeginFrameArgs::kInvalidFrameNumber, BeginFrameArgs::kInvalidFrameNumber,
      BeginFrameArgs::kInvalidFrameNumber, BeginFrameArgs::kInvalidFrameNumber,
      BeginFrameArgs::kInvalidFrameNumber);
}

TEST(SchedulerStateMachineTest, TestBeginFrameFreshnessWithoutUpdates) {
  // Setup without any requested updates.
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)
  state.SetNeedsRedraw(false);
  EXPECT_FALSE(state.RedrawPending());
  EXPECT_FALSE(state.NeedsCommit());

  // OnBeginImplFrame() updates the sequence number.
  state.OnBeginImplFrame(0, 10);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);

  // When no updates are required, OnBeginImplFrameDeadline() updates active
  // tree and compositor frame freshness.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber, 10u, 10u);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestBeginFrameFreshnessWithImplFrameUpdates) {
  // Setup with an impl-thread draw requested.
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)
  state.SetNeedsRedraw(true);
  EXPECT_TRUE(state.RedrawPending());
  EXPECT_FALSE(state.NeedsCommit());

  // OnBeginImplFrame() updates the sequence number.
  state.OnBeginImplFrame(0, 10);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);

  // With only an impl-thread draw requested, OnBeginImplFrameDeadline()
  // updates the active tree freshness.
  state.OnBeginImplFrameDeadline();
  EXPECT_SEQUENCE_NUMBERS(10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber, 10u,
                          BeginFrameArgs::kInvalidFrameNumber);

  // Compositor frame freshness is updated when active tree is drawn.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.RedrawPending());
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber, 10u, 10u);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest, TestBeginFrameFreshnessWithMainFrameUpdates) {
  // Setup with a BeginMainFrame requested.
  SchedulerSettings default_scheduler_settings;
  StateMachine state(default_scheduler_settings);
  SET_UP_STATE(state)
  state.SetNeedsRedraw(false);
  state.SetNeedsBeginMainFrameForTest(true);
  EXPECT_FALSE(state.RedrawPending());
  EXPECT_TRUE(state.NeedsCommit());

  // OnBeginImplFrame() updates the sequence number, WillSendBeginMainFrame()
  // updates begin_main_frame_sent sequence number.
  state.OnBeginImplFrame(0, 10);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_TRUE(state.CommitPending());
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);

  // When a BeginMainFrame is needed, OnBeginImplFrameDeadline() updates no
  // freshness numbers.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // When a main frame is in progress, OnBeginImplFrameDeadline() updates no
  // freshness numbers.
  state.NotifyBeginMainFrameStarted();
  state.OnBeginImplFrame(0, 11);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(11u, 10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Pending tree freshness is updated when the new pending tree is committed.
  state.NotifyReadyToCommit();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_COMMIT);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.CommitPending());
  EXPECT_TRUE(state.has_pending_tree());
  EXPECT_SEQUENCE_NUMBERS(11u, 10u, 10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);

  // If no further BeginMainFrame is needed, OnBeginFrameImplDeadline()
  // updates the pending tree's frame number.
  state.OnBeginImplFrame(0, 12);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(12u, 10u, 10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(12u, 10u, 12u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // If a further BeginMainFrame is needed, OnBeginFrameImplDeadline()
  // updates no freshness numbers.
  state.SetNeedsBeginMainFrameForTest(true);
  EXPECT_TRUE(state.NeedsCommit());
  state.OnBeginImplFrame(0, 13);
  // BMF will be sent only after pending_tree activation.
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(13u, 10u, 12u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Active tree freshness is updated when pending frame is activated.
  state.NotifyReadyToActivate();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.has_pending_tree());
  EXPECT_SEQUENCE_NUMBERS(13u, 10u, 12u, 12u,
                          BeginFrameArgs::kInvalidFrameNumber);
  EXPECT_TRUE(state.RedrawPending());

  // BMF requested above will be sent after activation.
  state.OnBeginImplFrame(0, 14);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Active tree freshness is updated when commit didn't have updates, but
  // compositor frame freshness is not updated.
  state.NotifyBeginMainFrameStarted();
  state.BeginMainFrameAborted(CommitEarlyOutReason::FINISHED_NO_UPDATES);
  EXPECT_SEQUENCE_NUMBERS(14u, 14u, 12u, 14u,
                          BeginFrameArgs::kInvalidFrameNumber);

  // Compositor frame freshness is updated when active tree is drawn.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.RedrawPending());
  state.DidSubmitCompositorFrame();
  state.DidReceiveCompositorFrameAck();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(14u, 14u, 12u, 14u, 14u);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // When no updates are required, OnBeginImplFrameDeadline() updates active
  // tree and compositor frame freshness.
  state.OnBeginImplFrame(0, 15);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(15u, 14u, 12u, 15u, 15u);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // Active tree and compositor frame freshness are updated when commit doesn't
  // have updates and compositor frame was fresh before.
  state.SetNeedsBeginMainFrameForTest(true);
  EXPECT_TRUE(state.NeedsCommit());
  state.OnBeginImplFrame(0, 16);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_TRUE(state.CommitPending());
  state.NotifyBeginMainFrameStarted();
  state.BeginMainFrameAborted(CommitEarlyOutReason::FINISHED_NO_UPDATES);
  EXPECT_SEQUENCE_NUMBERS(16u, 16u, 12u, 16u, 16u);
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(16u, 16u, 12u, 16u, 16u);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);

  // When the source changes, the current frame number is updated and frame
  // numbers for freshness are reset to invalid numbers.
  state.OnBeginImplFrame(1, 5);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(5u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);

  // When no updates are required, OnBeginImplFrameDeadline() updates active
  // tree and compositor frame freshness.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(5u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber, 5u, 5u);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

TEST(SchedulerStateMachineTest,
     TestBeginFrameFreshnessWithNoUpdatesCommitAndPendingTree) {
  // Setup with main_frame_before_activation_enabled, a pending tree, and a
  // new BeginMainFrame requested.
  SchedulerSettings settings;
  settings.main_frame_before_activation_enabled = true;
  StateMachine state(settings);
  SET_UP_STATE(state)
  state.SetHasPendingTree(true);
  state.SetNeedsRedraw(false);
  state.SetNeedsBeginMainFrameForTest(true);
  EXPECT_FALSE(state.RedrawPending());
  EXPECT_TRUE(state.NeedsCommit());

  // OnBeginImplFrame() updates the sequence number, WillSendBeginMainFrame()
  // updates begin_main_frame_sent sequence number.
  state.OnBeginImplFrame(0, 10);
  EXPECT_ACTION_UPDATE_STATE(
      SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME);
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_FALSE(state.NeedsCommit());
  EXPECT_TRUE(state.CommitPending());
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);

  // Pending tree freshness is updated when commit didn't have updates, but an
  // older pending tree is still pending.
  state.NotifyBeginMainFrameStarted();
  state.BeginMainFrameAborted(CommitEarlyOutReason::FINISHED_NO_UPDATES);
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, 10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);

  // As no further BeginMainFrame is needed, OnBeginFrameImplDeadline()
  // updates the pending tree's frame number, but without any visible change.
  state.OnBeginImplFrameDeadline();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
  EXPECT_SEQUENCE_NUMBERS(10u, 10u, 10u, BeginFrameArgs::kInvalidFrameNumber,
                          BeginFrameArgs::kInvalidFrameNumber);
  state.OnBeginImplFrameIdle();
  EXPECT_ACTION_UPDATE_STATE(SchedulerStateMachine::ACTION_NONE);
}

}  // namespace
}  // namespace cc
