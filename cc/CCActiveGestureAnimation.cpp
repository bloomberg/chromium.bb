// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCActiveGestureAnimation.h"

#include "CCGestureCurve.h"
#include "TraceEvent.h"

namespace WebCore {

PassOwnPtr<CCActiveGestureAnimation> CCActiveGestureAnimation::create(PassOwnPtr<CCGestureCurve> curve, CCGestureCurveTarget* target)
{
    return adoptPtr(new CCActiveGestureAnimation(curve, target));
}

CCActiveGestureAnimation::CCActiveGestureAnimation(PassOwnPtr<CCGestureCurve> curve, CCGestureCurveTarget* target)
    : m_startTime(0)
    , m_waitingForFirstTick(true)
    , m_gestureCurve(curve)
    , m_gestureCurveTarget(target)
{
    TRACE_EVENT_ASYNC_BEGIN1("input", "GestureAnimation", this, "curve", m_gestureCurve->debugName());
}

CCActiveGestureAnimation::~CCActiveGestureAnimation()
{
    TRACE_EVENT_ASYNC_END0("input", "GestureAnimation", this);
}

bool CCActiveGestureAnimation::animate(double monotonicTime)
{
    if (m_waitingForFirstTick) {
        m_startTime = monotonicTime;
        m_waitingForFirstTick = false;
    }

    // CCGestureCurves used zero-based time, so subtract start-time.
    return m_gestureCurve->apply(monotonicTime - m_startTime, m_gestureCurveTarget);
}

} // namespace WebCore
