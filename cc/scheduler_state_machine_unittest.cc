// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler_state_machine.h"

#include "cc/scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

namespace {

const SchedulerStateMachine::CommitState allCommitStates[] = {
    SchedulerStateMachine::COMMIT_STATE_IDLE,
    SchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS,
    SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT,
    SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW
};

// Exposes the protected state fields of the SchedulerStateMachine for testing
class StateMachine : public SchedulerStateMachine {
public:
    StateMachine(const SchedulerSettings& schedulerSettings)
      : SchedulerStateMachine(schedulerSettings) { }
    void setCommitState(CommitState cs) { m_commitState = cs; }
    CommitState commitState() const { return  m_commitState; }

    bool needsCommit() const { return m_needsCommit; }

    void setNeedsRedraw(bool b) { m_needsRedraw = b; }
    bool needsRedraw() const { return m_needsRedraw; }

    void setNeedsForcedRedraw(bool b) { m_needsForcedRedraw = b; }
    bool needsForcedRedraw() const { return m_needsForcedRedraw; }

    bool canDraw() const { return m_canDraw; }
    bool insideVSync() const { return m_insideVSync; }
    bool visible() const { return m_visible; }
};

TEST(SchedulerStateMachineTest, TestNextActionBeginsFrameIfNeeded)
{
    SchedulerSettings defaultSchedulerSettings;

    // If no commit needed, do nothing
    {    
        StateMachine state(defaultSchedulerSettings);
        state.setCommitState(SchedulerStateMachine::COMMIT_STATE_IDLE);
        state.setCanBeginFrame(true);
        state.setNeedsRedraw(false);
        state.setVisible(true);

        EXPECT_FALSE(state.vsyncCallbackNeeded());

        state.didLeaveVSync();
        EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
        EXPECT_FALSE(state.vsyncCallbackNeeded());
        state.didEnterVSync();
        EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
    }

    // If commit requested but canBeginFrame is still false, do nothing.
    {
        StateMachine state(defaultSchedulerSettings);
        state.setCommitState(SchedulerStateMachine::COMMIT_STATE_IDLE);
        state.setNeedsRedraw(false);
        state.setVisible(true);

        EXPECT_FALSE(state.vsyncCallbackNeeded());

        state.didLeaveVSync();
        EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
        EXPECT_FALSE(state.vsyncCallbackNeeded());
        state.didEnterVSync();
        EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
    }


    // If commit requested, begin a frame
    {
        StateMachine state(defaultSchedulerSettings);
        state.setCommitState(SchedulerStateMachine::COMMIT_STATE_IDLE);
        state.setCanBeginFrame(true);
        state.setNeedsRedraw(false);
        state.setVisible(true);
        EXPECT_FALSE(state.vsyncCallbackNeeded());
    }

    // Begin the frame, make sure needsCommit and commitState update correctly.
    {
        StateMachine state(defaultSchedulerSettings);
        state.setCanBeginFrame(true);
        state.setVisible(true);
        state.updateState(SchedulerStateMachine::ACTION_BEGIN_FRAME);
        EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
        EXPECT_FALSE(state.needsCommit());
        EXPECT_FALSE(state.vsyncCallbackNeeded());
    }
}

TEST(SchedulerStateMachineTest, TestSetForcedRedrawDoesNotSetsNormalRedraw)
{
    SchedulerSettings defaultSchedulerSettings;
    SchedulerStateMachine state(defaultSchedulerSettings);
    state.setCanDraw(true);
    state.setNeedsForcedRedraw();
    EXPECT_FALSE(state.redrawPending());
    EXPECT_TRUE(state.vsyncCallbackNeeded());
}

TEST(SchedulerStateMachineTest, TestFailedDrawSetsNeedsCommitAndDoesNotDrawAgain)
{
    SchedulerSettings defaultSchedulerSettings;
    SchedulerStateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);
    state.setNeedsRedraw();
    EXPECT_TRUE(state.redrawPending());
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();

    // We're drawing now.
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
    EXPECT_FALSE(state.redrawPending());
    EXPECT_FALSE(state.commitPending());

