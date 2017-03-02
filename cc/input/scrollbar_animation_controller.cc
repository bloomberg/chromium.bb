// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scrollbar_animation_controller.h"

#include <algorithm>

#include "base/time/time.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

std::unique_ptr<ScrollbarAnimationController>
ScrollbarAnimationController::CreateScrollbarAnimationControllerAndroid(
    int scroll_layer_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta fade_out_delay,
    base::TimeDelta fade_out_resize_delay,
    base::TimeDelta fade_out_duration) {
  return base::WrapUnique(new ScrollbarAnimationController(
      scroll_layer_id, client, fade_out_delay, fade_out_resize_delay,
      fade_out_duration));
}

std::unique_ptr<ScrollbarAnimationController>
ScrollbarAnimationController::CreateScrollbarAnimationControllerAuraOverlay(
    int scroll_layer_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta show_delay,
    base::TimeDelta fade_out_delay,
    base::TimeDelta fade_out_resize_delay,
    base::TimeDelta fade_out_duration,
    base::TimeDelta thinning_duration) {
  return base::WrapUnique(new ScrollbarAnimationController(
      scroll_layer_id, client, show_delay, fade_out_delay,
      fade_out_resize_delay, fade_out_duration, thinning_duration));
}

ScrollbarAnimationController::ScrollbarAnimationController(
    int scroll_layer_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta fade_out_delay,
    base::TimeDelta fade_out_resize_delay,
    base::TimeDelta fade_out_duration)
    : client_(client),
      fade_out_delay_(fade_out_delay),
      fade_out_resize_delay_(fade_out_resize_delay),
      need_trigger_scrollbar_show_(false),
      is_animating_(false),
      scroll_layer_id_(scroll_layer_id),
      currently_scrolling_(false),
      scroll_gesture_has_scrolled_(false),
      opacity_(0.0f),
      fade_out_duration_(fade_out_duration),
      need_thinning_animation_(false),
      weak_factory_(this) {
  ApplyOpacityToScrollbars(0.0f);
}

ScrollbarAnimationController::ScrollbarAnimationController(
    int scroll_layer_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta show_delay,
    base::TimeDelta fade_out_delay,
    base::TimeDelta fade_out_resize_delay,
    base::TimeDelta fade_out_duration,
    base::TimeDelta thinning_duration)
    : client_(client),
      show_delay_(show_delay),
      fade_out_delay_(fade_out_delay),
      fade_out_resize_delay_(fade_out_resize_delay),
      need_trigger_scrollbar_show_(false),
      is_animating_(false),
      scroll_layer_id_(scroll_layer_id),
      currently_scrolling_(false),
      scroll_gesture_has_scrolled_(false),
      opacity_(0.0f),
      fade_out_duration_(fade_out_duration),
      need_thinning_animation_(true),
      weak_factory_(this) {
  vertical_controller_ = SingleScrollbarAnimationControllerThinning::Create(
      scroll_layer_id, ScrollbarOrientation::VERTICAL, client,
      thinning_duration);
  horizontal_controller_ = SingleScrollbarAnimationControllerThinning::Create(
      scroll_layer_id, ScrollbarOrientation::HORIZONTAL, client,
      thinning_duration);
  ApplyOpacityToScrollbars(0.0f);
}

ScrollbarAnimationController::~ScrollbarAnimationController() {}

ScrollbarSet ScrollbarAnimationController::Scrollbars() const {
  return client_->ScrollbarsFor(scroll_layer_id_);
}

SingleScrollbarAnimationControllerThinning&
ScrollbarAnimationController::GetScrollbarAnimationController(
    ScrollbarOrientation orientation) const {
  DCHECK(need_thinning_animation_);
  if (orientation == ScrollbarOrientation::VERTICAL)
    return *(vertical_controller_.get());
  else
    return *(horizontal_controller_.get());
}

void ScrollbarAnimationController::StartAnimation() {
  delayed_scrollbar_show_.Cancel();
  delayed_scrollbar_fade_out_.Cancel();
  is_animating_ = true;
  last_awaken_time_ = base::TimeTicks();
  client_->SetNeedsAnimateForScrollbarAnimation();
}

