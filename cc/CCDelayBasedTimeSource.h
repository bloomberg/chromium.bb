// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCDelayBasedTimeSource_h
#define CCDelayBasedTimeSource_h

#include "CCTimeSource.h"
#include "CCTimer.h"
#include <wtf/PassRefPtr.h>

namespace cc {

class CCThread;

// This timer implements a time source that achieves the specified interval
// in face of millisecond-precision delayed callbacks and random queueing delays.
class CCDelayBasedTimeSource : public CCTimeSource, CCTimerClient {
public:
    static PassRefPtr<CCDelayBasedTimeSource> create(base::TimeDelta interval, CCThread*);

    virtual ~CCDelayBasedTimeSource();

    virtual void setClient(CCTimeSourceClient* client) OVERRIDE;

    // CCTimeSource implementation
    virtual void setTimebaseAndInterval(base::TimeTicks timebase, base::TimeDelta interval) OVERRIDE;

    virtual void setActive(bool) OVERRIDE;
    virtual bool active() const OVERRIDE;

    // Get the last and next tick times.
    virtual base::TimeTicks lastTickTime() OVERRIDE;
    virtual base::TimeTicks nextTickTimeIfActivated() OVERRIDE;

    // CCTimerClient implementation.
    virtual void onTimerFired() OVERRIDE;

    // Virtual for testing.
    virtual base::TimeTicks now() const;

protected:
    CCDelayBasedTimeSource(base::TimeDelta interval, CCThread*);
    base::TimeTicks nextTickTarget(base::TimeTicks now);
    void postNextTickTask(base::TimeTicks now);

    enum State {
        STATE_INACTIVE,
        STATE_STARTING,
        STATE_ACTIVE,
    };

    struct Parameters {
        Parameters(base::TimeDelta interval, base::TimeTicks tickTarget)
            : interval(interval), tickTarget(tickTarget)
        { }
        base::TimeDelta interval;
        base::TimeTicks tickTarget;
    };

    CCTimeSourceClient* m_client;
    bool m_hasTickTarget;
    base::TimeTicks m_lastTickTime;

    // m_currentParameters should only be written by postNextTickTask.
    // m_nextParameters will take effect on the next call to postNextTickTask.
    // Maintaining a pending set of parameters allows nextTickTime() to always
    // reflect the actual time we expect onTimerFired to be called.
    Parameters m_currentParameters;
    Parameters m_nextParameters;

    State m_state;
    CCThread* m_thread;
    CCTimer m_timer;
};

}
#endif // CCDelayBasedTimeSource_h
