// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/delay_based_time_source.h"

#include <algorithm>
#include <cmath>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "cc/thread.h"

namespace cc {

namespace {

// doubleTickThreshold prevents ticks from running within the specified fraction of an interval.
// This helps account for jitter in the timebase as well as quick timer reactivation.
const double doubleTickThreshold = 0.25;

// intervalChangeThreshold is the fraction of the interval that will trigger an immediate interval change.
// phaseChangeThreshold is the fraction of the interval that will trigger an immediate phase change.
// If the changes are within the thresholds, the change will take place on the next tick.
// If either change is outside the thresholds, the next tick will be canceled and reissued immediately.
const double intervalChangeThreshold = 0.25;
const double phaseChangeThreshold = 0.25;

}  // namespace

scoped_refptr<DelayBasedTimeSource> DelayBasedTimeSource::create(base::TimeDelta interval, Thread* thread)
{
    return make_scoped_refptr(new DelayBasedTimeSource(interval, thread));
}

DelayBasedTimeSource::DelayBasedTimeSource(base::TimeDelta interval, Thread* thread)
    : m_client(0)
    , m_hasTickTarget(false)
    , m_currentParameters(interval, base::TimeTicks())
    , m_nextParameters(interval, base::TimeTicks())
    , m_state(STATE_INACTIVE)
    , m_thread(thread)
    , m_weakFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this))
{
}

DelayBasedTimeSource::~DelayBasedTimeSource()
{
}

void DelayBasedTimeSource::setActive(bool active)
{
    TRACE_EVENT1("cc", "DelayBasedTimeSource::setActive", "active", active);
    if (!active) {
        m_state = STATE_INACTIVE;
        m_weakFactory.InvalidateWeakPtrs();
        return;
    }

    if (m_state == STATE_STARTING || m_state == STATE_ACTIVE)
        return;

    if (!m_hasTickTarget) {
        // Becoming active the first time is deferred: we post a 0-delay task. When
        // it runs, we use that to establish the timebase, become truly active, and
        // fire the first tick.
        m_state = STATE_STARTING;
        m_thread->postTask(base::Bind(&DelayBasedTimeSource::onTimerFired, m_weakFactory.GetWeakPtr()));
        return;
    }

    m_state = STATE_ACTIVE;

    postNextTickTask(now());
}

bool DelayBasedTimeSource::active() const
{
    return m_state != STATE_INACTIVE;
}

base::TimeTicks DelayBasedTimeSource::lastTickTime()
{
    return m_lastTickTime;
}

base::TimeTicks DelayBasedTimeSource::nextTickTime()
{
    return active() ? m_currentParameters.tickTarget : base::TimeTicks();
}

void DelayBasedTimeSource::onTimerFired()
{
    DCHECK(m_state != STATE_INACTIVE);

    base::TimeTicks now = this->now();
    m_lastTickTime = now;

    if (m_state == STATE_STARTING) {
        setTimebaseAndInterval(now, m_currentParameters.interval);
        m_state = STATE_ACTIVE;
    }

    postNextTickTask(now);

    // Fire the tick
    if (m_client)
        m_client->onTimerTick();
}

void DelayBasedTimeSource::setClient(TimeSourceClient* client)
{
    m_client = client;
}

void DelayBasedTimeSource::setTimebaseAndInterval(base::TimeTicks timebase, base::TimeDelta interval)
{
    m_nextParameters.interval = interval;
    m_nextParameters.tickTarget = timebase;
    m_hasTickTarget = true;

    if (m_state != STATE_ACTIVE) {
        // If we aren't active, there's no need to reset the timer.
        return;
    }

    // If the change in interval is larger than the change threshold,
    // request an immediate reset.
    double intervalDelta = std::abs((interval - m_currentParameters.interval).InSecondsF());
    double intervalChange = intervalDelta / interval.InSecondsF();
    if (intervalChange > intervalChangeThreshold) {
        setActive(false);
        setActive(true);
        return;
    }

    // If the change in phase is greater than the change threshold in either
    // direction, request an immediate reset. This logic might result in a false
    // negative if there is a simultaneous small change in the interval and the
    // fmod just happens to return something near zero. Assuming the timebase
    // is very recent though, which it should be, we'll still be ok because the
    // old clock and new clock just happen to line up.
    double targetDelta = std::abs((timebase - m_currentParameters.tickTarget).InSecondsF());
    double phaseChange = fmod(targetDelta, interval.InSecondsF()) / interval.InSecondsF();
    if (phaseChange > phaseChangeThreshold && phaseChange < (1.0 - phaseChangeThreshold)) {
        setActive(false);
        setActive(true);
        return;
    }
}

