// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCScrollbarAnimationControllerLinearFade.h"

#include "CCScrollbarLayerImpl.h"

namespace cc {

scoped_ptr<ScrollbarAnimationControllerLinearFade> ScrollbarAnimationControllerLinearFade::create(LayerImpl* scrollLayer, double fadeoutDelay, double fadeoutLength)
{
    return make_scoped_ptr(new ScrollbarAnimationControllerLinearFade(scrollLayer, fadeoutDelay, fadeoutLength));
}

ScrollbarAnimationControllerLinearFade::ScrollbarAnimationControllerLinearFade(LayerImpl* scrollLayer, double fadeoutDelay, double fadeoutLength)
    : ScrollbarAnimationController(scrollLayer)
    , m_lastAwakenTime(-100000000) // arbitrary invalid timestamp
    , m_pinchGestureInEffect(false)
    , m_fadeoutDelay(fadeoutDelay)
    , m_fadeoutLength(fadeoutLength)
{
}

ScrollbarAnimationControllerLinearFade::~ScrollbarAnimationControllerLinearFade()
{
}

bool ScrollbarAnimationControllerLinearFade::animate(double monotonicTime)
{
    float opacity = opacityAtTime(monotonicTime);
    if (horizontalScrollbarLayer())
        horizontalScrollbarLayer()->setOpacity(opacity);
    if (verticalScrollbarLayer())
        verticalScrollbarLayer()->setOpacity(opacity);
    return opacity;
}

void ScrollbarAnimationControllerLinearFade::didPinchGestureUpdateAtTime(double)
{
    m_pinchGestureInEffect = true;
}

void ScrollbarAnimationControllerLinearFade::didPinchGestureEndAtTime(double monotonicTime)
{
    m_pinchGestureInEffect = false;
    m_lastAwakenTime = monotonicTime;
}

void ScrollbarAnimationControllerLinearFade::updateScrollOffsetAtTime(LayerImpl* scrollLayer, double monotonicTime)
{
    FloatPoint previousPos = currentPos();
    ScrollbarAnimationController::updateScrollOffsetAtTime(scrollLayer, monotonicTime);

    if (previousPos == currentPos())
        return;

    m_lastAwakenTime = monotonicTime;
}

float ScrollbarAnimationControllerLinearFade::opacityAtTime(double monotonicTime)
{
    if (m_pinchGestureInEffect)
        return 1;

    double delta = monotonicTime - m_lastAwakenTime;

    if (delta <= m_fadeoutDelay)
        return 1;
    if (delta < m_fadeoutDelay + m_fadeoutLength)
        return (m_fadeoutDelay + m_fadeoutLength - delta) / m_fadeoutLength;
    return 0;
}

} // namespace cc