void ScrollbarAnimationController::StopAnimation() {
  delayed_scrollbar_show_.Cancel();
  delayed_scrollbar_fade_out_.Cancel();
  is_animating_ = false;
}

void ScrollbarAnimationController::PostDelayedShow() {
  DCHECK(delayed_scrollbar_fade_out_.IsCancelled());
  delayed_scrollbar_show_.Reset(base::Bind(&ScrollbarAnimationController::Show,
                                           weak_factory_.GetWeakPtr()));
  client_->PostDelayedScrollbarAnimationTask(delayed_scrollbar_show_.callback(),
                                             show_delay_);
}

void ScrollbarAnimationController::PostDelayedFadeOut(bool on_resize) {
  DCHECK(delayed_scrollbar_show_.IsCancelled());
  base::TimeDelta delay = on_resize ? fade_out_resize_delay_ : fade_out_delay_;
  delayed_scrollbar_fade_out_.Reset(
      base::Bind(&ScrollbarAnimationController::StartAnimation,
                 weak_factory_.GetWeakPtr()));
  client_->PostDelayedScrollbarAnimationTask(
      delayed_scrollbar_fade_out_.callback(), delay);
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

  if (need_thinning_animation_) {
    animated |= vertical_controller_->Animate(now);
    animated |= horizontal_controller_->Animate(now);
  }

  return animated;
}

float ScrollbarAnimationController::AnimationProgressAtTime(
    base::TimeTicks now) {
  base::TimeDelta delta = now - last_awaken_time_;
  float progress = delta.InSecondsF() / fade_out_duration_.InSecondsF();
  return std::max(std::min(progress, 1.f), 0.f);
}

void ScrollbarAnimationController::DidScrollBegin() {
  currently_scrolling_ = true;
}

void ScrollbarAnimationController::RunAnimationFrame(float progress) {
  ApplyOpacityToScrollbars(1.f - progress);
  client_->SetNeedsRedrawForScrollbarAnimation();
  if (progress == 1.f)
    StopAnimation();
}

void ScrollbarAnimationController::DidScrollUpdate(bool on_resize) {
  if (need_thinning_animation_ && Captured())
    return;

  StopAnimation();

  // As an optimization, we avoid spamming fade delay tasks during active fast
  // scrolls.  But if we're not within one, we need to post every scroll update.
  if (!currently_scrolling_) {
    // We don't fade out scrollbar if they need thinning animation and mouse is
    // near.
    if (!need_thinning_animation_ || !mouse_is_near_any_scrollbar())
      PostDelayedFadeOut(on_resize);
  } else {
    scroll_gesture_has_scrolled_ = true;
  }

  Show();

  if (need_thinning_animation_) {
    vertical_controller_->UpdateThumbThicknessScale();
    horizontal_controller_->UpdateThumbThicknessScale();
  }
}

void ScrollbarAnimationController::DidScrollEnd() {
  bool has_scrolled = scroll_gesture_has_scrolled_;
  scroll_gesture_has_scrolled_ = false;

  currently_scrolling_ = false;

  // We don't fade out scrollbar if they need thinning animation and mouse is
  // near.
  if (need_thinning_animation_ && mouse_is_near_any_scrollbar())
    return;

  if (has_scrolled)
    PostDelayedFadeOut(false);
}

void ScrollbarAnimationController::DidMouseDown() {
  if (!need_thinning_animation_ || ScrollbarsHidden())
    return;

  vertical_controller_->DidMouseDown();
  horizontal_controller_->DidMouseDown();
}

void ScrollbarAnimationController::DidMouseUp() {
  if (!need_thinning_animation_ || !Captured())
    return;

  vertical_controller_->DidMouseUp();
  horizontal_controller_->DidMouseUp();

  if (!mouse_is_near_any_scrollbar())
    PostDelayedFadeOut(false);
}

void ScrollbarAnimationController::DidMouseLeave() {
  if (!need_thinning_animation_)
    return;

  vertical_controller_->DidMouseLeave();
  horizontal_controller_->DidMouseLeave();

  if (ScrollbarsHidden() || Captured())
    return;

  PostDelayedFadeOut(false);
}

