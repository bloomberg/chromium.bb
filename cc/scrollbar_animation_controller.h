// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCROLLBAR_ANIMATION_CONTROLLER_H_
#define CC_SCROLLBAR_ANIMATION_CONTROLLER_H_

#include "base/time.h"
#include "cc/cc_export.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {

// This abstract class represents the compositor-side analogy of ScrollbarAnimator.
// Individual platforms should subclass it to provide specialized implementation.
class CC_EXPORT ScrollbarAnimationController {
public:
    virtual ~ScrollbarAnimationController() {}

    virtual bool animate(base::TimeTicks) = 0;
    virtual void didPinchGestureUpdate(base::TimeTicks) = 0;
    virtual void didPinchGestureEnd(base::TimeTicks) = 0;
    virtual void didUpdateScrollOffset(base::TimeTicks) = 0;

};

} // namespace cc

#endif  // CC_SCROLLBAR_ANIMATION_CONTROLLER_H_