    // Failing the draw makes us require a commit.
    state.didDrawIfPossibleCompleted(false);
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(SchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_TRUE(state.redrawPending());
    EXPECT_TRUE(state.commitPending());
}

TEST(SchedulerStateMachineTest, TestSetNeedsRedrawDuringFailedDrawDoesNotRemoveNeedsRedraw)
{
    SchedulerSettings defaultSchedulerSettings;
    SchedulerStateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);
    state.setNeedsRedraw();
    EXPECT_TRUE(state.redrawPending());
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();

    // We're drawing now.
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
    EXPECT_FALSE(state.redrawPending());
    EXPECT_FALSE(state.commitPending());

    // While still in the same vsync callback, set needs redraw again.
    // This should not redraw.
    state.setNeedsRedraw();
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Failing the draw makes us require a commit.
    state.didDrawIfPossibleCompleted(false);
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    EXPECT_TRUE(state.redrawPending());
}

TEST(SchedulerStateMachineTest, TestCommitAfterFailedDrawAllowsDrawInSameFrame)
{
    SchedulerSettings defaultSchedulerSettings;
    SchedulerStateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Start a commit.
    state.setNeedsCommit();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(SchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_TRUE(state.commitPending());

    // Then initiate a draw.
    state.setNeedsRedraw();
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    EXPECT_TRUE(state.redrawPending());

    // Fail the draw.
    state.updateState(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.didDrawIfPossibleCompleted(false);
    EXPECT_TRUE(state.redrawPending());
    // But the commit is ongoing.
    EXPECT_TRUE(state.commitPending());

    // Finish the commit.
    state.beginFrameComplete();
    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.updateState(SchedulerStateMachine::ACTION_COMMIT);
    EXPECT_TRUE(state.redrawPending());

    // And we should be allowed to draw again.
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestCommitAfterFailedAndSuccessfulDrawDoesNotAllowDrawInSameFrame)
{
    SchedulerSettings defaultSchedulerSettings;
    SchedulerStateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Start a commit.
    state.setNeedsCommit();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(SchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_TRUE(state.commitPending());

    // Then initiate a draw.
    state.setNeedsRedraw();
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    EXPECT_TRUE(state.redrawPending());

    // Fail the draw.
    state.updateState(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.didDrawIfPossibleCompleted(false);
    EXPECT_TRUE(state.redrawPending());
    // But the commit is ongoing.
    EXPECT_TRUE(state.commitPending());

    // Force a draw.
    state.setNeedsForcedRedraw();
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_FORCED, state.nextAction());

    // Do the forced draw.
    state.updateState(SchedulerStateMachine::ACTION_DRAW_FORCED);
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
    EXPECT_FALSE(state.redrawPending());
    // And the commit is still ongoing.
    EXPECT_TRUE(state.commitPending());

    // Finish the commit.
    state.beginFrameComplete();
    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.updateState(SchedulerStateMachine::ACTION_COMMIT);
    EXPECT_TRUE(state.redrawPending());

    // And we should not be allowed to draw again in the same frame..
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestFailedDrawsWillEventuallyForceADrawAfterTheNextCommit)
{
    SchedulerSettings defaultSchedulerSettings;
    SchedulerStateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);
    state.setMaximumNumberOfFailedDrawsBeforeDrawIsForced(1);

    // Start a commit.
    state.setNeedsCommit();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(SchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_TRUE(state.commitPending());

    // Then initiate a draw.
    state.setNeedsRedraw();
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    EXPECT_TRUE(state.redrawPending());

    // Fail the draw.
    state.updateState(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.didDrawIfPossibleCompleted(false);
    EXPECT_TRUE(state.redrawPending());
    // But the commit is ongoing.
    EXPECT_TRUE(state.commitPending());

    // Finish the commit. Note, we should not yet be forcing a draw, but should
    // continue the commit as usual.
    state.beginFrameComplete();
    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.updateState(SchedulerStateMachine::ACTION_COMMIT);
    EXPECT_TRUE(state.redrawPending());

    // The redraw should be forced in this case.
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_FORCED, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestFailedDrawIsRetriedNextVSync)
{
    SchedulerSettings defaultSchedulerSettings;
    SchedulerStateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Start a draw.
    state.setNeedsRedraw();
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    EXPECT_TRUE(state.redrawPending());

    // Fail the draw.
    state.updateState(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.didDrawIfPossibleCompleted(false);
    EXPECT_TRUE(state.redrawPending());

    // We should not be trying to draw again now, but we have a commit pending.
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());

    state.didLeaveVSync();
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();

    // We should try draw again in the next vsync.
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestDoestDrawTwiceInSameFrame)
{
    SchedulerSettings defaultSchedulerSettings;
    SchedulerStateMachine state(defaultSchedulerSettings);
    state.setVisible(true);
    state.setCanDraw(true);
    state.setNeedsRedraw();
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);

    // While still in the same vsync callback, set needs redraw again.
    // This should not redraw.
    state.setNeedsRedraw();
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Move to another frame. This should now draw.
    state.didDrawIfPossibleCompleted(true);
    state.didLeaveVSync();
    EXPECT_TRUE(state.vsyncCallbackNeeded());
    state.didEnterVSync();

    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    state.didDrawIfPossibleCompleted(true);
    EXPECT_FALSE(state.vsyncCallbackNeeded());
}

TEST(SchedulerStateMachineTest, TestNextActionDrawsOnVSync)
{
    SchedulerSettings defaultSchedulerSettings;

    // When not on vsync, or on vsync but not visible, don't draw.
    size_t numCommitStates = sizeof(allCommitStates) / sizeof(SchedulerStateMachine::CommitState);
    for (size_t i = 0; i < numCommitStates; ++i) {
        for (unsigned j = 0; j < 2; ++j) {
            StateMachine state(defaultSchedulerSettings);
            state.setCommitState(allCommitStates[i]);
            bool visible = j;
            if (!visible) {
                state.didEnterVSync();
                state.setVisible(false);
            } else
                state.setVisible(true);

            // Case 1: needsCommit=false
            EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());

            // Case 2: needsCommit=true
            state.setNeedsCommit();
            EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
        }
    }

    // When on vsync, or not on vsync but needsForcedRedraw set, should always draw except if you're ready to commit, in which case commit.
    for (size_t i = 0; i < numCommitStates; ++i) {
        for (unsigned j = 0; j < 2; ++j) {
            StateMachine state(defaultSchedulerSettings);
            state.setCanDraw(true);
            state.setCommitState(allCommitStates[i]);
            bool forcedDraw = j;
            if (!forcedDraw) {
                state.didEnterVSync();
                state.setNeedsRedraw(true);
                state.setVisible(true);
            } else
                state.setNeedsForcedRedraw(true);

            SchedulerStateMachine::Action expectedAction;
            if (allCommitStates[i] != SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT)
                expectedAction = forcedDraw ? SchedulerStateMachine::ACTION_DRAW_FORCED : SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE;
            else
                expectedAction = SchedulerStateMachine::ACTION_COMMIT;

            // Case 1: needsCommit=false.
            EXPECT_TRUE(state.vsyncCallbackNeeded());
            EXPECT_EQ(expectedAction, state.nextAction());

            // Case 2: needsCommit=true.
            state.setNeedsCommit();
            EXPECT_TRUE(state.vsyncCallbackNeeded());
            EXPECT_EQ(expectedAction, state.nextAction());
        }
    }
}

TEST(SchedulerStateMachineTest, TestNoCommitStatesRedrawWhenInvisible)
{
    SchedulerSettings defaultSchedulerSettings;

    size_t numCommitStates = sizeof(allCommitStates) / sizeof(SchedulerStateMachine::CommitState);
    for (size_t i = 0; i < numCommitStates; ++i) {
        // There shouldn't be any drawing regardless of vsync.
        for (unsigned j = 0; j < 2; ++j) {
            StateMachine state(defaultSchedulerSettings);
            state.setCommitState(allCommitStates[i]);
            state.setVisible(false);
            state.setNeedsRedraw(true);
            state.setNeedsForcedRedraw(false);
            if (j == 1)
                state.didEnterVSync();

            // Case 1: needsCommit=false.
            EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());

            // Case 2: needsCommit=true.
            state.setNeedsCommit();
            EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
        }
    }
}

TEST(SchedulerStateMachineTest, TestCanRedraw_StopsDraw)
{
    SchedulerSettings defaultSchedulerSettings;

    size_t numCommitStates = sizeof(allCommitStates) / sizeof(SchedulerStateMachine::CommitState);
    for (size_t i = 0; i < numCommitStates; ++i) {
        // There shouldn't be any drawing regardless of vsync.
        for (unsigned j = 0; j < 2; ++j) {
            StateMachine state(defaultSchedulerSettings);
            state.setCommitState(allCommitStates[i]);
            state.setVisible(false);
            state.setNeedsRedraw(true);
            state.setNeedsForcedRedraw(false);
            if (j == 1)
                state.didEnterVSync();

            state.setCanDraw(false);
            EXPECT_NE(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
        }
    }
}

TEST(SchedulerStateMachineTest, TestCanRedrawWithWaitingForFirstDrawMakesProgress)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCommitState(SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW);
    state.setCanBeginFrame(true);
    state.setNeedsCommit();
    state.setNeedsRedraw(true);
    state.setVisible(true);
    state.setCanDraw(false);
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestSetNeedsCommitIsNotLost)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setNeedsCommit();
    state.setVisible(true);
    state.setCanDraw(true);

    // Begin the frame.
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(state.nextAction());
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());

    // Now, while the frame is in progress, set another commit.
    state.setNeedsCommit();
    EXPECT_TRUE(state.needsCommit());

    // Let the frame finish.
    state.beginFrameComplete();
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());

    // Expect to commit regardless of vsync state.
    state.didLeaveVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());

    // Commit and make sure we draw on next vsync
    state.updateState(SchedulerStateMachine::ACTION_COMMIT);
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());
    state.updateState(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    state.didDrawIfPossibleCompleted(true);

    // Verify that another commit will begin.
    state.didLeaveVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestFullCycle)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Start clean and set commit.
    state.setNeedsCommit();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());

    // Begin the frame.
    state.updateState(SchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
    EXPECT_FALSE(state.needsCommit());
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Tell the scheduler the frame finished.
    state.beginFrameComplete();
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());
    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());

    // Commit.
    state.updateState(SchedulerStateMachine::ACTION_COMMIT);
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());
    EXPECT_TRUE(state.needsRedraw());

    // Expect to do nothing until vsync.
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());

    // At vsync, draw.
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    state.didDrawIfPossibleCompleted(true);
    state.didLeaveVSync();

    // Should be synchronized, no draw needed, no action needed.
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());
    EXPECT_FALSE(state.needsRedraw());
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestFullCycleWithCommitRequestInbetween)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Start clean and set commit.
    state.setNeedsCommit();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());

    // Begin the frame.
    state.updateState(SchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
    EXPECT_FALSE(state.needsCommit());
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Request another commit while the commit is in flight.
    state.setNeedsCommit();
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Tell the scheduler the frame finished.
    state.beginFrameComplete();
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());
    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());

    // Commit.
    state.updateState(SchedulerStateMachine::ACTION_COMMIT);
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());
    EXPECT_TRUE(state.needsRedraw());

    // Expect to do nothing until vsync.
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());

    // At vsync, draw.
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE);
    state.didDrawIfPossibleCompleted(true);
    state.didLeaveVSync();

    // Should be synchronized, no draw needed, no action needed.
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());
    EXPECT_FALSE(state.needsRedraw());
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestRequestCommitInvisible)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setNeedsCommit();
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestGoesInvisibleBeforeBeginFrameCompletes)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Start clean and set commit.
    state.setNeedsCommit();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());

    // Begin the frame while visible.
    state.updateState(SchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
    EXPECT_FALSE(state.needsCommit());
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Become invisible and abort the beginFrame.
    state.setVisible(false);
    state.beginFrameAborted();

    // We should now be back in the idle state as if we didn't start a frame at all.
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Become visible again
    state.setVisible(true);

    // We should be beginning a frame now
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());

    // Begin the frame
    state.updateState(state.nextAction());

    // We should be starting the commit now
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
}

