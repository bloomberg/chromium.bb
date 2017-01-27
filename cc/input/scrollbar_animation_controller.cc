// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scrollbar_animation_controller.h"

#include <algorithm>

#include "base/time/time.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

ScrollbarAnimationController::ScrollbarAnimationController(
    int scroll_layer_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta delay_before_starting,
    base::TimeDelta resize_delay_before_starting)
    : client_(client),
      delay_before_starting_(delay_before_starting),
      resize_delay_before_starting_(resize_delay_before_starting),
      is_animating_(false),
      scroll_layer_id_(scroll_layer_id),
      currently_scrolling_(false),
      scroll_gesture_has_scrolled_(false),
      weak_factory_(this) {
}

ScrollbarAnimationController::~ScrollbarAnimationController() {}

SingleScrollbarAnimationControllerThinning&
ScrollbarAnimationController::GetScrollbarAnimationController(
    ScrollbarOrientation orientation) const {
  DCHECK(NeedThinningAnimation());
  if (orientation == ScrollbarOrientation::VERTICAL)
    return *(vertical_controller_.get());
  else
    return *(horizontal_controller_.get());
}

bool ScrollbarAnimationController::Animate(base::TimeTicks now) {
  bool animated = false;

  if (is_animating_) {
    if (last_awaken_time_.is_null())
      last_awaken_time_ = now;

    float progress = AnimationProgressAtTime(now);
    RunAnimationFrame(progress);

    if (is_animating_)
      client_->SetNeedsAnimateForScrollbarAnimation();
    animated = true;
  }

  if (NeedThinningAnimation()) {
    animated |= vertical_controller_->Animate(now);
    animated |= horizontal_controller_->Animate(now);
  }

  return animated;
}

bool ScrollbarAnimationController::ScrollbarsHidden() const {
  return false;
}

bool ScrollbarAnimationController::NeedThinningAnimation() const {
  return false;
}

float ScrollbarAnimationController::AnimationProgressAtTime(
    base::TimeTicks now) {
  base::TimeDelta delta = now - last_awaken_time_;
  float progress = delta.InSecondsF() / Duration().InSecondsF();
  return std::max(std::min(progress, 1.f), 0.f);
}

void ScrollbarAnimationController::DidScrollBegin() {
  currently_scrolling_ = true;
}

void ScrollbarAnimationController::DidScrollUpdate(bool on_resize) {
  StopAnimation();

  // As an optimization, we avoid spamming fade delay tasks during active fast
  // scrolls.  But if we're not within one, we need to post every scroll update.
  if (!currently_scrolling_)
    PostDelayedAnimationTask(on_resize);
  else
    scroll_gesture_has_scrolled_ = true;
}

void ScrollbarAnimationController::DidScrollEnd() {
  if (scroll_gesture_has_scrolled_) {
    PostDelayedAnimationTask(false);
    scroll_gesture_has_scrolled_ = false;
  }

  currently_scrolling_ = false;
}

void ScrollbarAnimationController::DidMouseDown() {
  if (!NeedThinningAnimation() || ScrollbarsHidden())
    return;

  vertical_controller_->DidMouseDown();
  horizontal_controller_->DidMouseDown();
}

void ScrollbarAnimationController::DidMouseUp() {
  if (!NeedThinningAnimation())
    return;

  vertical_controller_->DidMouseUp();
  horizontal_controller_->DidMouseUp();

  if (!mouse_is_near_any_scrollbar())
    PostDelayedAnimationTask(false);
}

void ScrollbarAnimationController::DidMouseLeave() {
  if (!NeedThinningAnimation())
    return;

  vertical_controller_->DidMouseLeave();
  horizontal_controller_->DidMouseLeave();
}

void ScrollbarAnimationController::DidMouseMoveNear(
    ScrollbarOrientation orientation,
    float distance) {
  if (!NeedThinningAnimation())
    return;

  GetScrollbarAnimationController(orientation).DidMouseMoveNear(distance);

  if (ScrollbarsHidden() || Captured())
    return;

  if (mouse_is_near_any_scrollbar()) {
    ApplyOpacityToScrollbars(1);
    StopAnimation();
  } else if (!animating_fade()) {
    PostDelayedAnimationTask(false);
  }
}

bool ScrollbarAnimationController::mouse_is_over_scrollbar(
    ScrollbarOrientation orientation) const {
  DCHECK(NeedThinningAnimation());
  return GetScrollbarAnimationController(orientation).mouse_is_over_scrollbar();
}

bool ScrollbarAnimationController::mouse_is_near_scrollbar(
    ScrollbarOrientation orientation) const {
  DCHECK(NeedThinningAnimation());
  return GetScrollbarAnimationController(orientation).mouse_is_near_scrollbar();
}

bool ScrollbarAnimationController::mouse_is_near_any_scrollbar() const {
  DCHECK(NeedThinningAnimation());
  return vertical_controller_->mouse_is_near_scrollbar() ||
         horizontal_controller_->mouse_is_near_scrollbar();
}

bool ScrollbarAnimationController::Captured() const {
  DCHECK(NeedThinningAnimation());
  return vertical_controller_->captured() || horizontal_controller_->captured();
}

void ScrollbarAnimationController::PostDelayedAnimationTask(bool on_resize) {
  base::TimeDelta delay =
      on_resize ? resize_delay_before_starting_ : delay_before_starting_;
  delayed_scrollbar_fade_.Reset(
      base::Bind(&ScrollbarAnimationController::StartAnimation,
                 weak_factory_.GetWeakPtr()));
  client_->PostDelayedScrollbarAnimationTask(delayed_scrollbar_fade_.callback(),
                                             delay);
}

void ScrollbarAnimationController::StartAnimation() {
  delayed_scrollbar_fade_.Cancel();
  is_animating_ = true;
  last_awaken_time_ = base::TimeTicks();
  client_->SetNeedsAnimateForScrollbarAnimation();
}

void ScrollbarAnimationController::StopAnimation() {
  delayed_scrollbar_fade_.Cancel();
  is_animating_ = false;
}

ScrollbarSet ScrollbarAnimationController::Scrollbars() const {
  return client_->ScrollbarsFor(scroll_layer_id_);
}

void ScrollbarAnimationController::set_mouse_move_distance_for_test(
    float distance) {
  vertical_controller_->set_mouse_move_distance_for_test(distance);
  horizontal_controller_->set_mouse_move_distance_for_test(distance);
}

}  // namespace cc
