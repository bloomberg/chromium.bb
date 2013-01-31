// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/page_scale_animation.h"

#include "base/logging.h"
#include "ui/gfx/point_f.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/vector2d_conversions.h"

#include <math.h>

namespace {

// This takes a viewport-relative vector and returns a vector whose values are
// between 0 and 1, representing the percentage position within the viewport.
gfx::Vector2dF normalizeFromViewport(const gfx::Vector2dF& denormalized, const gfx::SizeF& viewportSize)
{
    return gfx::ScaleVector2d(denormalized, 1 / viewportSize.width(), 1 / viewportSize.height());
}

gfx::Vector2dF denormalizeToViewport(const gfx::Vector2dF& normalized, const gfx::SizeF& viewportSize)
{
    return gfx::ScaleVector2d(normalized, viewportSize.width(), viewportSize.height());
}

gfx::Vector2dF interpolateBetween(const gfx::Vector2dF& start, const gfx::Vector2dF end, float interp)
{
    return start + gfx::ScaleVector2d(end - start, interp);
}

}

namespace cc {

scoped_ptr<PageScaleAnimation> PageScaleAnimation::create(const gfx::Vector2dF& startScrollOffset, float startPageScaleFactor, const gfx::SizeF& viewportSize, const gfx::SizeF& rootLayerSize, double startTime)
{
    return make_scoped_ptr(new PageScaleAnimation(startScrollOffset, startPageScaleFactor, viewportSize, rootLayerSize, startTime));
}

PageScaleAnimation::PageScaleAnimation(const gfx::Vector2dF& startScrollOffset, float startPageScaleFactor, const gfx::SizeF& viewportSize, const gfx::SizeF& rootLayerSize, double startTime)
    : m_startPageScaleFactor(startPageScaleFactor)
    , m_targetPageScaleFactor(0)
    , m_startScrollOffset(startScrollOffset)
    , m_startAnchor()
    , m_targetAnchor()
    , m_viewportSize(viewportSize)
    , m_rootLayerSize(rootLayerSize)
    , m_startTime(startTime)
    , m_duration(0)
{
}

PageScaleAnimation::~PageScaleAnimation()
{
}

void PageScaleAnimation::zoomTo(const gfx::Vector2dF& targetScrollOffset, float targetPageScaleFactor, double duration)
{
    m_targetPageScaleFactor = targetPageScaleFactor;
    m_targetScrollOffset = targetScrollOffset;
    clampTargetScrollOffset();
    m_duration = duration;

    if (m_startPageScaleFactor == targetPageScaleFactor) {
        m_startAnchor = m_startScrollOffset;
        m_targetAnchor = targetScrollOffset;
        return;
    }

    // For uniform-looking zooming, infer an anchor from the start and target
    // viewport rects.
    inferTargetAnchorFromScrollOffsets();
    m_startAnchor = m_targetAnchor;
}

void PageScaleAnimation::zoomWithAnchor(const gfx::Vector2dF& anchor, float targetPageScaleFactor, double duration)
{
    m_startAnchor = anchor;
    m_targetPageScaleFactor = targetPageScaleFactor;
    m_duration = duration;

    // We start zooming out from the anchor tapped by the user. But if
    // the target scale is impossible to attain without hitting the root layer
    // edges, then infer an anchor that doesn't collide with the edges.
    // We will interpolate between the two anchors during the animation.
    inferTargetScrollOffsetFromStartAnchor();
    clampTargetScrollOffset();

    if (m_startPageScaleFactor == m_targetPageScaleFactor) {
        m_targetAnchor = m_startAnchor;
        return;
    }
    inferTargetAnchorFromScrollOffsets();
}

void PageScaleAnimation::inferTargetScrollOffsetFromStartAnchor()
{
    gfx::Vector2dF normalized = normalizeFromViewport(m_startAnchor - m_startScrollOffset, startViewportSize());
    m_targetScrollOffset = m_startAnchor - denormalizeToViewport(normalized, targetViewportSize());
}

void PageScaleAnimation::inferTargetAnchorFromScrollOffsets()
{
    // The anchor is the point which is at the same normalized relative position
    // within both start viewport rect and target viewport rect. For example, a
    // zoom-in double-tap to a perfectly centered rect will have normalized
    // anchor (0.5, 0.5), while one to a rect touching the bottom-right of the
    // screen will have normalized anchor (1.0, 1.0). In other words, it obeys
    // the equations:
    // anchor = start_size * normalized + start_offset
    // anchor = target_size * normalized + target_offset
    // where both anchor and normalized begin as unknowns. Solving
    // for the normalized, we get the following:
    float widthScale = 1 / (targetViewportSize().width() - startViewportSize().width());
    float heightScale = 1 / (targetViewportSize().height() - startViewportSize().height());
    gfx::Vector2dF normalized = gfx::ScaleVector2d(m_startScrollOffset - m_targetScrollOffset, widthScale, heightScale);
    m_targetAnchor = m_targetScrollOffset + denormalizeToViewport(normalized, targetViewportSize());
}

void PageScaleAnimation::clampTargetScrollOffset()
{
    gfx::Vector2dF maxScrollOffset = gfx::RectF(m_rootLayerSize).bottom_right() - gfx::RectF(targetViewportSize()).bottom_right();
    m_targetScrollOffset.ClampToMin(gfx::Vector2dF());
    m_targetScrollOffset.ClampToMax(maxScrollOffset);
}

gfx::SizeF PageScaleAnimation::startViewportSize() const
{
    return gfx::ScaleSize(m_viewportSize, 1 / m_startPageScaleFactor);
}

gfx::SizeF PageScaleAnimation::targetViewportSize() const
{
    return gfx::ScaleSize(m_viewportSize, 1 / m_targetPageScaleFactor);
}

gfx::SizeF PageScaleAnimation::viewportSizeAt(float interp) const
{
    return gfx::ScaleSize(m_viewportSize, 1 / pageScaleFactorAt(interp));
}

gfx::Vector2dF PageScaleAnimation::scrollOffsetAtTime(double time) const
{
    return scrollOffsetAt(interpAtTime(time));
}

float PageScaleAnimation::pageScaleFactorAtTime(double time) const
{
    return pageScaleFactorAt(interpAtTime(time));
}

bool PageScaleAnimation::isAnimationCompleteAtTime(double time) const
{
    return time >= endTime();
}

float PageScaleAnimation::interpAtTime(double time) const
{
    DCHECK_GE(time, m_startTime);
    if (isAnimationCompleteAtTime(time))
        return 1;

    return (time - m_startTime) / m_duration;
}

gfx::Vector2dF PageScaleAnimation::scrollOffsetAt(float interp) const
{
    if (interp <= 0)
        return m_startScrollOffset;
    if (interp >= 1)
        return m_targetScrollOffset;

    return anchorAt(interp) - viewportRelativeAnchorAt(interp);
}

gfx::Vector2dF PageScaleAnimation::anchorAt(float interp) const
{
    // Interpolate from start to target anchor in absolute space.
    return interpolateBetween(m_startAnchor, m_targetAnchor, interp);
}

gfx::Vector2dF PageScaleAnimation::viewportRelativeAnchorAt(float interp) const
{
    // Interpolate from start to target anchor in normalized space.
    gfx::Vector2dF startNormalized = normalizeFromViewport(m_startAnchor - m_startScrollOffset, startViewportSize());
    gfx::Vector2dF targetNormalized = normalizeFromViewport(m_targetAnchor - m_targetScrollOffset, targetViewportSize());
    gfx::Vector2dF interpNormalized = interpolateBetween(startNormalized, targetNormalized, interp);

    return denormalizeToViewport(interpNormalized, viewportSizeAt(interp));
}

float PageScaleAnimation::pageScaleFactorAt(float interp) const
{
    if (interp <= 0)
        return m_startPageScaleFactor;
    if (interp >= 1)
        return m_targetPageScaleFactor;

    // Linearly interpolate the magnitude in log scale.
    float diff = m_targetPageScaleFactor / m_startPageScaleFactor;
    float logDiff = log(diff);
    logDiff *= interp;
    diff = exp(logDiff);
    return m_startPageScaleFactor * diff;
}

}  // namespace cc