TEST(SchedulerStateMachineTest, TestContextLostWhenCompletelyIdle)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    state.didLoseOutputSurface();

    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_RECREATION, state.nextAction());
    state.updateState(state.nextAction());

    // Once context recreation begins, nothing should happen.
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Recreate the context
    state.didRecreateOutputSurface();

    // When the context is recreated, we should begin a commit
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(state.nextAction());
}

TEST(SchedulerStateMachineTest, TestContextLostWhenIdleAndCommitRequestedWhileRecreating)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    state.didLoseOutputSurface();

    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_RECREATION, state.nextAction());
    state.updateState(state.nextAction());

    // Once context recreation begins, nothing should happen.
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());

    // While context is recreating, commits shouldn't begin.
    state.setNeedsCommit();
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Recreate the context
    state.didRecreateOutputSurface();

    // When the context is recreated, we should begin a commit
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(state.nextAction());

    // Once the context is recreated, whether we draw should be based on
    // setCanDraw.
    state.setNeedsRedraw(true);
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.setCanDraw(false);
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.setCanDraw(true);
    state.didLeaveVSync();
}

TEST(SchedulerStateMachineTest, TestContextLostWhileCommitInProgress)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Get a commit in flight.
    state.setNeedsCommit();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(state.nextAction());

    // Set damage and expect a draw.
    state.setNeedsRedraw(true);
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(state.nextAction());
    state.didLeaveVSync();

    // Cause a lost context while the begin frame is in flight.
    state.didLoseOutputSurface();

    // Ask for another draw. Expect nothing happens.
    state.setNeedsRedraw(true);
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Finish the frame, and commit.
    state.beginFrameComplete();
    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.updateState(state.nextAction());

    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());

    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(state.nextAction());

    // Expect to be told to begin context recreation, independent of vsync state
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_RECREATION, state.nextAction());
    state.didLeaveVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_RECREATION, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestContextLostWhileCommitInProgressAndAnotherCommitRequested)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Get a commit in flight.
    state.setNeedsCommit();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(state.nextAction());

    // Set damage and expect a draw.
    state.setNeedsRedraw(true);
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(state.nextAction());
    state.didLeaveVSync();

    // Cause a lost context while the begin frame is in flight.
    state.didLoseOutputSurface();

    // Ask for another draw and also set needs commit. Expect nothing happens.
    state.setNeedsRedraw(true);
    state.setNeedsCommit();
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Finish the frame, and commit.
    state.beginFrameComplete();
    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.updateState(state.nextAction());

    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());

    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE, state.nextAction());
    state.updateState(state.nextAction());

    // Expect to be told to begin context recreation, independent of vsync state
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_RECREATION, state.nextAction());
    state.didLeaveVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_RECREATION, state.nextAction());
}


