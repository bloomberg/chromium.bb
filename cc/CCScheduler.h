// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCScheduler_h
#define CCScheduler_h

#include "CCFrameRateController.h"
#include "CCSchedulerStateMachine.h"

#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class CCThread;

struct CCScheduledActionDrawAndSwapResult {
    CCScheduledActionDrawAndSwapResult()
            : didDraw(false)
            , didSwap(false)
    {
    }
    CCScheduledActionDrawAndSwapResult(bool didDraw, bool didSwap)
            : didDraw(didDraw)
            , didSwap(didSwap)
    {
    }
    bool didDraw;
    bool didSwap;
};

class CCSchedulerClient {
public:
    virtual bool hasMoreResourceUpdates() const = 0;

    virtual void scheduledActionBeginFrame() = 0;
    virtual CCScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapIfPossible() = 0;
    virtual CCScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapForced() = 0;
    virtual void scheduledActionUpdateMoreResources(double monotonicTimeLimit) = 0;
    virtual void scheduledActionCommit() = 0;
    virtual void scheduledActionBeginContextRecreation() = 0;
    virtual void scheduledActionAcquireLayerTexturesForMainThread() = 0;

protected:
    virtual ~CCSchedulerClient() { }
};

class CCScheduler : CCFrameRateControllerClient {
    WTF_MAKE_NONCOPYABLE(CCScheduler);
public:
    static PassOwnPtr<CCScheduler> create(CCSchedulerClient* client, PassOwnPtr<CCFrameRateController> frameRateController)
    {
        return adoptPtr(new CCScheduler(client, frameRateController));
    }

    virtual ~CCScheduler();

    void setCanBeginFrame(bool);

    void setVisible(bool);
    void setCanDraw(bool);

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

    void didLoseContext();
    void didRecreateContext();

    bool commitPending() const { return m_stateMachine.commitPending(); }
    bool redrawPending() const { return m_stateMachine.redrawPending(); }

    void setTimebaseAndInterval(double timebase, double intervalSeconds);

    // CCFrameRateControllerClient implementation
    virtual void vsyncTick() OVERRIDE;

private:
    CCScheduler(CCSchedulerClient*, PassOwnPtr<CCFrameRateController>);

    void processScheduledActions();

    CCSchedulerClient* m_client;
    OwnPtr<CCFrameRateController> m_frameRateController;
    CCSchedulerStateMachine m_stateMachine;
    bool m_updateMoreResourcesPending;
};

}

#endif // CCScheduler_h
