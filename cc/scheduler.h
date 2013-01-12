// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_H_
#define CC_SCHEDULER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cc/cc_export.h"
#include "cc/frame_rate_controller.h"
#include "cc/layer_tree_host.h"
#include "cc/scheduler_state_machine.h"

namespace cc {

class Thread;

struct ScheduledActionDrawAndSwapResult {
    ScheduledActionDrawAndSwapResult()
            : didDraw(false)
            , didSwap(false)
    {
    }
    ScheduledActionDrawAndSwapResult(bool didDraw, bool didSwap)
            : didDraw(didDraw)
            , didSwap(didSwap)
    {
    }
    bool didDraw;
    bool didSwap;
};

class SchedulerClient {
public:
    virtual void scheduledActionBeginFrame() = 0;
    virtual ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapIfPossible() = 0;
    virtual ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapForced() = 0;
    virtual void scheduledActionCommit() = 0;
    virtual void scheduledActionActivatePendingTreeIfNeeded() = 0;
    virtual void scheduledActionBeginContextRecreation() = 0;
    virtual void scheduledActionAcquireLayerTexturesForMainThread() = 0;
    virtual void didAnticipatedDrawTimeChange(base::TimeTicks) = 0;

protected:
    virtual ~SchedulerClient() { }
};

class CC_EXPORT Scheduler : FrameRateControllerClient {
public:
    static scoped_ptr<Scheduler> create(SchedulerClient* client, scoped_ptr<FrameRateController> frameRateController)
    {
        return make_scoped_ptr(new Scheduler(client, frameRateController.Pass()));
    }

    virtual ~Scheduler();

    void setCanBeginFrame(bool);

    void setVisible(bool);
    void setCanDraw(bool);
    void setHasPendingTree(bool);

    void setNeedsCommit();

    // Like setNeedsCommit(), but ensures a commit will definitely happen even if we are not visible.
    void setNeedsForcedCommit();

    void setNeedsRedraw();

    void setMainThreadNeedsLayerTextures();

    // Like setNeedsRedraw(), but ensures the draw will definitely happen even if we are not visible.
    void setNeedsForcedRedraw();

    void beginFrameComplete();
    void beginFrameAborted();

    void setMaxFramesPending(int);
    void setSwapBuffersCompleteSupported(bool);
    void didSwapBuffersComplete();

    void didLoseOutputSurface();
    void didRecreateOutputSurface();

    bool commitPending() const { return m_stateMachine.commitPending(); }
    bool redrawPending() const { return m_stateMachine.redrawPending(); }

    void setTimebaseAndInterval(base::TimeTicks timebase, base::TimeDelta interval);

    base::TimeTicks anticipatedDrawTime();

    // FrameRateControllerClient implementation
    virtual void vsyncTick(bool throttled) OVERRIDE;

private:
    Scheduler(SchedulerClient*, scoped_ptr<FrameRateController>);

    void processScheduledActions();

    SchedulerClient* m_client;
    scoped_ptr<FrameRateController> m_frameRateController;
    SchedulerStateMachine m_stateMachine;
    bool m_insideProcessScheduledActions;

    DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace cc

#endif  // CC_SCHEDULER_H_
