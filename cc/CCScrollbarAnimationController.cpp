// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCScrollbarAnimationController.h"

#include "CCScrollbarLayerImpl.h"
#include <wtf/CurrentTime.h>

#if OS(ANDROID)
#include "CCScrollbarAnimationControllerLinearFade.h"
#endif

namespace cc {

#if OS(ANDROID)
PassOwnPtr<CCScrollbarAnimationController> CCScrollbarAnimationController::create(CCLayerImpl* scrollLayer)
{
    static const double fadeoutDelay = 0.3;
    static const double fadeoutLength = 0.3;
    return CCScrollbarAnimationControllerLinearFade::create(scrollLayer, fadeoutDelay, fadeoutLength);
}
#else
PassOwnPtr<CCScrollbarAnimationController> CCScrollbarAnimationController::create(CCLayerImpl* scrollLayer)
{
    return adoptPtr(new CCScrollbarAnimationController(scrollLayer));
}
#endif

CCScrollbarAnimationController::CCScrollbarAnimationController(CCLayerImpl* scrollLayer)
    : m_horizontalScrollbarLayer(0)
    , m_verticalScrollbarLayer(0)
{
    CCScrollbarAnimationController::updateScrollOffsetAtTime(scrollLayer, 0);
}

CCScrollbarAnimationController::~CCScrollbarAnimationController()
{
}

void CCScrollbarAnimationController::didPinchGestureBegin()
{
    didPinchGestureBeginAtTime(monotonicallyIncreasingTime());
}

void CCScrollbarAnimationController::didPinchGestureUpdate()
{
    didPinchGestureUpdateAtTime(monotonicallyIncreasingTime());
}

void CCScrollbarAnimationController::didPinchGestureEnd()
{
    didPinchGestureEndAtTime(monotonicallyIncreasingTime());
}

void CCScrollbarAnimationController::updateScrollOffset(CCLayerImpl* scrollLayer)
{
    updateScrollOffsetAtTime(scrollLayer, monotonicallyIncreasingTime());
}

IntSize CCScrollbarAnimationController::getScrollLayerBounds(const CCLayerImpl* scrollLayer)
{
    if (!scrollLayer->children().size())
        return IntSize();
    // Copy & paste from CCLayerTreeHostImpl...
    // FIXME: Hardcoding the first child here is weird. Think of
    // a cleaner way to get the contentBounds on the Impl side.
    return scrollLayer->children()[0]->bounds();
}

void CCScrollbarAnimationController::updateScrollOffsetAtTime(CCLayerImpl* scrollLayer, double)
{
    m_currentPos = scrollLayer->scrollPosition() + scrollLayer->scrollDelta();
    m_totalSize = getScrollLayerBounds(scrollLayer);
    m_maximum = scrollLayer->maxScrollPosition();

    if (m_horizontalScrollbarLayer) {
        m_horizontalScrollbarLayer->setCurrentPos(m_currentPos.x());
        m_horizontalScrollbarLayer->setTotalSize(m_totalSize.width());
        m_horizontalScrollbarLayer->setMaximum(m_maximum.width());
    }

    if (m_verticalScrollbarLayer) {
        m_verticalScrollbarLayer->setCurrentPos(m_currentPos.y());
        m_verticalScrollbarLayer->setTotalSize(m_totalSize.height());
        m_verticalScrollbarLayer->setMaximum(m_maximum.height());
    }
}

} // namespace cc
