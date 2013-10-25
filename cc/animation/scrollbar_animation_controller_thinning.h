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

  void set_mouse_move_distance_for_test(float distance) {
    mouse_move_distance_to_trigger_animation_ = distance;
  }
  bool mouse_is_over_scrollbar() const { return mouse_is_over_scrollbar_; }
  bool mouse_is_near_scrollbar() const { return mouse_is_near_scrollbar_; }

  // ScrollbarAnimationController overrides.
  virtual bool IsAnimating() const OVERRIDE;
  virtual base::TimeDelta DelayBeforeStart(base::TimeTicks now) const OVERRIDE;

  virtual bool Animate(base::TimeTicks now) OVERRIDE;
  virtual void DidScrollGestureBegin() OVERRIDE;
  virtual void DidScrollGestureEnd(base::TimeTicks now) OVERRIDE;
  virtual void DidMouseMoveOffScrollbar(base::TimeTicks now) OVERRIDE;
  virtual bool DidScrollUpdate(base::TimeTicks now) OVERRIDE;
  virtual bool DidMouseMoveNear(base::TimeTicks now, float distance) OVERRIDE;

 protected:
  ScrollbarAnimationControllerThinning(LayerImpl* scroll_layer,
                                       base::TimeDelta animation_delay,
                                       base::TimeDelta animation_duration);

 private:
  // Describes whether the current animation should INCREASE (darken / thicken)
  // a bar or DECREASE it (lighten / thin).
  enum AnimationChange {
    NONE,
    INCREASE,
    DECREASE
  };
  // Returns how far through the animation we are as a progress value from
  // 0 to 1.
  float AnimationProgressAtTime(base::TimeTicks now);
  float OpacityAtAnimationProgress(float progress);
  float ThumbThicknessScaleAtAnimationProgress(float progress);
  float AdjustScale(float new_value,
                    float current_value,
                    AnimationChange animation_change);
  void ApplyOpacityAndThumbThicknessScale(float opacity,
                                          float thumb_thickness_scale);

  LayerImpl* scroll_layer_;

  base::TimeTicks last_awaken_time_;
  bool mouse_is_over_scrollbar_;
  bool mouse_is_near_scrollbar_;
  // Are we narrowing or thickening the bars.
  AnimationChange thickness_change_;
  // Are we darkening or lightening the bars.
  AnimationChange opacity_change_;
  // Should the animation be delayed or start immediately.
  bool should_delay_animation_;
  // If |should_delay_animation_| is true, delay the animation by this amount.
  base::TimeDelta animation_delay_;
  // The time for the animation to run.
  base::TimeDelta animation_duration_;
  // How close should the mouse be to the scrollbar before we thicken it.
  float mouse_move_distance_to_trigger_animation_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarAnimationControllerThinning);
};

}  // namespace cc

#endif  // CC_ANIMATION_SCROLLBAR_ANIMATION_CONTROLLER_THINNING_H_
