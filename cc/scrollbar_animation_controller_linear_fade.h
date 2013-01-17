// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCROLLBAR_ANIMATION_CONTROLLER_LINEAR_FADE_H_
#define CC_SCROLLBAR_ANIMATION_CONTROLLER_LINEAR_FADE_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/scrollbar_animation_controller.h"

namespace cc {
class LayerImpl;

class CC_EXPORT ScrollbarAnimationControllerLinearFade : public ScrollbarAnimationController {
public:
    static scoped_ptr<ScrollbarAnimationControllerLinearFade> create(LayerImpl* scrollLayer, double fadeoutDelay, double fadeoutLength);

    virtual ~ScrollbarAnimationControllerLinearFade();

    // ScrollbarAnimationController overrides.
    virtual bool animate(base::TimeTicks) OVERRIDE;
    virtual void didPinchGestureUpdate(base::TimeTicks) OVERRIDE;
    virtual void didPinchGestureEnd(base::TimeTicks) OVERRIDE;
    virtual void didUpdateScrollOffset(base::TimeTicks) OVERRIDE;

protected:
    ScrollbarAnimationControllerLinearFade(LayerImpl* scrollLayer, double fadeoutDelay, double fadeoutLength);

private:
    float opacityAtTime(base::TimeTicks);

    LayerImpl* m_scrollLayer;

    base::TimeTicks m_lastAwakenTime;
    bool m_pinchGestureInEffect;

    double m_fadeoutDelay;
    double m_fadeoutLength;

    double m_currentTimeForTesting;
};

} // namespace cc

#endif  // CC_SCROLLBAR_ANIMATION_CONTROLLER_LINEAR_FADE_H_
