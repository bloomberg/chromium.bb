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

class CC_EXPORT ScrollbarAnimationControllerLinearFade
    : public ScrollbarAnimationController {
 public:
  static scoped_ptr<ScrollbarAnimationControllerLinearFade> Create(
      LayerImpl* scroll_layer,
      base::TimeDelta fadeout_delay,
      base::TimeDelta fadeout_length);

  virtual ~ScrollbarAnimationControllerLinearFade();

  // ScrollbarAnimationController overrides.
  virtual bool IsScrollGestureInProgress() const OVERRIDE;
  virtual bool IsAnimating() const OVERRIDE;
  virtual base::TimeDelta DelayBeforeStart(base::TimeTicks now) const OVERRIDE;

  virtual bool Animate(base::TimeTicks now) OVERRIDE;
  virtual void DidScrollGestureBegin() OVERRIDE;
  virtual void DidScrollGestureEnd(base::TimeTicks now) OVERRIDE;
  virtual void DidProgrammaticallyUpdateScroll(base::TimeTicks now) OVERRIDE;

 protected:
  ScrollbarAnimationControllerLinearFade(LayerImpl* scroll_layer,
                                         base::TimeDelta fadeout_delay,
                                         base::TimeDelta fadeout_length);

 private:
  float OpacityAtTime(base::TimeTicks now);

  LayerImpl* scroll_layer_;

  base::TimeTicks last_awaken_time_;
  bool scroll_gesture_in_progress_;

  base::TimeDelta fadeout_delay_;
  base::TimeDelta fadeout_length_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarAnimationControllerLinearFade);
};

}  // namespace cc

#endif  // CC_ANIMATION_SCROLLBAR_ANIMATION_CONTROLLER_LINEAR_FADE_H_
