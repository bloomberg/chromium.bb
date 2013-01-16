// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler.h"

#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"

namespace cc {

Scheduler::Scheduler(SchedulerClient* client, scoped_ptr<FrameRateController> frameRateController)
    : m_client(client)
    , m_frameRateController(frameRateController.Pass())
    , m_insideProcessScheduledActions(false)
{
    DCHECK(m_client);
    m_frameRateController->setClient(this);
    DCHECK(!m_stateMachine.vsyncCallbackNeeded());
}

Scheduler::~Scheduler()
{
    m_frameRateController->setActive(false);
}

void Scheduler::setCanBeginFrame(bool can)
{
    m_stateMachine.setCanBeginFrame(can);
    processScheduledActions();
}

void Scheduler::setVisible(bool visible)
{
    m_stateMachine.setVisible(visible);
    processScheduledActions();
}

void Scheduler::setCanDraw(bool canDraw)
{
    m_stateMachine.setCanDraw(canDraw);
    processScheduledActions();
}

void Scheduler::setHasPendingTree(bool hasPendingTree)
{
    m_stateMachine.setHasPendingTree(hasPendingTree);
    processScheduledActions();
}

void Scheduler::setNeedsCommit()
{
    m_stateMachine.setNeedsCommit();
    processScheduledActions();
}

void Scheduler::setNeedsForcedCommit()
{
    m_stateMachine.setNeedsCommit();
    m_stateMachine.setNeedsForcedCommit();
    processScheduledActions();
}

void Scheduler::setNeedsRedraw()
{
    m_stateMachine.setNeedsRedraw();
    processScheduledActions();
}

void Scheduler::setNeedsForcedRedraw()
{
    m_stateMachine.setNeedsForcedRedraw();
    processScheduledActions();
}

void Scheduler::setMainThreadNeedsLayerTextures()
{
    m_stateMachine.setMainThreadNeedsLayerTextures();
    processScheduledActions();
}

void Scheduler::beginFrameComplete()
{
    TRACE_EVENT0("cc", "Scheduler::beginFrameComplete");
    m_stateMachine.beginFrameComplete();
    processScheduledActions();
}

void Scheduler::beginFrameAborted()
{
    TRACE_EVENT0("cc", "Scheduler::beginFrameAborted");
    m_stateMachine.beginFrameAborted();
    processScheduledActions();
}

void Scheduler::setMaxFramesPending(int maxFramesPending)
{
    m_frameRateController->setMaxFramesPending(maxFramesPending);
}

int Scheduler::maxFramesPending() const
{
    return m_frameRateController->maxFramesPending();
}

void Scheduler::setSwapBuffersCompleteSupported(bool supported)
{
    m_frameRateController->setSwapBuffersCompleteSupported(supported);
}

void Scheduler::didSwapBuffersComplete()
{
    TRACE_EVENT0("cc", "Scheduler::didSwapBuffersComplete");
    m_frameRateController->didFinishFrame();
}

void Scheduler::didLoseOutputSurface()
{
    TRACE_EVENT0("cc", "Scheduler::didLoseOutputSurface");
    m_frameRateController->didAbortAllPendingFrames();
    m_stateMachine.didLoseOutputSurface();
    processScheduledActions();
}

void Scheduler::didRecreateOutputSurface()
{
    TRACE_EVENT0("cc", "Scheduler::didRecreateOutputSurface");
    m_stateMachine.didRecreateOutputSurface();
    processScheduledActions();
}

void Scheduler::setTimebaseAndInterval(base::TimeTicks timebase, base::TimeDelta interval)
{
    m_frameRateController->setTimebaseAndInterval(timebase, interval);
}

base::TimeTicks Scheduler::anticipatedDrawTime()
{
    return m_frameRateController->nextTickTime();
}

void Scheduler::vsyncTick(bool throttled)
{
    TRACE_EVENT1("cc", "Scheduler::vsyncTick", "throttled", throttled);
    if (!throttled)
        m_stateMachine.didEnterVSync();
    processScheduledActions();
    if (!throttled)
        m_stateMachine.didLeaveVSync();
}

void Scheduler::processScheduledActions()
{
    // We do not allow processScheduledActions to be recursive.
    // The top-level call will iteratively execute the next action for us anyway.
    if (m_insideProcessScheduledActions)
        return;

    base::AutoReset<bool> markInside(&m_insideProcessScheduledActions, true);

    SchedulerStateMachine::Action action = m_stateMachine.nextAction();
    while (action != SchedulerStateMachine::ACTION_NONE) {
        m_stateMachine.updateState(action);
        TRACE_EVENT1("cc", "Scheduler::processScheduledActions()", "action", action);

        switch (action) {
        case SchedulerStateMachine::ACTION_NONE:
            break;
        case SchedulerStateMachine::ACTION_BEGIN_FRAME:
            m_client->scheduledActionBeginFrame();
            break;
        case SchedulerStateMachine::ACTION_COMMIT:
            m_client->scheduledActionCommit();
            break;
        case SchedulerStateMachine::ACTION_ACTIVATE_PENDING_TREE_IF_NEEDED:
            m_client->scheduledActionActivatePendingTreeIfNeeded();
            break;
        case SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE: {
            ScheduledActionDrawAndSwapResult result = m_client->scheduledActionDrawAndSwapIfPossible();
            m_stateMachine.didDrawIfPossibleCompleted(result.didDraw);
            if (result.didSwap)
                m_frameRateController->didBeginFrame();
            break;
        }
        case SchedulerStateMachine::ACTION_DRAW_FORCED: {
            ScheduledActionDrawAndSwapResult result = m_client->scheduledActionDrawAndSwapForced();
            if (result.didSwap)
                m_frameRateController->didBeginFrame();
            break;
        } case SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_RECREATION:
            m_client->scheduledActionBeginContextRecreation();
            break;
        case SchedulerStateMachine::ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD:
            m_client->scheduledActionAcquireLayerTexturesForMainThread();
            break;
        }
        action = m_stateMachine.nextAction();
    }

    // Activate or deactivate the frame rate controller.
    m_frameRateController->setActive(m_stateMachine.vsyncCallbackNeeded());
    m_client->didAnticipatedDrawTimeChange(m_frameRateController->nextTickTime());
}

}  // namespace cc
