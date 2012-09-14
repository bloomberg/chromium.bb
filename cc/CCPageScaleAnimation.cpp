// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCPageScaleAnimation.h"

#include "FloatRect.h"
#include "FloatSize.h"

#include <math.h>

namespace cc {

PassOwnPtr<CCPageScaleAnimation> CCPageScaleAnimation::create(const IntSize& scrollStart, float pageScaleStart, const IntSize& windowSize, const IntSize& contentSize, double startTime)
{
    return adoptPtr(new CCPageScaleAnimation(scrollStart, pageScaleStart, windowSize, contentSize, startTime));
}


CCPageScaleAnimation::CCPageScaleAnimation(const IntSize& scrollStart, float pageScaleStart, const IntSize& windowSize, const IntSize& contentSize, double startTime)
    : m_scrollStart(scrollStart)
    , m_pageScaleStart(pageScaleStart)
    , m_windowSize(windowSize)
    , m_contentSize(contentSize)
    , m_anchorMode(false)
    , m_scrollEnd(scrollStart)
    , m_pageScaleEnd(pageScaleStart)
    , m_startTime(startTime)
    , m_duration(0)
{
}

void CCPageScaleAnimation::zoomTo(const IntSize& finalScroll, float finalPageScale, double duration)
{
    if (m_pageScaleStart != finalPageScale) {
        // For uniform-looking zooming, infer the anchor (point that remains in
        // place throughout the zoom) from the start and end rects.
        FloatRect startRect(IntPoint(m_scrollStart), m_windowSize);
        FloatRect endRect(IntPoint(finalScroll), m_windowSize);
        endRect.scale(m_pageScaleStart / finalPageScale);

        // The anchor is the point which is at the same ratio of the sides of
        // both startRect and endRect. For example, a zoom-in double-tap to a
        // perfectly centered rect will have anchor ratios (0.5, 0.5), while one
        // to a rect touching the bottom-right of the screen will have anchor
        // ratios (1.0, 1.0). In other words, it obeys the equations:
        // anchorX = start_width * ratioX + start_x
        // anchorX = end_width * ratioX + end_x
        // anchorY = start_height * ratioY + start_y
        // anchorY = end_height * ratioY + end_y
        // where both anchor{x,y} and ratio{x,y} begin as unknowns. Solving
        // for the ratios, we get the following formulas:
        float ratioX = (startRect.x() - endRect.x()) / (endRect.width() - startRect.width());
        float ratioY = (startRect.y() - endRect.y()) / (endRect.height() - startRect.height());

        IntSize anchor(m_windowSize.width() * ratioX, m_windowSize.height() * ratioY);
        zoomWithAnchor(anchor, finalPageScale, duration);
    } else {
        // If this is a pure translation, then there exists no anchor. Linearly
        // interpolate the scroll offset instead.
        m_scrollEnd = finalScroll;
        m_pageScaleEnd = finalPageScale;
        m_duration = duration;
        m_anchorMode = false;
    }
}

void CCPageScaleAnimation::zoomWithAnchor(const IntSize& anchor, float finalPageScale, double duration)
{
    m_scrollEnd = m_scrollStart + anchor;
    m_scrollEnd.scale(finalPageScale / m_pageScaleStart);
    m_scrollEnd -= anchor;

    m_scrollEnd.clampNegativeToZero();
    FloatSize scaledContentSize(m_contentSize);
    scaledContentSize.scale(finalPageScale / m_pageScaleStart);
    IntSize maxScrollPosition = roundedIntSize(scaledContentSize - m_windowSize);
    m_scrollEnd = m_scrollEnd.shrunkTo(maxScrollPosition);

    m_anchor = anchor;
    m_pageScaleEnd = finalPageScale;
    m_duration = duration;
    m_anchorMode = true;
}

IntSize CCPageScaleAnimation::scrollOffsetAtTime(double time) const
{
    return scrollOffsetAtRatio(progressRatioForTime(time));
}

float CCPageScaleAnimation::pageScaleAtTime(double time) const
{
    return pageScaleAtRatio(progressRatioForTime(time));
}

bool CCPageScaleAnimation::isAnimationCompleteAtTime(double time) const
{
    return time >= endTime();
}

float CCPageScaleAnimation::progressRatioForTime(double time) const
{
    if (isAnimationCompleteAtTime(time))
        return 1;

    return (time - m_startTime) / m_duration;
}

IntSize CCPageScaleAnimation::scrollOffsetAtRatio(float ratio) const
{
    if (ratio <= 0)
        return m_scrollStart;
    if (ratio >= 1)
        return m_scrollEnd;

    float currentPageScale = pageScaleAtRatio(ratio);
    IntSize currentScrollOffset;
    if (m_anchorMode) {
        // Keep the anchor stable on the screen at the current scale.
        IntSize documentAnchor = m_scrollStart + m_anchor;
        documentAnchor.scale(currentPageScale / m_pageScaleStart);
        currentScrollOffset = documentAnchor - m_anchor;
    } else {
        // First move both scroll offsets to the current coordinate space.
        FloatSize scaledStartScroll(m_scrollStart);
        scaledStartScroll.scale(currentPageScale / m_pageScaleStart);
        FloatSize scaledEndScroll(m_scrollEnd);
        scaledEndScroll.scale(currentPageScale / m_pageScaleEnd);

        // Linearly interpolate between them.
        FloatSize delta = scaledEndScroll - scaledStartScroll;
        delta.scale(ratio);
        currentScrollOffset = roundedIntSize(scaledStartScroll + delta);
    }

    return currentScrollOffset;
}

float CCPageScaleAnimation::pageScaleAtRatio(float ratio) const
{
    if (ratio <= 0)
        return m_pageScaleStart;
    if (ratio >= 1)
        return m_pageScaleEnd;

    // Linearly interpolate the magnitude in log scale.
    // Log scale is needed to maintain the appearance of uniform zoom. For
    // example, if we zoom from 0.5 to 4.0 in 3 seconds, then we should
    // be zooming by 2x every second.
    float diff = m_pageScaleEnd / m_pageScaleStart;
    float logDiff = log(diff);
    logDiff *= ratio;
    diff = exp(logDiff);
    return m_pageScaleStart * diff;
}

} // namespace cc
