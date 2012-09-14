// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCPageScaleAnimation_h
#define CCPageScaleAnimation_h

#include "IntSize.h"
#include <wtf/PassOwnPtr.h>

namespace cc {

// A small helper class that does the math for zoom animations, primarily for
// double-tap zoom. Initialize it with starting and ending scroll/page scale
// positions and an animation length time, then call ...AtTime() at every frame
// to obtain the current interpolated position.
class CCPageScaleAnimation {
public:
    // Construct with the starting page scale and scroll offset (which is in
    // pageScaleStart space). The window size is the user-viewable area
    // in pixels.
    static PassOwnPtr<CCPageScaleAnimation> create(const IntSize& scrollStart, float pageScaleStart, const IntSize& windowSize, const IntSize& contentSize, double startTime);

    // The following methods initialize the animation. Call one of them
    // immediately after construction to set the final scroll and page scale.

    // Zoom while explicitly specifying the top-left scroll position. The
    // scroll offset is in finalPageScale coordinates.
    void zoomTo(const IntSize& finalScroll, float finalPageScale, double duration);

    // Zoom based on a specified onscreen anchor, which will remain at the same
    // position on the screen throughout the animation. The anchor is in local
    // space relative to scrollStart.
    void zoomWithAnchor(const IntSize& anchor, float finalPageScale, double duration);

    // Call these functions while the animation is in progress to output the
    // current state.
    IntSize scrollOffsetAtTime(double time) const;
    float pageScaleAtTime(double time) const;
    bool isAnimationCompleteAtTime(double time) const;

    // The following methods return state which is invariant throughout the
    // course of the animation.
    double startTime() const { return m_startTime; }
    double duration() const { return m_duration; }
    double endTime() const { return m_startTime + m_duration; }
    const IntSize& finalScrollOffset() const { return m_scrollEnd; }
    float finalPageScale() const { return m_pageScaleEnd; }

protected:
    CCPageScaleAnimation(const IntSize& scrollStart, float pageScaleStart, const IntSize& windowSize, const IntSize& contentSize, double startTime);

private:
    float progressRatioForTime(double time) const;
    IntSize scrollOffsetAtRatio(float ratio) const;
    float pageScaleAtRatio(float ratio) const;

    IntSize m_scrollStart;
    float m_pageScaleStart;
    IntSize m_windowSize;
    IntSize m_contentSize;

    bool m_anchorMode;
    IntSize m_anchor;
    IntSize m_scrollEnd;
    float m_pageScaleEnd;

    double m_startTime;
    double m_duration;
};

} // namespace cc

#endif
