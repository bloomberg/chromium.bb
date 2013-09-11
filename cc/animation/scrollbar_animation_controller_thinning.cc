// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scrollbar_animation_controller_thinning.h"

#include <algorithm>

#include "base/time/time.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"

namespace cc {

scoped_ptr<ScrollbarAnimationControllerThinning>
ScrollbarAnimationControllerThinning::Create(LayerImpl* scroll_layer) {
  return make_scoped_ptr(new ScrollbarAnimationControllerThinning(
      scroll_layer,
      base::TimeDelta::FromMilliseconds(500),
      base::TimeDelta::FromMilliseconds(300)));
}

scoped_ptr<ScrollbarAnimationControllerThinning>
ScrollbarAnimationControllerThinning::CreateForTest(LayerImpl* scroll_layer,
    base::TimeDelta animation_delay, base::TimeDelta animation_duration) {
  return make_scoped_ptr(new ScrollbarAnimationControllerThinning(
      scroll_layer, animation_delay, animation_duration));
}

ScrollbarAnimationControllerThinning::ScrollbarAnimationControllerThinning(
    LayerImpl* scroll_layer,
    base::TimeDelta animation_delay,
    base::TimeDelta animation_duration)
    : ScrollbarAnimationController(),
      scroll_layer_(scroll_layer),
      scroll_gesture_in_progress_(false),
      animation_delay_(animation_delay),
      animation_duration_(animation_duration) {}

ScrollbarAnimationControllerThinning::~ScrollbarAnimationControllerThinning() {
}

bool ScrollbarAnimationControllerThinning::IsAnimating() const {
  return !last_awaken_time_.is_null();
}

base::TimeDelta ScrollbarAnimationControllerThinning::DelayBeforeStart(
    base::TimeTicks now) const {
  if (now > last_awaken_time_ + animation_delay_)
    return base::TimeDelta();
  return animation_delay_ - (now - last_awaken_time_);
}

bool ScrollbarAnimationControllerThinning::Animate(base::TimeTicks now) {
  float progress = AnimationProgressAtTime(now);
  float opacity = OpacityAtAnimationProgress(progress);
  float thumb_thickness_scale = ThumbThicknessScaleAtAnimationProgress(
      progress);
  ApplyOpacityAndThumbThicknessScale(opacity, thumb_thickness_scale);
  if (progress == 1.f)
    last_awaken_time_ = base::TimeTicks();
  return IsAnimating() && DelayBeforeStart(now) == base::TimeDelta();
}

void ScrollbarAnimationControllerThinning::DidScrollGestureBegin() {
  ApplyOpacityAndThumbThicknessScale(1, 1);
  last_awaken_time_ = base::TimeTicks();
  scroll_gesture_in_progress_ = true;
}

void ScrollbarAnimationControllerThinning::DidScrollGestureEnd(
    base::TimeTicks now) {
  last_awaken_time_ = now;
  scroll_gesture_in_progress_ = false;
}

bool ScrollbarAnimationControllerThinning::DidScrollUpdate(
    base::TimeTicks now) {
  ApplyOpacityAndThumbThicknessScale(1, 1);

  last_awaken_time_ = now;
  return true;
}

float ScrollbarAnimationControllerThinning::AnimationProgressAtTime(
    base::TimeTicks now) {
  if (scroll_gesture_in_progress_)
    return 0;

  if (last_awaken_time_.is_null())
    return 1;

  base::TimeDelta delta = now - last_awaken_time_;
  float progress = (delta - animation_delay_).InSecondsF() /
      animation_duration_.InSecondsF();
  return std::max(std::min(progress, 1.f), 0.f);
}

float ScrollbarAnimationControllerThinning::OpacityAtAnimationProgress(
    float progress) {
  const float kIdleOpacity = 0.7f;

  return ((1 - kIdleOpacity) * (1.f - progress)) + kIdleOpacity;
}

float
ScrollbarAnimationControllerThinning::ThumbThicknessScaleAtAnimationProgress(
    float progress) {
  const float kIdleThicknessScale = 0.4f;

  return ((1 - kIdleThicknessScale) * (1.f - progress)) + kIdleThicknessScale;
}

void ScrollbarAnimationControllerThinning::ApplyOpacityAndThumbThicknessScale(
    float opacity, float thumb_thickness_scale) {
  ScrollbarLayerImplBase* horizontal_scrollbar =
      scroll_layer_->horizontal_scrollbar_layer();
  if (horizontal_scrollbar) {
    horizontal_scrollbar->SetOpacity(opacity);
    horizontal_scrollbar->set_thumb_thickness_scale_factor(
        thumb_thickness_scale);
  }

  ScrollbarLayerImplBase* vertical_scrollbar =
      scroll_layer_->vertical_scrollbar_layer();
  if (vertical_scrollbar) {
    vertical_scrollbar->SetOpacity(opacity);
    vertical_scrollbar->set_thumb_thickness_scale_factor(thumb_thickness_scale);
  }
}

}  // namespace cc
