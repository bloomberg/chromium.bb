// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scrollbar_animation_controller_linear_fade.h"

#include "base/time.h"
#include "cc/layers/layer_impl.h"

namespace cc {

scoped_ptr<ScrollbarAnimationControllerLinearFade>
ScrollbarAnimationControllerLinearFade::Create(LayerImpl* scroll_layer,
                                               base::TimeDelta fadeout_delay,
                                               base::TimeDelta fadeout_length) {
  return make_scoped_ptr(new ScrollbarAnimationControllerLinearFade(
      scroll_layer, fadeout_delay, fadeout_length));
}

ScrollbarAnimationControllerLinearFade::ScrollbarAnimationControllerLinearFade(
    LayerImpl* scroll_layer,
    base::TimeDelta fadeout_delay,
    base::TimeDelta fadeout_length)
    : ScrollbarAnimationController(),
      scroll_layer_(scroll_layer),
      scroll_gesture_in_progress_(false),
      fadeout_delay_(fadeout_delay),
      fadeout_length_(fadeout_length) {}

ScrollbarAnimationControllerLinearFade::
    ~ScrollbarAnimationControllerLinearFade() {}

bool ScrollbarAnimationControllerLinearFade::IsScrollGestureInProgress() const {
  return scroll_gesture_in_progress_;
}

bool ScrollbarAnimationControllerLinearFade::IsAnimating() const {
  return !last_awaken_time_.is_null();
}

base::TimeDelta ScrollbarAnimationControllerLinearFade::DelayBeforeStart(
    base::TimeTicks now) const {
  if (now > last_awaken_time_ + fadeout_delay_)
    return base::TimeDelta();
  return fadeout_delay_ - (now - last_awaken_time_);
}

bool ScrollbarAnimationControllerLinearFade::Animate(base::TimeTicks now) {
  float opacity = OpacityAtTime(now);
  scroll_layer_->SetScrollbarOpacity(opacity);
  if (!opacity)
    last_awaken_time_ = base::TimeTicks();
  return IsAnimating() && DelayBeforeStart(now) == base::TimeDelta();
}

void ScrollbarAnimationControllerLinearFade::DidScrollGestureBegin() {
  scroll_layer_->SetScrollbarOpacity(1.0f);
  scroll_gesture_in_progress_ = true;
  last_awaken_time_ = base::TimeTicks();
}

void ScrollbarAnimationControllerLinearFade::DidScrollGestureEnd(
    base::TimeTicks now) {
  scroll_gesture_in_progress_ = false;
  last_awaken_time_ = now;
}

void ScrollbarAnimationControllerLinearFade::DidProgrammaticallyUpdateScroll(
    base::TimeTicks now) {
  // Don't set scroll_gesture_in_progress_ as this scroll is not from a gesture
  // and we won't receive ScrollEnd.
  scroll_layer_->SetScrollbarOpacity(1.0f);
  last_awaken_time_ = now;
}

float ScrollbarAnimationControllerLinearFade::OpacityAtTime(
    base::TimeTicks now) {
  if (scroll_gesture_in_progress_)
    return 1.0f;

  if (last_awaken_time_.is_null())
    return 0.0f;

  base::TimeDelta delta = now - last_awaken_time_;

  if (delta <= fadeout_delay_)
    return 1.0f;
  if (delta < fadeout_delay_ + fadeout_length_) {
    return (fadeout_delay_ + fadeout_length_ - delta).InSecondsF() /
           fadeout_length_.InSecondsF();
  }
  return 0.0f;
}

}  // namespace cc
