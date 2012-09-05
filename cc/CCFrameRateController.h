// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCFrameRateController_h
#define CCFrameRateController_h

#include "CCTimer.h"

#include <wtf/Deque.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class CCThread;
class CCTimeSource;

class CCFrameRateControllerClient {
public:
    virtual void vsyncTick() = 0;

protected:
    virtual ~CCFrameRateControllerClient() { }
};

class CCFrameRateControllerTimeSourceAdapter;

class CCFrameRateController : public CCTimerClient {
public:
    explicit CCFrameRateController(PassRefPtr<CCTimeSource>);
    // Alternate form of CCFrameRateController with unthrottled frame-rate.
    explicit CCFrameRateController(CCThread*);
    virtual ~CCFrameRateController();

    void setClient(CCFrameRateControllerClient* client) { m_client = client; }

    void setActive(bool);

    // Use the following methods to adjust target frame rate.
    //
    // Multiple frames can be in-progress, but for every didBeginFrame, a
    // didFinishFrame should be posted.
    //
    // If the rendering pipeline crashes, call didAbortAllPendingFrames.
    void didBeginFrame();
    void didFinishFrame();
    void didAbortAllPendingFrames();
    void setMaxFramesPending(int); // 0 for unlimited.
    double nextTickTimeIfActivated();

    void setTimebaseAndInterval(double timebase, double intervalSeconds);
    void setSwapBuffersCompleteSupported(bool);

protected:
    friend class CCFrameRateControllerTimeSourceAdapter;
    void onTimerTick();

    void postManualTick();

    // CCTimerClient implementation (used for unthrottled frame-rate).
    virtual void onTimerFired() OVERRIDE;

    CCFrameRateControllerClient* m_client;
    int m_numFramesPending;
    int m_maxFramesPending;
    RefPtr<CCTimeSource> m_timeSource;
    OwnPtr<CCFrameRateControllerTimeSourceAdapter> m_timeSourceClientAdapter;
    bool m_active;
    bool m_swapBuffersCompleteSupported;

    // Members for unthrottled frame-rate.
    bool m_isTimeSourceThrottling;
    OwnPtr<CCTimer> m_manualTicker;
};

}
#endif // CCFrameRateController_h
