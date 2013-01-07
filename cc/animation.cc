// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation.h"

#include <cmath>

#include "base/debug/trace_event.h"
#include "base/string_util.h"
#include "cc/animation_curve.h"

namespace {

// This should match the RunState enum.
static const char* const s_runStateNames[] = {
    "WaitingForNextTick",
    "WaitingForTargetAvailability",
    "WaitingForStartTime",
    "WaitingForDeletion",
    "Running",
    "Paused",
    "Finished",
    "Aborted"
};

COMPILE_ASSERT(static_cast<int>(cc::Animation::RunStateEnumSize) == arraysize(s_runStateNames), RunState_names_match_enum);

// This should match the TargetProperty enum.
static const char* const s_targetPropertyNames[] = {
    "Transform",
    "Opacity"
};

COMPILE_ASSERT(static_cast<int>(cc::Animation::TargetPropertyEnumSize) == arraysize(s_targetPropertyNames), TargetProperty_names_match_enum);

} // namespace

namespace cc {

scoped_ptr<Animation> Animation::create(scoped_ptr<AnimationCurve> curve, int animationId, int groupId, TargetProperty targetProperty)
{
    return make_scoped_ptr(new Animation(curve.Pass(), animationId, groupId, targetProperty));
}

Animation::Animation(scoped_ptr<AnimationCurve> curve, int animationId, int groupId, TargetProperty targetProperty)
    : m_curve(curve.Pass())
    , m_id(animationId)
    , m_group(groupId)
    , m_targetProperty(targetProperty)
    , m_runState(WaitingForTargetAvailability)
    , m_iterations(1)
    , m_startTime(0)
    , m_alternatesDirection(false)
    , m_timeOffset(0)
    , m_needsSynchronizedStartTime(false)
    , m_suspended(false)
    , m_pauseTime(0)
    , m_totalPausedTime(0)
    , m_isControllingInstance(false)
{
}

Animation::~Animation()
{
    if (m_runState == Running || m_runState == Paused)
        setRunState(Aborted, 0);
}

void Animation::setRunState(RunState runState, double monotonicTime)
{
    if (m_suspended)
        return;

    char nameBuffer[256];
    base::snprintf(nameBuffer, sizeof(nameBuffer), "%s-%d%s", s_targetPropertyNames[m_targetProperty], m_group, m_isControllingInstance ? "(impl)" : "");

    bool isWaitingToStart = m_runState == WaitingForNextTick
        || m_runState == WaitingForTargetAvailability
        || m_runState == WaitingForStartTime;

    if (isWaitingToStart && runState == Running)
        TRACE_EVENT_ASYNC_BEGIN1("cc", "Animation", this, "Name", TRACE_STR_COPY(nameBuffer));

    bool wasFinished = isFinished();

    const char* oldRunStateName = s_runStateNames[m_runState];

    if (runState == Running && m_runState == Paused)
        m_totalPausedTime += monotonicTime - m_pauseTime;
    else if (runState == Paused)
        m_pauseTime = monotonicTime;
    m_runState = runState;

    const char* newRunStateName = s_runStateNames[runState];

    if (!wasFinished && isFinished())
        TRACE_EVENT_ASYNC_END0("cc", "Animation", this);

    char stateBuffer[256];
    base::snprintf(stateBuffer, sizeof(stateBuffer), "%s->%s", oldRunStateName, newRunStateName);

    TRACE_EVENT_INSTANT2("cc", "LayerAnimationController::setRunState", "Name", TRACE_STR_COPY(nameBuffer), "State", TRACE_STR_COPY(stateBuffer));
}

void Animation::suspend(double monotonicTime)
{
    setRunState(Paused, monotonicTime);
    m_suspended = true;
}

void Animation::resume(double monotonicTime)
{
    m_suspended = false;
    setRunState(Running, monotonicTime);
}

bool Animation::isFinishedAt(double monotonicTime) const
{
    if (isFinished())
        return true;

    if (m_needsSynchronizedStartTime)
        return false;

    return m_runState == Running
        && m_iterations >= 0
        && m_iterations * m_curve->duration() <= monotonicTime - startTime() - m_totalPausedTime;
}

double Animation::trimTimeToCurrentIteration(double monotonicTime) const
{
    double trimmed = monotonicTime + m_timeOffset;

    // If we're paused, time is 'stuck' at the pause time.
    if (m_runState == Paused)
        trimmed = m_pauseTime;

    // Returned time should always be relative to the start time and should subtract
    // all time spent paused.
    trimmed -= m_startTime + m_totalPausedTime;

    // Zero is always the start of the animation.
    if (trimmed <= 0)
        return 0;

    // Always return zero if we have no iterations.
    if (!m_iterations)
        return 0;

    // Don't attempt to trim if we have no duration.
    if (m_curve->duration() <= 0)
        return 0;

    // If less than an iteration duration, just return trimmed.
    if (trimmed < m_curve->duration())
        return trimmed;

    // If greater than or equal to the total duration, return iteration duration.
    if (m_iterations >= 0 && trimmed >= m_curve->duration() * m_iterations) {
        if (m_alternatesDirection && !(m_iterations % 2))
            return 0;
        return m_curve->duration();
    }

    // We need to know the current iteration if we're alternating.
    int iteration = static_cast<int>(trimmed / m_curve->duration());

    // Calculate x where trimmed = x + n * m_curve->duration() for some positive integer n.
    trimmed = fmod(trimmed, m_curve->duration());

    // If we're alternating and on an odd iteration, reverse the direction.
    if (m_alternatesDirection && iteration % 2 == 1)
        return m_curve->duration() - trimmed;

    return trimmed;
}

scoped_ptr<Animation> Animation::clone(InstanceType instanceType) const
{
    return cloneAndInitialize(instanceType, m_runState, m_startTime);
}

scoped_ptr<Animation> Animation::cloneAndInitialize(InstanceType instanceType, RunState initialRunState, double startTime) const
{
    scoped_ptr<Animation> toReturn(new Animation(m_curve->clone(), m_id, m_group, m_targetProperty));
    toReturn->m_runState = initialRunState;
    toReturn->m_iterations = m_iterations;
    toReturn->m_startTime = startTime;
    toReturn->m_pauseTime = m_pauseTime;
    toReturn->m_totalPausedTime = m_totalPausedTime;
    toReturn->m_timeOffset = m_timeOffset;
    toReturn->m_alternatesDirection = m_alternatesDirection;
    toReturn->m_isControllingInstance = instanceType == ControllingInstance;
    return toReturn.Pass();
}

void Animation::pushPropertiesTo(Animation* other) const
{
    // Currently, we only push changes due to pausing and resuming animations on the main thread.
    if (m_runState == Animation::Paused || other->m_runState == Animation::Paused) {
        other->m_runState = m_runState;
        other->m_pauseTime = m_pauseTime;
        other->m_totalPausedTime = m_totalPausedTime;
    }
}

}  // namespace cc