void ScrollbarAnimationController::DidMouseMoveNear(
    ScrollbarOrientation orientation,
    float distance) {
  if (!need_thinning_animation_)
    return;

  bool need_trigger_scrollbar_show_before = need_trigger_scrollbar_show_;

  GetScrollbarAnimationController(orientation).DidMouseMoveNear(distance);

  need_trigger_scrollbar_show_ =
      CalcNeedTriggerScrollbarShow(orientation, distance);

  if (Captured())
    return;

  if (ScrollbarsHidden()) {
    if (need_trigger_scrollbar_show_before != need_trigger_scrollbar_show_) {
      if (need_trigger_scrollbar_show_) {
        PostDelayedShow();
      } else {
        delayed_scrollbar_show_.Cancel();
      }
    }
  } else {
    if (mouse_is_near_any_scrollbar()) {
      Show();
      StopAnimation();
    } else if (!is_animating_) {
      PostDelayedFadeOut(false);
    }
  }
}

bool ScrollbarAnimationController::CalcNeedTriggerScrollbarShow(
    ScrollbarOrientation orientation,
    float distance) const {
  DCHECK(need_thinning_animation_);

  if (vertical_controller_->mouse_is_over_scrollbar() ||
      horizontal_controller_->mouse_is_over_scrollbar())
    return true;

  for (ScrollbarLayerImplBase* scrollbar : Scrollbars()) {
    if (scrollbar->orientation() != orientation)
      continue;

    if (distance <
        (kMouseMoveDistanceToTriggerShow - scrollbar->ThumbThickness()))
      return true;
  }

  return false;
}

bool ScrollbarAnimationController::mouse_is_over_scrollbar(
    ScrollbarOrientation orientation) const {
  DCHECK(need_thinning_animation_);
  return GetScrollbarAnimationController(orientation).mouse_is_over_scrollbar();
}

bool ScrollbarAnimationController::mouse_is_near_scrollbar(
    ScrollbarOrientation orientation) const {
  DCHECK(need_thinning_animation_);
  return GetScrollbarAnimationController(orientation).mouse_is_near_scrollbar();
}

bool ScrollbarAnimationController::mouse_is_near_any_scrollbar() const {
  DCHECK(need_thinning_animation_);
  return vertical_controller_->mouse_is_near_scrollbar() ||
         horizontal_controller_->mouse_is_near_scrollbar();
}

bool ScrollbarAnimationController::ScrollbarsHidden() const {
  return opacity_ == 0.0f;
}

bool ScrollbarAnimationController::Captured() const {
  DCHECK(need_thinning_animation_);
  return vertical_controller_->captured() || horizontal_controller_->captured();
}

void ScrollbarAnimationController::Show() {
  ApplyOpacityToScrollbars(1.0f);
}

void ScrollbarAnimationController::ApplyOpacityToScrollbars(float opacity) {
  for (ScrollbarLayerImplBase* scrollbar : Scrollbars()) {
    if (!scrollbar->is_overlay_scrollbar())
      continue;
    float effective_opacity = scrollbar->CanScrollOrientation() ? opacity : 0;
    PropertyTrees* property_trees =
        scrollbar->layer_tree_impl()->property_trees();
    // If this method is called during LayerImpl::PushPropertiesTo, we may not
    // yet have valid layer_id_to_effect_node_index entries as property trees
    // are pushed after layers during activation. We can skip updating opacity
    // in that case as we are only registering a scrollbar and because opacity
    // will be overwritten anyway when property trees are pushed.
    if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::EFFECT,
                                         scrollbar->id())) {
      property_trees->effect_tree.OnOpacityAnimated(
          effective_opacity,
          property_trees->layer_id_to_effect_node_index[scrollbar->id()],
          scrollbar->layer_tree_impl());
    }
  }

  bool previouslyVisible = opacity_ > 0.0f;
  bool currentlyVisible = opacity > 0.0f;

  opacity_ = opacity;

  if (previouslyVisible != currentlyVisible)
    client_->DidChangeScrollbarVisibility();
}

}  // namespace cc
