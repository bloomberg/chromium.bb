// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLLBAR_ANIMATION_CONTROLLER_THINNING_H_
#define CC_ANIMATION_SCROLLBAR_ANIMATION_CONTROLLER_THINNING_H_

#include "base/memory/scoped_ptr.h"
#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/base/cc_export.h"

namespace cc {
class LayerImpl;

// Scrollbar animation that partially fades and thins after an idle delay.
class CC_EXPORT ScrollbarAnimationControllerThinning
    : public ScrollbarAnimationController {
 public:
  static scoped_ptr<ScrollbarAnimationControllerThinning> Create(
      LayerImpl* scroll_layer);

  static scoped_ptr<ScrollbarAnimationControllerThinning> CreateForTest(
      LayerImpl* scroll_layer,
      base::TimeDelta animation_delay,
      base::TimeDelta animation_duration);

  virtual ~ScrollbarAnimationControllerThinning();

  // ScrollbarAnimationController overrides.
  virtual bool IsAnimating() const OVERRIDE;
  virtual base::TimeDelta DelayBeforeStart(base::TimeTicks now) const OVERRIDE;

  virtual bool Animate(base::TimeTicks now) OVERRIDE;
  virtual void DidScrollGestureBegin() OVERRIDE;
  virtual void DidScrollGestureEnd(base::TimeTicks now) OVERRIDE;
  virtual bool DidScrollUpdate(base::TimeTicks now) OVERRIDE;

 protected:
  ScrollbarAnimationControllerThinning(LayerImpl* scroll_layer,
                                       base::TimeDelta animation_delay,
                                       base::TimeDelta animation_duration);

 private:
  // Returns how far through the animation we are as a progress value from
  // 0 to 1.
  float AnimationProgressAtTime(base::TimeTicks now);
  float OpacityAtAnimationProgress(float progress);
  float ThumbThicknessScaleAtAnimationProgress(float progress);
  void ApplyOpacityAndThumbThicknessScale(float opacity,
                                          float thumb_thickness_scale);

  LayerImpl* scroll_layer_;

  base::TimeTicks last_awaken_time_;
  bool scroll_gesture_in_progress_;

  base::TimeDelta animation_delay_;
  base::TimeDelta animation_duration_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarAnimationControllerThinning);
};

}  // namespace cc

#endif  // CC_ANIMATION_SCROLLBAR_ANIMATION_CONTROLLER_THINNING_H_
