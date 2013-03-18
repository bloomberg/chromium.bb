// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLLBAR_ANIMATION_CONTROLLER_LINEAR_FADE_H_
#define CC_ANIMATION_SCROLLBAR_ANIMATION_CONTROLLER_LINEAR_FADE_H_

#include "base/memory/scoped_ptr.h"
#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/base/cc_export.h"

namespace cc {
class LayerImpl;

class CC_EXPORT ScrollbarAnimationControllerLinearFade : public ScrollbarAnimationController {
public:
    static scoped_ptr<ScrollbarAnimationControllerLinearFade> create(LayerImpl* scrollLayer, base::TimeDelta fadeoutDelay, base::TimeDelta fadeoutLength);

    virtual ~ScrollbarAnimationControllerLinearFade();

    // ScrollbarAnimationController overrides.
    virtual bool isScrollGestureInProgress() const OVERRIDE;
    virtual bool isAnimating() const OVERRIDE;
    virtual base::TimeDelta delayBeforeStart(base::TimeTicks now) const OVERRIDE;

    virtual bool animate(base::TimeTicks) OVERRIDE;
    virtual void didScrollGestureBegin() OVERRIDE;
    virtual void didScrollGestureEnd(base::TimeTicks now) OVERRIDE;
    virtual void didProgrammaticallyUpdateScroll(base::TimeTicks now) OVERRIDE;

protected:
    ScrollbarAnimationControllerLinearFade(LayerImpl* scrollLayer, base::TimeDelta fadeoutDelay, base::TimeDelta fadeoutLength);

private:
    float opacityAtTime(base::TimeTicks);

    LayerImpl* m_scrollLayer;

    base::TimeTicks m_lastAwakenTime;
    bool m_scrollGestureInProgress;

    base::TimeDelta m_fadeoutDelay;
    base::TimeDelta m_fadeoutLength;

    double m_currentTimeForTesting;
};

} // namespace cc

#endif  // CC_ANIMATION_SCROLLBAR_ANIMATION_CONTROLLER_LINEAR_FADE_H_
