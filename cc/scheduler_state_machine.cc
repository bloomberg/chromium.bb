// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler_state_machine.h"

#include "base/logging.h"
#include "base/stringprintf.h"

namespace cc {

SchedulerStateMachine::SchedulerStateMachine(const SchedulerSettings& settings)
    : m_settings(settings)
    , m_commitState(COMMIT_STATE_IDLE)
    , m_currentFrameNumber(0)
    , m_lastFrameNumberWhereDrawWasCalled(-1)
    , m_lastFrameNumberWhereTreeActivationAttempted(-1)
    , m_lastFrameNumberWhereCheckForCompletedTexturesCalled(-1)
    , m_consecutiveFailedDraws(0)
    , m_maximumNumberOfFailedDrawsBeforeDrawIsForced(3)
    , m_needsRedraw(false)
    , m_swapUsedIncompleteTexture(false)
    , m_needsForcedRedraw(false)
    , m_needsForcedRedrawAfterNextCommit(false)
    , m_needsCommit(false)
    , m_needsForcedCommit(false)
    , m_expectImmediateBeginFrame(false)
    , m_mainThreadNeedsLayerTextures(false)
    , m_insideVSync(false)
    , m_visible(false)
    , m_canBeginFrame(false)
    , m_canDraw(false)
    , m_hasPendingTree(false)
    , m_drawIfPossibleFailed(false)
    , m_textureState(LAYER_TEXTURE_STATE_UNLOCKED)
    , m_outputSurfaceState(OUTPUT_SURFACE_ACTIVE)
{
}

std::string SchedulerStateMachine::toString()
{
    std::string str;
    base::StringAppendF(&str, "m_settings.implSidePainting = %d; ", m_settings.implSidePainting);
    base::StringAppendF(&str, "m_commitState = %d; ", m_commitState);
    base::StringAppendF(&str, "m_currentFrameNumber = %d; ", m_currentFrameNumber);
    base::StringAppendF(&str, "m_lastFrameNumberWhereDrawWasCalled = %d; ", m_lastFrameNumberWhereDrawWasCalled);
    base::StringAppendF(&str, "m_lastFrameNumberWhereTreeActivationAttempted = %d; ", m_lastFrameNumberWhereTreeActivationAttempted);
    base::StringAppendF(&str, "m_lastFrameNumberWhereCheckForCompletedTexturesCalled = %d; ", m_lastFrameNumberWhereCheckForCompletedTexturesCalled);
    base::StringAppendF(&str, "m_consecutiveFailedDraws = %d; ", m_consecutiveFailedDraws);
    base::StringAppendF(&str, "m_maximumNumberOfFailedDrawsBeforeDrawIsForced = %d; ", m_maximumNumberOfFailedDrawsBeforeDrawIsForced);
    base::StringAppendF(&str, "m_needsRedraw = %d; ", m_needsRedraw);
    base::StringAppendF(&str, "m_swapUsedIncompleteTexture = %d; ", m_swapUsedIncompleteTexture);
    base::StringAppendF(&str, "m_needsForcedRedraw = %d; ", m_needsForcedRedraw);
    base::StringAppendF(&str, "m_needsForcedRedrawAfterNextCommit = %d; ", m_needsForcedRedrawAfterNextCommit);
    base::StringAppendF(&str, "m_needsCommit = %d; ", m_needsCommit);
    base::StringAppendF(&str, "m_needsForcedCommit = %d; ", m_needsForcedCommit);
    base::StringAppendF(&str, "m_expectImmediateBeginFrame = %d; ", m_expectImmediateBeginFrame);
    base::StringAppendF(&str, "m_mainThreadNeedsLayerTextures = %d; ", m_mainThreadNeedsLayerTextures);
    base::StringAppendF(&str, "m_insideVSync = %d; ", m_insideVSync);
    base::StringAppendF(&str, "m_visible = %d; ", m_visible);
    base::StringAppendF(&str, "m_canBeginFrame = %d; ", m_canBeginFrame);
    base::StringAppendF(&str, "m_canDraw = %d; ", m_canDraw);
    base::StringAppendF(&str, "m_drawIfPossibleFailed = %d; ", m_drawIfPossibleFailed);
    base::StringAppendF(&str, "m_hasPendingTree = %d; ", m_hasPendingTree);
    base::StringAppendF(&str, "m_textureState = %d; ", m_textureState);
    base::StringAppendF(&str, "m_outputSurfaceState = %d; ", m_outputSurfaceState);
    return str;
}

bool SchedulerStateMachine::hasDrawnThisFrame() const
{
    return m_currentFrameNumber == m_lastFrameNumberWhereDrawWasCalled;
}

bool SchedulerStateMachine::hasAttemptedTreeActivationThisFrame() const
{
    return m_currentFrameNumber == m_lastFrameNumberWhereTreeActivationAttempted;
}

bool SchedulerStateMachine::hasCheckedForCompletedTexturesThisFrame() const
{
    return m_currentFrameNumber ==
           m_lastFrameNumberWhereCheckForCompletedTexturesCalled;
}

bool SchedulerStateMachine::drawSuspendedUntilCommit() const
{
    if (!m_canDraw)
        return true;
    if (!m_visible)
        return true;
    if (m_textureState == LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD)
        return true;
    return false;
}

bool SchedulerStateMachine::scheduledToDraw() const
{
    if (!m_needsRedraw)
        return false;
    if (drawSuspendedUntilCommit())
        return false;
    return true;
}

bool SchedulerStateMachine::shouldDraw() const
{
    if (m_needsForcedRedraw)
        return true;

    if (!scheduledToDraw())
        return false;
    if (!m_insideVSync)
        return false;
    if (hasDrawnThisFrame())
        return false;
    if (m_outputSurfaceState != OUTPUT_SURFACE_ACTIVE)
        return false;
    return true;
}

bool SchedulerStateMachine::shouldAttemptTreeActivation() const
{
    return m_hasPendingTree && m_insideVSync && !hasAttemptedTreeActivationThisFrame();
}

bool SchedulerStateMachine::shouldCheckForCompletedTextures() const
{
    if (!m_settings.implSidePainting)
        return false;
    if (hasCheckedForCompletedTexturesThisFrame())
        return false;

    return shouldAttemptTreeActivation() ||
           shouldDraw() ||
           m_swapUsedIncompleteTexture;
}

bool SchedulerStateMachine::shouldAcquireLayerTexturesForMainThread() const
{
    if (!m_mainThreadNeedsLayerTextures)
        return false;
    if (m_textureState == LAYER_TEXTURE_STATE_UNLOCKED)
        return true;
    DCHECK(m_textureState == LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD);
    // Transfer the lock from impl thread to main thread immediately if the
    // impl thread is not even scheduled to draw. Guards against deadlocking.
    if (!scheduledToDraw())
        return true;
    if (!vsyncCallbackNeeded())
        return true;
    return false;
}

SchedulerStateMachine::Action SchedulerStateMachine::nextAction() const
{
    if (shouldAcquireLayerTexturesForMainThread())
        return ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD;

    switch (m_commitState) {
    case COMMIT_STATE_IDLE:
        if (m_outputSurfaceState != OUTPUT_SURFACE_ACTIVE && m_needsForcedRedraw)
            return ACTION_DRAW_FORCED;
        if (m_outputSurfaceState != OUTPUT_SURFACE_ACTIVE && m_needsForcedCommit)
            // TODO(enne): Should probably drop the active tree on force commit
            return m_hasPendingTree ? ACTION_NONE : ACTION_BEGIN_FRAME;
        if (m_outputSurfaceState == OUTPUT_SURFACE_LOST)
            return ACTION_BEGIN_OUTPUT_SURFACE_RECREATION;
        if (m_outputSurfaceState == OUTPUT_SURFACE_RECREATING)
            return ACTION_NONE;
        if (shouldCheckForCompletedTextures())
            return ACTION_CHECK_FOR_NEW_TEXTURES;
        if (shouldAttemptTreeActivation())
            return ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED;
        if (shouldDraw())
            return m_needsForcedRedraw ? ACTION_DRAW_FORCED : ACTION_DRAW_IF_POSSIBLE;
        if (m_needsCommit && ((m_visible && m_canBeginFrame) || m_needsForcedCommit))
            // TODO(enne): Should probably drop the active tree on force commit
            return m_hasPendingTree ? ACTION_NONE : ACTION_BEGIN_FRAME;
        return ACTION_NONE;

    case COMMIT_STATE_FRAME_IN_PROGRESS:
        if (shouldCheckForCompletedTextures())
            return ACTION_CHECK_FOR_NEW_TEXTURES;
        if (shouldAttemptTreeActivation())
            return ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED;
        if (shouldDraw())
            return m_needsForcedRedraw ? ACTION_DRAW_FORCED : ACTION_DRAW_IF_POSSIBLE;
        return ACTION_NONE;

    case COMMIT_STATE_READY_TO_COMMIT:
        return ACTION_COMMIT;

    case COMMIT_STATE_WAITING_FOR_FIRST_DRAW: {
        if (shouldCheckForCompletedTextures())
            return ACTION_CHECK_FOR_NEW_TEXTURES;
        if (shouldAttemptTreeActivation())
            return ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED;
        if (shouldDraw() || m_outputSurfaceState == OUTPUT_SURFACE_LOST)
            return m_needsForcedRedraw ? ACTION_DRAW_FORCED : ACTION_DRAW_IF_POSSIBLE;
        // COMMIT_STATE_WAITING_FOR_FIRST_DRAW wants to enforce a draw. If m_canDraw is false
        // or textures are not available, proceed to the next step (similar as in COMMIT_STATE_IDLE).
        bool canCommit = m_visible || m_needsForcedCommit;
        if (m_needsCommit && canCommit && drawSuspendedUntilCommit())
            return m_hasPendingTree ? ACTION_NONE : ACTION_BEGIN_FRAME;
        return ACTION_NONE;
    }

    case COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW:
        if (shouldCheckForCompletedTextures())
            return ACTION_CHECK_FOR_NEW_TEXTURES;
        if (shouldAttemptTreeActivation())
            return ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED;
        if (m_needsForcedRedraw)
            return ACTION_DRAW_FORCED;
        return ACTION_NONE;
    }
    NOTREACHED();
    return ACTION_NONE;
}

void SchedulerStateMachine::updateState(Action action)
{
    switch (action) {
    case ACTION_NONE:
        return;

    case ACTION_CHECK_FOR_NEW_TEXTURES:
        m_lastFrameNumberWhereCheckForCompletedTexturesCalled = m_currentFrameNumber;
        return;

    case ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED:
        m_lastFrameNumberWhereTreeActivationAttempted = m_currentFrameNumber;
        return;

    case ACTION_BEGIN_FRAME:
        DCHECK(!m_hasPendingTree);
        DCHECK(m_visible || m_needsForcedCommit);
        m_commitState = COMMIT_STATE_FRAME_IN_PROGRESS;
        m_needsCommit = false;
        m_needsForcedCommit = false;
        return;

    case ACTION_COMMIT:
        if (m_expectImmediateBeginFrame)
            m_commitState = COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW;
        else
            m_commitState = COMMIT_STATE_WAITING_FOR_FIRST_DRAW;
        // When impl-side painting, we draw on activation instead of on commit.
        if (!m_settings.implSidePainting)
            m_needsRedraw = true;
        if (m_drawIfPossibleFailed)
            m_lastFrameNumberWhereDrawWasCalled = -1;

        if (m_needsForcedRedrawAfterNextCommit) {
            m_needsForcedRedrawAfterNextCommit = false;
            m_needsForcedRedraw = true;
        }

        m_textureState = LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD;
        return;

    case ACTION_DRAW_FORCED:
    case ACTION_DRAW_IF_POSSIBLE:
        m_needsRedraw = false;
        m_needsForcedRedraw = false;
        m_drawIfPossibleFailed = false;
        if (m_insideVSync)
            m_lastFrameNumberWhereDrawWasCalled = m_currentFrameNumber;
        if (m_commitState == COMMIT_STATE_WAITING_FOR_FIRST_FORCED_DRAW) {
            DCHECK(m_expectImmediateBeginFrame);
            m_commitState = COMMIT_STATE_FRAME_IN_PROGRESS;
            m_expectImmediateBeginFrame = false;
        } else if (m_commitState == COMMIT_STATE_WAITING_FOR_FIRST_DRAW)
            m_commitState = COMMIT_STATE_IDLE;
        if (m_textureState == LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD)
            m_textureState = LAYER_TEXTURE_STATE_UNLOCKED;
        return;

    case ACTION_BEGIN_OUTPUT_SURFACE_RECREATION:
        DCHECK(m_commitState == COMMIT_STATE_IDLE);
        DCHECK(m_outputSurfaceState == OUTPUT_SURFACE_LOST);
        m_outputSurfaceState = OUTPUT_SURFACE_RECREATING;
        return;

    case ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD:
        m_textureState = LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD;
        m_mainThreadNeedsLayerTextures = false;
        if (m_commitState != COMMIT_STATE_FRAME_IN_PROGRESS)
            m_needsCommit = true;
        return;
    }
}

void SchedulerStateMachine::setMainThreadNeedsLayerTextures()
{
    DCHECK(!m_mainThreadNeedsLayerTextures);
    DCHECK(m_textureState != LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD);
    m_mainThreadNeedsLayerTextures = true;
}

bool SchedulerStateMachine::vsyncCallbackNeeded() const
{
    // If we have a pending tree, need to keep getting notifications until
    // the tree is ready to be swapped.
    if (m_hasPendingTree)
        return true;

    // If we can't draw, don't tick until we are notified that we can draw again.
    if (!m_canDraw)
        return false;

    if (m_needsForcedRedraw)
        return true;

    if (m_visible && m_swapUsedIncompleteTexture)
        return true;

    return m_needsRedraw && m_visible && m_outputSurfaceState == OUTPUT_SURFACE_ACTIVE;
}

void SchedulerStateMachine::didEnterVSync()
{
    m_insideVSync = true;
}

void SchedulerStateMachine::didLeaveVSync()
{
    m_currentFrameNumber++;
    m_insideVSync = false;
}

void SchedulerStateMachine::setVisible(bool visible)
{
    m_visible = visible;
}

void SchedulerStateMachine::setNeedsRedraw()
{
    m_needsRedraw = true;
}

void SchedulerStateMachine::didSwapUseIncompleteTexture()
{
    m_swapUsedIncompleteTexture = true;
}

void SchedulerStateMachine::setNeedsForcedRedraw()
{
    m_needsForcedRedraw = true;
}

void SchedulerStateMachine::didDrawIfPossibleCompleted(bool success)
{
    m_drawIfPossibleFailed = !success;
    if (m_drawIfPossibleFailed) {
        m_needsRedraw = true;
        m_needsCommit = true;
        m_consecutiveFailedDraws++;
        if (m_consecutiveFailedDraws >= m_maximumNumberOfFailedDrawsBeforeDrawIsForced) {
            m_consecutiveFailedDraws = 0;
            // We need to force a draw, but it doesn't make sense to do this until
            // we've committed and have new textures.
            m_needsForcedRedrawAfterNextCommit = true;
        }
    } else
      m_consecutiveFailedDraws = 0;
}

void SchedulerStateMachine::setNeedsCommit()
{
    m_needsCommit = true;
}

void SchedulerStateMachine::setNeedsForcedCommit()
{
    m_needsForcedCommit = true;
    m_expectImmediateBeginFrame = true;
}

void SchedulerStateMachine::beginFrameComplete()
{
    DCHECK(m_commitState == COMMIT_STATE_FRAME_IN_PROGRESS ||
           (m_expectImmediateBeginFrame && m_commitState != COMMIT_STATE_IDLE)) << toString();
    m_commitState = COMMIT_STATE_READY_TO_COMMIT;
}

void SchedulerStateMachine::beginFrameAborted()
{
    DCHECK(m_commitState == COMMIT_STATE_FRAME_IN_PROGRESS);
    if (m_expectImmediateBeginFrame)
        m_expectImmediateBeginFrame = false;
    else {
        m_commitState = COMMIT_STATE_IDLE;
        setNeedsCommit();
    }
}

void SchedulerStateMachine::didLoseOutputSurface()
{
    if (m_outputSurfaceState == OUTPUT_SURFACE_LOST || m_outputSurfaceState == OUTPUT_SURFACE_RECREATING)
        return;
    m_outputSurfaceState = OUTPUT_SURFACE_LOST;
}

void SchedulerStateMachine::setHasPendingTree(bool hasPendingTree)
{
    m_hasPendingTree = hasPendingTree;
}

void SchedulerStateMachine::setCanDraw(bool can)
{
    m_canDraw = can;
}

void SchedulerStateMachine::didRecreateOutputSurface()
{
    DCHECK(m_outputSurfaceState == OUTPUT_SURFACE_RECREATING);
    m_outputSurfaceState = OUTPUT_SURFACE_ACTIVE;
    setNeedsCommit();
}

void SchedulerStateMachine::setMaximumNumberOfFailedDrawsBeforeDrawIsForced(int numDraws)
{
    m_maximumNumberOfFailedDrawsBeforeDrawIsForced = numDraws;
}

}  // namespace cc