base::TimeTicks DelayBasedTimeSource::now() const
{
    return base::TimeTicks::Now();
}

// This code tries to achieve an average tick rate as close to m_interval as possible.
// To do this, it has to deal with a few basic issues:
//   1. postDelayedTask can delay only at a millisecond granularity. So, 16.666 has to
//      posted as 16 or 17.
//   2. A delayed task may come back a bit late (a few ms), or really late (frames later)
//
// The basic idea with this scheduler here is to keep track of where we *want* to run in
// m_tickTarget. We update this with the exact interval.
//
// Then, when we post our task, we take the floor of (m_tickTarget and now()). If we
// started at now=0, and 60FPs (all times in milliseconds):
//      now=0    target=16.667   postDelayedTask(16)
//
// When our callback runs, we figure out how far off we were from that goal. Because of the flooring
// operation, and assuming our timer runs exactly when it should, this yields:
//      now=16   target=16.667
//
// Since we can't post a 0.667 ms task to get to now=16, we just treat this as a tick. Then,
// we update target to be 33.333. We now post another task based on the difference between our target
// and now:
//      now=16   tickTarget=16.667  newTarget=33.333   --> postDelayedTask(floor(33.333 - 16)) --> postDelayedTask(17)
//
// Over time, with no late tasks, this leads to us posting tasks like this:
//      now=0    tickTarget=0       newTarget=16.667   --> tick(), postDelayedTask(16)
//      now=16   tickTarget=16.667  newTarget=33.333   --> tick(), postDelayedTask(17)
//      now=33   tickTarget=33.333  newTarget=50.000   --> tick(), postDelayedTask(17)
//      now=50   tickTarget=50.000  newTarget=66.667   --> tick(), postDelayedTask(16)
//
// We treat delays in tasks differently depending on the amount of delay we encounter. Suppose we
// posted a task with a target=16.667:
//   Case 1: late but not unrecoverably-so
//      now=18 tickTarget=16.667
//
//   Case 2: so late we obviously missed the tick
//      now=25.0 tickTarget=16.667
//
// We treat the first case as a tick anyway, and assume the delay was
// unusual. Thus, we compute the newTarget based on the old timebase:
//      now=18   tickTarget=16.667  newTarget=33.333   --> tick(), postDelayedTask(floor(33.333-18)) --> postDelayedTask(15)
// This brings us back to 18+15 = 33, which was where we would have been if the task hadn't been late.
//
// For the really late delay, we we move to the next logical tick. The timebase is not reset.
//      now=37   tickTarget=16.667  newTarget=50.000  --> tick(), postDelayedTask(floor(50.000-37)) --> postDelayedTask(13)
base::TimeTicks DelayBasedTimeSource::nextTickTarget(base::TimeTicks now)
{
    base::TimeDelta newInterval = m_nextParameters.interval;
    int intervalsElapsed = static_cast<int>(floor((now - m_nextParameters.tickTarget).InSecondsF() / newInterval.InSecondsF()));
    base::TimeTicks lastEffectiveTick = m_nextParameters.tickTarget + newInterval * intervalsElapsed;
    base::TimeTicks newTickTarget = lastEffectiveTick + newInterval;
    DCHECK(newTickTarget > now);

    // Avoid double ticks when:
    // 1) Turning off the timer and turning it right back on.
    // 2) Jittery data is passed to setTimebaseAndInterval().
    if (newTickTarget - m_lastTickTime <= newInterval / static_cast<int>(1.0 / doubleTickThreshold))
        newTickTarget += newInterval;

    return newTickTarget;
}

void DelayBasedTimeSource::postNextTickTask(base::TimeTicks now)
{
    base::TimeTicks newTickTarget = nextTickTarget(now);

    // Post another task *before* the tick and update state
    base::TimeDelta delay = newTickTarget - now;
    DCHECK(delay.InMillisecondsF() <=
           m_nextParameters.interval.InMillisecondsF() * (1.0 + doubleTickThreshold));
    m_thread->postDelayedTask(base::Bind(&DelayBasedTimeSource::onTimerFired,
                                         m_weakFactory.GetWeakPtr()),
                              delay.InMilliseconds());

    m_nextParameters.tickTarget = newTickTarget;
    m_currentParameters = m_nextParameters;
}

}  // namespace cc