TEST(SchedulerStateMachineTest, TestFinishAllRenderingWhileContextLost)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setVisible(true);
    state.setCanDraw(true);

    // Cause a lost context lost.
    state.didLoseOutputSurface();

    // Ask a forced redraw and verify it ocurrs.
    state.setNeedsForcedRedraw(true);
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_FORCED, state.nextAction());
    state.didLeaveVSync();

    // Clear the forced redraw bit.
    state.setNeedsForcedRedraw(false);

    // Expect to be told to begin context recreation, independent of vsync state
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_RECREATION, state.nextAction());
    state.updateState(state.nextAction());

    // Ask a forced redraw and verify it ocurrs.
    state.setNeedsForcedRedraw(true);
    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_FORCED, state.nextAction());
    state.didLeaveVSync();
}

TEST(SchedulerStateMachineTest, TestBeginFrameWhenInvisibleAndForceCommit)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(false);
    state.setNeedsCommit();
    state.setNeedsForcedCommit();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestBeginFrameWhenCanBeginFrameFalseAndForceCommit)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setVisible(true);
    state.setCanDraw(true);
    state.setNeedsCommit();
    state.setNeedsForcedCommit();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestBeginFrameWhenCommitInProgress)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(false);
    state.setCommitState(SchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS);
    state.setNeedsCommit();

    state.beginFrameComplete();
    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.updateState(state.nextAction());

    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());

    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestBeginFrameWhenForcedCommitInProgress)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(false);
    state.setCommitState(SchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS);
    state.setNeedsCommit();
    state.setNeedsForcedCommit();

    state.beginFrameComplete();
    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.updateState(state.nextAction());

    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW, state.commitState());

    // If we are waiting for forced draw then we know a begin frame is already in flight.
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestBeginFrameWhenContextLost)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);
    state.setNeedsCommit();
    state.setNeedsForcedCommit();
    state.didLoseOutputSurface();
    EXPECT_EQ(SchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

TEST(SchedulerStateMachineTest, TestImmediateBeginFrame)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Schedule a forced frame, commit it, draw it.
    state.setNeedsCommit();
    state.setNeedsForcedCommit();
    state.updateState(state.nextAction());
    state.beginFrameComplete();
    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());
    state.updateState(state.nextAction());

    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW, state.commitState());

    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.setNeedsForcedRedraw(true);
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_FORCED, state.nextAction());
    state.updateState(state.nextAction());
    state.didDrawIfPossibleCompleted(true);
    state.didLeaveVSync();

    // Should be waiting for the normal begin frame
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
}

