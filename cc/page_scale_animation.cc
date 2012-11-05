// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/page_scale_animation.h"

#include "cc/geometry.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/vector2d_conversions.h"
#include "ui/gfx/vector2d_f.h"

#include <math.h>

namespace cc {

scoped_ptr<PageScaleAnimation> PageScaleAnimation::create(gfx::Vector2d scrollStart, float pageScaleStart, const gfx::Size& windowSize, const gfx::Size& contentSize, double startTime)
{
    return make_scoped_ptr(new PageScaleAnimation(scrollStart, pageScaleStart, windowSize, contentSize, startTime));
}

PageScaleAnimation::PageScaleAnimation(gfx::Vector2d scrollStart, float pageScaleStart, const gfx::Size& windowSize, const gfx::Size& contentSize, double startTime)
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

PageScaleAnimation::~PageScaleAnimation()
{
}

void PageScaleAnimation::zoomTo(gfx::Vector2d finalScroll, float finalPageScale, double duration)
{
    if (m_pageScaleStart != finalPageScale) {
        // For uniform-looking zooming, infer the anchor (point that remains in
        // place throughout the zoom) from the start and end rects.
        gfx::RectF startRect(gfx::PointAtOffsetFromOrigin(m_scrollStart), m_windowSize);
        gfx::RectF endRect(gfx::PointAtOffsetFromOrigin(finalScroll), m_windowSize);
        endRect.Scale(m_pageScaleStart / finalPageScale);

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

        gfx::Vector2d anchor(m_windowSize.width() * ratioX, m_windowSize.height() * ratioY);
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

void PageScaleAnimation::zoomWithAnchor(gfx::Vector2d anchor, float finalPageScale, double duration)
{
    m_scrollEnd = m_scrollStart + anchor;
    m_scrollEnd = gfx::ToFlooredVector2d(cc::ScaleVector2d(m_scrollEnd, finalPageScale / m_pageScaleStart));
    m_scrollEnd -= anchor;

    m_scrollEnd = ClampFromBelow(m_scrollEnd, gfx::Vector2d());
    gfx::SizeF scaledContentSize = m_contentSize.Scale(finalPageScale / m_pageScaleStart);
    gfx::Vector2d maxScrollOffset = gfx::ToRoundedVector2d(BottomRight(gfx::RectF(scaledContentSize)) - BottomRight(gfx::Rect(m_windowSize)));
    m_scrollEnd = m_scrollEnd;
    m_scrollEnd = ClampFromAbove(m_scrollEnd, maxScrollOffset);

    m_anchor = anchor;
    m_pageScaleEnd = finalPageScale;
    m_duration = duration;
    m_anchorMode = true;
}

gfx::Vector2d PageScaleAnimation::scrollOffsetAtTime(double time) const
{
    return scrollOffsetAtRatio(progressRatioForTime(time));
}

float PageScaleAnimation::pageScaleAtTime(double time) const
{
    return pageScaleAtRatio(progressRatioForTime(time));
}

bool PageScaleAnimation::isAnimationCompleteAtTime(double time) const
{
    return time >= endTime();
}

float PageScaleAnimation::progressRatioForTime(double time) const
{
    if (isAnimationCompleteAtTime(time))
        return 1;

    return (time - m_startTime) / m_duration;
}

gfx::Vector2d PageScaleAnimation::scrollOffsetAtRatio(float ratio) const
{
    if (ratio <= 0)
        return m_scrollStart;
    if (ratio >= 1)
        return m_scrollEnd;

    float currentPageScale = pageScaleAtRatio(ratio);
    gfx::Vector2d currentScrollOffset;
    if (m_anchorMode) {
        // Keep the anchor stable on the screen at the current scale.
        gfx::Vector2dF documentAnchor = m_scrollStart + m_anchor;
        documentAnchor.Scale(currentPageScale / m_pageScaleStart);
        currentScrollOffset = gfx::ToRoundedVector2d(documentAnchor - m_anchor);
    } else {
        // First move both scroll offsets to the current coordinate space.
        gfx::Vector2dF scaledStartScroll(m_scrollStart);
        scaledStartScroll.Scale(currentPageScale / m_pageScaleStart);
        gfx::Vector2dF scaledEndScroll(m_scrollEnd);
        scaledEndScroll.Scale(currentPageScale / m_pageScaleEnd);

        // Linearly interpolate between them.
        gfx::Vector2dF delta = scaledEndScroll - scaledStartScroll;
        delta.Scale(ratio);
        currentScrollOffset = gfx::ToRoundedVector2d(scaledStartScroll + delta);
    }

    return currentScrollOffset;
}

float PageScaleAnimation::pageScaleAtRatio(float ratio) const
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

}  // namespace cc
