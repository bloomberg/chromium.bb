// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scrollbar_animation_controller_linear_fade.h"

#include "base/time/time.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"

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
      scroll_gesture_has_scrolled_(false),
      fadeout_delay_(fadeout_delay),
      fadeout_length_(fadeout_length) {}

ScrollbarAnimationControllerLinearFade::
    ~ScrollbarAnimationControllerLinearFade() {}

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
  ApplyOpacityToScrollbars(opacity);
  if (!opacity)
    last_awaken_time_ = base::TimeTicks();
  return IsAnimating() && DelayBeforeStart(now) == base::TimeDelta();
}

void ScrollbarAnimationControllerLinearFade::DidScrollGestureBegin() {
  scroll_gesture_in_progress_ = true;
  scroll_gesture_has_scrolled_ = false;
}

void ScrollbarAnimationControllerLinearFade::DidScrollGestureEnd(
    base::TimeTicks now) {
  // The animation should not be triggered if no scrolling has occurred.
  if (scroll_gesture_has_scrolled_)
    last_awaken_time_ = now;
  scroll_gesture_has_scrolled_ = false;
  scroll_gesture_in_progress_ = false;
}

void ScrollbarAnimationControllerLinearFade::DidMouseMoveOffScrollbar(
    base::TimeTicks now) {
  // Ignore mouse move events.
}

bool ScrollbarAnimationControllerLinearFade::DidScrollUpdate(
    base::TimeTicks now) {
  ApplyOpacityToScrollbars(1.0f);
  // The animation should only be activated if the scroll updated occurred
  // programatically, outside the scope of a scroll gesture.
  if (scroll_gesture_in_progress_) {
    last_awaken_time_ = base::TimeTicks();
    scroll_gesture_has_scrolled_ = true;
    return false;
  }

  last_awaken_time_ = now;
  return true;
}

bool ScrollbarAnimationControllerLinearFade::DidMouseMoveNear(
    base::TimeTicks now, float distance) {
  // Ignore mouse move events.
  return false;
}

float ScrollbarAnimationControllerLinearFade::OpacityAtTime(
    base::TimeTicks now) {
  if (scroll_gesture_has_scrolled_)
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

void ScrollbarAnimationControllerLinearFade::ApplyOpacityToScrollbars(
    float opacity) {
  if (!scroll_layer_->scrollbars())
    return;

  LayerImpl::ScrollbarSet* scrollbars = scroll_layer_->scrollbars();
  for (LayerImpl::ScrollbarSet::iterator it = scrollbars->begin();
       it != scrollbars->end();
       ++it) {
    ScrollbarLayerImplBase* scrollbar = *it;
    if (scrollbar->is_overlay_scrollbar())
      scrollbar->SetOpacity(opacity);
  }
}

}  // namespace cc
