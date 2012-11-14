// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAGE_SCALE_ANIMATION_H_
#define CC_PAGE_SCALE_ANIMATION_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {

// A small helper class that does the math for zoom animations, primarily for
// double-tap zoom. Initialize it with starting and ending scroll/page scale
// positions and an animation length time, then call ...AtTime() at every frame
// to obtain the current interpolated position.
//
// All sizes and vectors in this class's public methods are in the root scroll
// layer's coordinate space.
class PageScaleAnimation {
public:
    // Construct with the state at the beginning of the animation.
    static scoped_ptr<PageScaleAnimation> create(const gfx::Vector2dF& startScrollOffset, float startPageScaleFactor, const gfx::SizeF& viewportSize, const gfx::SizeF& rootLayerSize, double startTime);

    ~PageScaleAnimation();

    // The following methods initialize the animation. Call one of them
    // immediately after construction to set the final scroll and page scale.

    // Zoom while explicitly specifying the top-left scroll position.
    void zoomTo(const gfx::Vector2dF& targetScrollOffset, float targetPageScaleFactor, double duration);

    // Zoom based on a specified anchor. The animator will attempt to keep it
    // at the same position on the physical display throughout the animation,
    // unless the edges of the root layer are hit. The anchor is specified
    // as an offset from the content layer.
    void zoomWithAnchor(const gfx::Vector2dF& anchor, float targetPageScaleFactor, double duration);

    // Call these functions while the animation is in progress to output the
    // current state.
    gfx::Vector2dF scrollOffsetAtTime(double time) const;
    float pageScaleFactorAtTime(double time) const;
    bool isAnimationCompleteAtTime(double time) const;

    // The following methods return state which is invariant throughout the
    // course of the animation.
    double startTime() const { return m_startTime; }
    double duration() const { return m_duration; }
    double endTime() const { return m_startTime + m_duration; }
    const gfx::Vector2dF& targetScrollOffset() const { return m_targetScrollOffset; }
    float targetPageScaleFactor() const { return m_targetPageScaleFactor; }

protected:
    PageScaleAnimation(const gfx::Vector2dF& startScrollOffset, float startPageScaleFactor, const gfx::SizeF& viewportSize, const gfx::SizeF& rootLayerSize, double startTime);

private:
    void clampTargetScrollOffset();
    void inferTargetScrollOffsetFromStartAnchor();
    void inferTargetAnchorFromScrollOffsets();

    gfx::SizeF startViewportSize() const;
    gfx::SizeF targetViewportSize() const;
    float interpAtTime(double time) const;
    gfx::SizeF viewportSizeAt(float interp) const;
    gfx::Vector2dF scrollOffsetAt(float interp) const;
    gfx::Vector2dF anchorAt(float interp) const;
    gfx::Vector2dF viewportRelativeAnchorAt(float interp) const;
    float pageScaleFactorAt(float interp) const;

    float m_startPageScaleFactor;
    float m_targetPageScaleFactor;
    gfx::Vector2dF m_startScrollOffset;
    gfx::Vector2dF m_targetScrollOffset;

    gfx::Vector2dF m_startAnchor;
    gfx::Vector2dF m_targetAnchor;

    gfx::SizeF m_viewportSize;
    gfx::SizeF m_rootLayerSize;

    double m_startTime;
    double m_duration;
};

}  // namespace cc

#endif  // CC_PAGE_SCALE_ANIMATION_H_
