// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scrollbar_animation_controller.h"

#include "base/debug/trace_event.h"
#include "base/time.h"
#include "build/build_config.h"
#include "cc/scrollbar_layer_impl.h"

#if defined(OS_ANDROID)
#include "cc/scrollbar_animation_controller_linear_fade.h"
#endif

namespace cc {

#if defined(OS_ANDROID)
scoped_ptr<ScrollbarAnimationController> ScrollbarAnimationController::create(LayerImpl* scrollLayer)
{
    static const double fadeoutDelay = 0.3;
    static const double fadeoutLength = 0.3;
    return ScrollbarAnimationControllerLinearFade::create(scrollLayer, fadeoutDelay, fadeoutLength).PassAs<ScrollbarAnimationController>();
}
#else
scoped_ptr<ScrollbarAnimationController> ScrollbarAnimationController::create(LayerImpl* scrollLayer)
{
    return make_scoped_ptr(new ScrollbarAnimationController(scrollLayer));
}
#endif

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

gfx::Size ScrollbarAnimationController::getScrollLayerBounds(const LayerImpl* scrollLayer)
{
    if (!scrollLayer->children().size())
        return gfx::Size();
    // Copy & paste from LayerTreeHostImpl...
    // FIXME: Hardcoding the first child here is weird. Think of
    // a cleaner way to get the contentBounds on the Impl side.
    return scrollLayer->children()[0]->bounds();
}

void ScrollbarAnimationController::updateScrollOffsetAtTime(LayerImpl* scrollLayer, double)
{
    m_currentOffset = scrollLayer->scrollOffset() + scrollLayer->scrollDelta();
    m_totalSize = getScrollLayerBounds(scrollLayer);
    m_maximum = scrollLayer->maxScrollOffset();

    // Get the m_currentOffset.y() value for a sanity-check on scrolling
    // benchmark metrics. Specifically, we want to make sure
    // BasicMouseWheelSmoothScrollGesture has proper scroll curves.
    TRACE_COUNTER_ID1("gpu", "scroll_offset_y", this, m_currentOffset.y());


    if (m_horizontalScrollbarLayer) {
        m_horizontalScrollbarLayer->setCurrentPos(m_currentOffset.x());
        m_horizontalScrollbarLayer->setTotalSize(m_totalSize.width());
        m_horizontalScrollbarLayer->setMaximum(m_maximum.x());
    }

    if (m_verticalScrollbarLayer) {
        m_verticalScrollbarLayer->setCurrentPos(m_currentOffset.y());
        m_verticalScrollbarLayer->setTotalSize(m_totalSize.height());
        m_verticalScrollbarLayer->setMaximum(m_maximum.y());
    }
}

}  // namespace cc
