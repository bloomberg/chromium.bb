// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scrollbar_animation_controller_linear_fade.h"

#include "base/time.h"
#include "cc/layer_impl.h"

namespace cc {

scoped_ptr<ScrollbarAnimationControllerLinearFade> ScrollbarAnimationControllerLinearFade::create(LayerImpl* scrollLayer, base::TimeDelta fadeoutDelay, base::TimeDelta fadeoutLength)
{
    return make_scoped_ptr(new ScrollbarAnimationControllerLinearFade(scrollLayer, fadeoutDelay, fadeoutLength));
}

ScrollbarAnimationControllerLinearFade::ScrollbarAnimationControllerLinearFade(LayerImpl* scrollLayer, base::TimeDelta fadeoutDelay, base::TimeDelta fadeoutLength)
    : ScrollbarAnimationController()
    , m_scrollLayer(scrollLayer)
    , m_scrollGestureInProgress(false)
    , m_fadeoutDelay(fadeoutDelay)
    , m_fadeoutLength(fadeoutLength)
{
}

ScrollbarAnimationControllerLinearFade::~ScrollbarAnimationControllerLinearFade()
{
}

bool ScrollbarAnimationControllerLinearFade::isScrollGestureInProgress() const
{
    return m_scrollGestureInProgress;
}

bool ScrollbarAnimationControllerLinearFade::isAnimating() const
{
    return !m_lastAwakenTime.is_null();
}

base::TimeDelta ScrollbarAnimationControllerLinearFade::delayBeforeStart(base::TimeTicks now) const
{
    if (now > m_lastAwakenTime + m_fadeoutDelay)
        return base::TimeDelta();
    return m_fadeoutDelay - (now - m_lastAwakenTime);
}

bool ScrollbarAnimationControllerLinearFade::animate(base::TimeTicks now)
{
    float opacity = opacityAtTime(now);
    m_scrollLayer->SetScrollbarOpacity(opacity);
    if (!opacity)
       m_lastAwakenTime = base::TimeTicks();
    return isAnimating() && delayBeforeStart(now) == base::TimeDelta();
}

void ScrollbarAnimationControllerLinearFade::didScrollGestureBegin()
{
    m_scrollLayer->SetScrollbarOpacity(1);
    m_scrollGestureInProgress = true;
    m_lastAwakenTime = base::TimeTicks();
}

void ScrollbarAnimationControllerLinearFade::didScrollGestureEnd(base::TimeTicks now)
{
    m_scrollGestureInProgress = false;
    m_lastAwakenTime = now;
}

void ScrollbarAnimationControllerLinearFade::didProgrammaticallyUpdateScroll(base::TimeTicks now)
{
    // Don't set m_scrollGestureInProgress as this scroll is not from a gesture
    // and we won't receive ScrollEnd.
    m_scrollLayer->SetScrollbarOpacity(1);
    m_lastAwakenTime = now;
}

float ScrollbarAnimationControllerLinearFade::opacityAtTime(base::TimeTicks now)
{
    if (m_scrollGestureInProgress)
        return 1;

    if (m_lastAwakenTime.is_null())
        return 0;

    base::TimeDelta delta = now - m_lastAwakenTime;

    if (delta <= m_fadeoutDelay)
        return 1;
    if (delta < m_fadeoutDelay + m_fadeoutLength)
        return (m_fadeoutDelay + m_fadeoutLength - delta).InSecondsF() / m_fadeoutLength.InSecondsF();
    return 0;
}

}  // namespace cc