TEST(SchedulerStateMachineTest, TestImmediateBeginFrameDuringCommit)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    // Start a normal commit.
    state.setNeedsCommit();
    state.updateState(state.nextAction());

    // Schedule a forced frame, commit it, draw it.
    state.setNeedsCommit();
    state.setNeedsForcedCommit();
    state.updateState(state.nextAction());
    state.beginFrameComplete();
    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());
    state.updateState(state.nextAction());

    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW, state.commitState());

    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.setNeedsForcedRedraw(true);
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_FORCED, state.nextAction());
    state.updateState(state.nextAction());
    state.didDrawIfPossibleCompleted(true);
    state.didLeaveVSync();

    // Should be waiting for the normal begin frame
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState()) << state.toString();
}

TEST(SchedulerStateMachineTest, ImmediateBeginFrameWhileInvisible)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(true);

    state.setNeedsCommit();
    state.updateState(state.nextAction());

    state.setNeedsCommit();
    state.setNeedsForcedCommit();
    state.updateState(state.nextAction());
    state.beginFrameComplete();

    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());
    state.updateState(state.nextAction());

    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW, state.commitState());

    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.setNeedsForcedRedraw(true);
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_FORCED, state.nextAction());
    state.updateState(state.nextAction());
    state.didDrawIfPossibleCompleted(true);
    state.didLeaveVSync();

    // Should be waiting for the normal begin frame
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState()) << state.toString();


    // Become invisible and abort the "normal" begin frame.
    state.setVisible(false);
    state.beginFrameAborted();

    // Should be back in the idle state, but needing a commit.
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());
    EXPECT_TRUE(state.needsCommit());
}

TEST(SchedulerStateMachineTest, ImmediateBeginFrameWhileCantDraw)
{
    SchedulerSettings defaultSchedulerSettings;
    StateMachine state(defaultSchedulerSettings);
    state.setCanBeginFrame(true);
    state.setVisible(true);
    state.setCanDraw(false);

    state.setNeedsCommit();
    state.updateState(state.nextAction());

    state.setNeedsCommit();
    state.setNeedsForcedCommit();
    state.updateState(state.nextAction());
    state.beginFrameComplete();

    EXPECT_EQ(SchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());
    state.updateState(state.nextAction());

    EXPECT_EQ(SchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW, state.commitState());

    state.didEnterVSync();
    EXPECT_EQ(SchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.setNeedsForcedRedraw(true);
    EXPECT_EQ(SchedulerStateMachine::ACTION_DRAW_FORCED, state.nextAction());
    state.updateState(state.nextAction());
    state.didDrawIfPossibleCompleted(true);
    state.didLeaveVSync();
}

}  // namespace
}  // namespace cc
