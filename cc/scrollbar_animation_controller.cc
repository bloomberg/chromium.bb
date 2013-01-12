// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scrollbar_animation_controller.h"

#include "base/debug/trace_event.h"
#include "base/time.h"
#include "build/build_config.h"
#include "cc/scrollbar_layer_impl.h"

namespace cc {

scoped_ptr<ScrollbarAnimationController> ScrollbarAnimationController::create(LayerImpl* scrollLayer)
{
    return make_scoped_ptr(new ScrollbarAnimationController(scrollLayer));
}

ScrollbarAnimationController::ScrollbarAnimationController(LayerImpl* scrollLayer)
    : m_horizontalScrollbarLayer(0)
    , m_verticalScrollbarLayer(0)
{
    ScrollbarAnimationController::updateScrollOffsetAtTime(scrollLayer, 0);
}

ScrollbarAnimationController::~ScrollbarAnimationController()
{
}

bool ScrollbarAnimationController::animate(double)
{
    return false;
}

void ScrollbarAnimationController::didPinchGestureBegin()
{
    didPinchGestureBeginAtTime((base::TimeTicks::Now() - base::TimeTicks()).InSecondsF());
}

void ScrollbarAnimationController::didPinchGestureUpdate()
{
    didPinchGestureUpdateAtTime((base::TimeTicks::Now() - base::TimeTicks()).InSecondsF());
}

void ScrollbarAnimationController::didPinchGestureEnd()
{
    didPinchGestureEndAtTime((base::TimeTicks::Now() - base::TimeTicks()).InSecondsF());
}

void ScrollbarAnimationController::updateScrollOffset(LayerImpl* scrollLayer)
{
    updateScrollOffsetAtTime(scrollLayer, (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF());
}

void ScrollbarAnimationController::setHorizontalScrollbarLayer(ScrollbarLayerImpl* layer)
{
    m_horizontalScrollbarLayer = layer;
    if (layer)
        layer->setAnimationController(this);
}

void ScrollbarAnimationController::setVerticalScrollbarLayer(ScrollbarLayerImpl* layer)
{
    m_verticalScrollbarLayer = layer;
    if (layer)
        layer->setAnimationController(this);
}

void ScrollbarAnimationController::updateScrollOffsetAtTime(LayerImpl* scrollLayer, double)
{
    m_currentOffset = scrollLayer->scrollOffset() + scrollLayer->scrollDelta();
    m_totalSize = scrollLayer->bounds();
    m_maximum = scrollLayer->maxScrollOffset();

    // Get the m_currentOffset.y() value for a sanity-check on scrolling
    // benchmark metrics. Specifically, we want to make sure
    // BasicMouseWheelSmoothScrollGesture has proper scroll curves.
    TRACE_COUNTER_ID1("gpu", "scroll_offset_y", this, m_currentOffset.y());
}

}  // namespace cc
