// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DELAY_BASED_TIME_SOURCE_H_
#define CC_DELAY_BASED_TIME_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "cc/cc_export.h"
#include "cc/time_source.h"

namespace cc {

class Thread;

// This timer implements a time source that achieves the specified interval
// in face of millisecond-precision delayed callbacks and random queueing delays.
class CC_EXPORT DelayBasedTimeSource : public TimeSource {
public:
    static scoped_refptr<DelayBasedTimeSource> create(base::TimeDelta interval, Thread* thread);

    virtual void setClient(TimeSourceClient* client) OVERRIDE;

    // TimeSource implementation
    virtual void setTimebaseAndInterval(base::TimeTicks timebase, base::TimeDelta interval) OVERRIDE;

    virtual void setActive(bool) OVERRIDE;
    virtual bool active() const OVERRIDE;

    // Get the last and next tick times. nextTimeTime() returns null when
    // inactive.
    virtual base::TimeTicks lastTickTime() OVERRIDE;
    virtual base::TimeTicks nextTickTime() OVERRIDE;


    // Virtual for testing.
    virtual base::TimeTicks now() const;

protected:
    DelayBasedTimeSource(base::TimeDelta interval, Thread* thread);
    virtual ~DelayBasedTimeSource();

    base::TimeTicks nextTickTarget(base::TimeTicks now);
    void postNextTickTask(base::TimeTicks now);
    void onTimerFired();

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

    TimeSourceClient* m_client;
    bool m_hasTickTarget;
    base::TimeTicks m_lastTickTime;

    // m_currentParameters should only be written by postNextTickTask.
    // m_nextParameters will take effect on the next call to postNextTickTask.
    // Maintaining a pending set of parameters allows nextTickTime() to always
    // reflect the actual time we expect onTimerFired to be called.
    Parameters m_currentParameters;
    Parameters m_nextParameters;

    State m_state;

    Thread* m_thread;
    base::WeakPtrFactory<DelayBasedTimeSource> m_weakFactory;
    DISALLOW_COPY_AND_ASSIGN(DelayBasedTimeSource);
};

}  // namespace cc

#endif  // CC_DELAY_BASED_TIME_SOURCE_H_
