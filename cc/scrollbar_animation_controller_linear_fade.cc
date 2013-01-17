// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scrollbar_animation_controller_linear_fade.h"

#include "base/time.h"
#include "cc/layer_impl.h"

namespace cc {

scoped_ptr<ScrollbarAnimationControllerLinearFade> ScrollbarAnimationControllerLinearFade::create(LayerImpl* scrollLayer, double fadeoutDelay, double fadeoutLength)
{
    return make_scoped_ptr(new ScrollbarAnimationControllerLinearFade(scrollLayer, fadeoutDelay, fadeoutLength));
}

ScrollbarAnimationControllerLinearFade::ScrollbarAnimationControllerLinearFade(LayerImpl* scrollLayer, double fadeoutDelay, double fadeoutLength)
    : ScrollbarAnimationController()
    , m_scrollLayer(scrollLayer)
    , m_pinchGestureInEffect(false)
    , m_fadeoutDelay(fadeoutDelay)
    , m_fadeoutLength(fadeoutLength)
    , m_currentTimeForTesting(0)
{
}

ScrollbarAnimationControllerLinearFade::~ScrollbarAnimationControllerLinearFade()
{
}

bool ScrollbarAnimationControllerLinearFade::animate(base::TimeTicks now)
{
    float opacity = opacityAtTime(now);
    m_scrollLayer->setScrollbarOpacity(opacity);
    return opacity;
}

void ScrollbarAnimationControllerLinearFade::didPinchGestureUpdate(base::TimeTicks now)
{
    m_pinchGestureInEffect = true;
}

void ScrollbarAnimationControllerLinearFade::didPinchGestureEnd(base::TimeTicks now)
{
    m_pinchGestureInEffect = false;
    m_lastAwakenTime = now;
}

void ScrollbarAnimationControllerLinearFade::didUpdateScrollOffset(base::TimeTicks now)
{
    m_lastAwakenTime = now;
}

float ScrollbarAnimationControllerLinearFade::opacityAtTime(base::TimeTicks now)
{
    if (m_pinchGestureInEffect)
        return 1;

    if (m_lastAwakenTime.is_null())
        return 0;

    double delta = (now - m_lastAwakenTime).InSecondsF();

    if (delta <= m_fadeoutDelay)
        return 1;
    if (delta < m_fadeoutDelay + m_fadeoutLength)
        return (m_fadeoutDelay + m_fadeoutLength - delta) / m_fadeoutLength;
    return 0;
}

}  // namespace cc
