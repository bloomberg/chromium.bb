// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLLBAR_ANIMATION_CONTROLLER_H_
#define CC_ANIMATION_SCROLLBAR_ANIMATION_CONTROLLER_H_

#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {

// This abstract class represents the compositor-side analogy of
// ScrollbarAnimator.  Individual platforms should subclass it to provide
// specialized implementation.
class CC_EXPORT ScrollbarAnimationController {
 public:
  virtual ~ScrollbarAnimationController() {}

  virtual bool IsAnimating() const = 0;
  virtual base::TimeDelta DelayBeforeStart(base::TimeTicks now) const = 0;

  virtual bool Animate(base::TimeTicks now) = 0;
  virtual void DidScrollGestureBegin() = 0;
  virtual void DidScrollGestureEnd(base::TimeTicks now) = 0;
  virtual void DidMouseMoveOffScrollbar(base::TimeTicks now) = 0;

  // Returns true if we should start an animation.
  virtual bool DidScrollUpdate(base::TimeTicks now) = 0;
  virtual bool DidMouseMoveNear(base::TimeTicks now, float distance) = 0;
};

}  // namespace cc

#endif  // CC_ANIMATION_SCROLLBAR_ANIMATION_CONTROLLER_H_
