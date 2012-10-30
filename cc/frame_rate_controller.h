// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCFrameRateController_h
#define CCFrameRateController_h

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"

namespace cc {

class Thread;
class TimeSource;

class FrameRateControllerClient {
public:
    // Throttled is true when we have a maximum number of frames pending.
    virtual void vsyncTick(bool throttled) = 0;

protected:
    virtual ~FrameRateControllerClient() {}
};

class FrameRateControllerTimeSourceAdapter;

class FrameRateController {
public:
    explicit FrameRateController(scoped_refptr<TimeSource>);
    // Alternate form of FrameRateController with unthrottled frame-rate.
    explicit FrameRateController(Thread*);
    virtual ~FrameRateController();

    void setClient(FrameRateControllerClient* client) { m_client = client; }

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

    // This returns null for unthrottled frame-rate.
    base::TimeTicks nextTickTime();

    void setTimebaseAndInterval(base::TimeTicks timebase, base::TimeDelta interval);
    void setSwapBuffersCompleteSupported(bool);

protected:
    friend class FrameRateControllerTimeSourceAdapter;
    void onTimerTick();

    void postManualTick();
    void manualTick();

    FrameRateControllerClient* m_client;
    int m_numFramesPending;
    int m_maxFramesPending;
    scoped_refptr<TimeSource> m_timeSource;
    scoped_ptr<FrameRateControllerTimeSourceAdapter> m_timeSourceClientAdapter;
    bool m_active;
    bool m_swapBuffersCompleteSupported;

    // Members for unthrottled frame-rate.
    bool m_isTimeSourceThrottling;
    base::WeakPtrFactory<FrameRateController> m_weakFactory;
    Thread* m_thread;

    DISALLOW_COPY_AND_ASSIGN(FrameRateController);
};

}  // namespace cc

#endif // CCFrameRateController_h
