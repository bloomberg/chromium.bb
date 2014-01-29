// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scrollbar_animation_controller_thinning.h"

#include <algorithm>

#include "base/time/time.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"

namespace {
const float kIdleThicknessScale = 0.4f;
const float kIdleOpacity = 0.7f;
const float kDefaultMouseMoveDistanceToTriggerAnimation = 25.f;
const int kDefaultAnimationDelay = 500;
const int kDefaultAnimationDuration = 300;
}

namespace cc {

scoped_ptr<ScrollbarAnimationControllerThinning>
ScrollbarAnimationControllerThinning::Create(LayerImpl* scroll_layer) {
  return make_scoped_ptr(new ScrollbarAnimationControllerThinning(
      scroll_layer,
      base::TimeDelta::FromMilliseconds(kDefaultAnimationDelay),
      base::TimeDelta::FromMilliseconds(kDefaultAnimationDuration)));
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
      mouse_is_over_scrollbar_(false),
      mouse_is_near_scrollbar_(false),
      thickness_change_(NONE),
      opacity_change_(NONE),
      should_delay_animation_(false),
      animation_delay_(animation_delay),
      animation_duration_(animation_duration),
      mouse_move_distance_to_trigger_animation_(
          kDefaultMouseMoveDistanceToTriggerAnimation) {
  ApplyOpacityAndThumbThicknessScale(kIdleOpacity, kIdleThicknessScale);
}

ScrollbarAnimationControllerThinning::~ScrollbarAnimationControllerThinning() {
}

bool ScrollbarAnimationControllerThinning::IsAnimating() const {
  return !last_awaken_time_.is_null();
}

base::TimeDelta ScrollbarAnimationControllerThinning::DelayBeforeStart(
    base::TimeTicks now) const {
  if (!should_delay_animation_)
    return base::TimeDelta();
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
  if (progress == 1.f) {
    opacity_change_ = NONE;
    thickness_change_ = NONE;
    last_awaken_time_ = base::TimeTicks();
  }
  return IsAnimating() && DelayBeforeStart(now) == base::TimeDelta();
}

void ScrollbarAnimationControllerThinning::DidScrollGestureBegin() {
}

void ScrollbarAnimationControllerThinning::DidScrollGestureEnd(
    base::TimeTicks now) {
}

void ScrollbarAnimationControllerThinning::DidMouseMoveOffScrollbar(
    base::TimeTicks now) {
  mouse_is_over_scrollbar_ = false;
  mouse_is_near_scrollbar_ = false;
  last_awaken_time_ = now;
  should_delay_animation_ = false;
  opacity_change_ = DECREASE;
  thickness_change_ = DECREASE;
}

bool ScrollbarAnimationControllerThinning::DidScrollUpdate(
    base::TimeTicks now) {
  ApplyOpacityAndThumbThicknessScale(
    1, mouse_is_near_scrollbar_ ? 1.f : kIdleThicknessScale);

  last_awaken_time_ = now;
  should_delay_animation_ = true;
  if (!mouse_is_over_scrollbar_)
    opacity_change_ = DECREASE;
  return true;
}

bool ScrollbarAnimationControllerThinning::DidMouseMoveNear(
    base::TimeTicks now, float distance) {
  bool mouse_is_over_scrollbar = distance == 0.0;
  bool mouse_is_near_scrollbar =
      distance < mouse_move_distance_to_trigger_animation_;

  if (mouse_is_over_scrollbar == mouse_is_over_scrollbar_ &&
      mouse_is_near_scrollbar == mouse_is_near_scrollbar_)
    return false;

  if (mouse_is_over_scrollbar_ != mouse_is_over_scrollbar) {
    mouse_is_over_scrollbar_ = mouse_is_over_scrollbar;
    opacity_change_ = mouse_is_over_scrollbar_ ? INCREASE : DECREASE;
  }

  if (mouse_is_near_scrollbar_ != mouse_is_near_scrollbar) {
    mouse_is_near_scrollbar_ = mouse_is_near_scrollbar;
    thickness_change_ = mouse_is_near_scrollbar_ ? INCREASE : DECREASE;
  }

  last_awaken_time_ = now;
  should_delay_animation_ = false;
  return true;
}

float ScrollbarAnimationControllerThinning::AnimationProgressAtTime(
    base::TimeTicks now) {
  if (last_awaken_time_.is_null())
    return 1;

  base::TimeDelta delta = now - last_awaken_time_;
  if (should_delay_animation_)
    delta -= animation_delay_;
  float progress = delta.InSecondsF() / animation_duration_.InSecondsF();
  return std::max(std::min(progress, 1.f), 0.f);
}

float ScrollbarAnimationControllerThinning::OpacityAtAnimationProgress(
    float progress) {
  if (opacity_change_ == NONE)
    return mouse_is_over_scrollbar_ ? 1.f : kIdleOpacity;
  float factor = opacity_change_ == INCREASE ? progress : (1.f - progress);
  float ret = ((1.f - kIdleOpacity) * factor) + kIdleOpacity;
  return ret;
}

float
ScrollbarAnimationControllerThinning::ThumbThicknessScaleAtAnimationProgress(
    float progress) {
  if (thickness_change_ == NONE)
    return mouse_is_near_scrollbar_ ? 1.f : kIdleThicknessScale;
  float factor = thickness_change_ == INCREASE ? progress : (1.f - progress);
  return ((1.f - kIdleThicknessScale) * factor) + kIdleThicknessScale;
}

float ScrollbarAnimationControllerThinning::AdjustScale(
    float new_value,
    float current_value,
    AnimationChange animation_change) {
  if (animation_change == INCREASE && current_value > new_value)
    return current_value;
  if (animation_change == DECREASE && current_value < new_value)
    return current_value;
  return new_value;
}

void ScrollbarAnimationControllerThinning::ApplyOpacityAndThumbThicknessScale(
    float opacity, float thumb_thickness_scale) {
  ScrollbarLayerImplBase* horizontal_scrollbar =
      scroll_layer_->horizontal_scrollbar_layer();
  if (horizontal_scrollbar) {
    horizontal_scrollbar->SetOpacity(
        AdjustScale(opacity, horizontal_scrollbar->opacity(), opacity_change_));
    horizontal_scrollbar->SetThumbThicknessScaleFactor(
        AdjustScale(
          thumb_thickness_scale,
          horizontal_scrollbar->thumb_thickness_scale_factor(),
          thickness_change_));
  }

  ScrollbarLayerImplBase* vertical_scrollbar =
      scroll_layer_->vertical_scrollbar_layer();
  if (vertical_scrollbar) {
    vertical_scrollbar->SetOpacity(
        AdjustScale(opacity, vertical_scrollbar->opacity(), opacity_change_));
    vertical_scrollbar->SetThumbThicknessScaleFactor(
        AdjustScale(
          thumb_thickness_scale,
          vertical_scrollbar->thumb_thickness_scale_factor(),
          thickness_change_));
  }
}

}  // namespace cc
