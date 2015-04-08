// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/overscroll_window_animation.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "content/browser/web_contents/aura/shadow_layer_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace content {

namespace {

OverscrollWindowAnimation::Direction GetDirectionForMode(OverscrollMode mode) {
  if (mode == (base::i18n::IsRTL() ? OVERSCROLL_EAST : OVERSCROLL_WEST))
    return OverscrollWindowAnimation::SLIDE_FRONT;
  if (mode == (base::i18n::IsRTL() ? OVERSCROLL_WEST : OVERSCROLL_EAST))
    return OverscrollWindowAnimation::SLIDE_BACK;
  return OverscrollWindowAnimation::SLIDE_NONE;
}

}  // namespace

OverscrollWindowAnimation::OverscrollWindowAnimation(Delegate* delegate)
    : delegate_(delegate),
      direction_(SLIDE_NONE),
      overscroll_cancelled_(false) {
  DCHECK(delegate_);
}

OverscrollWindowAnimation::~OverscrollWindowAnimation() {
}

void OverscrollWindowAnimation::CancelSlide() {
  ui::Layer* layer = GetFrontLayer();
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTweenType(gfx::Tween::EASE_OUT);
  settings.AddObserver(this);
  layer->SetTransform(gfx::Transform());
  overscroll_cancelled_ = true;
}

float OverscrollWindowAnimation::GetTranslationForOverscroll(float delta_x) {
  DCHECK(direction_ != SLIDE_NONE);
  const float bounds_width = GetVisibleBounds().width();
  if (direction_ == SLIDE_FRONT)
    return std::max(-bounds_width, delta_x);
  else
    return std::min(bounds_width, delta_x);
}

gfx::Rect OverscrollWindowAnimation::GetVisibleBounds() const {
  return delegate_->GetMainWindow()->bounds();
}

bool OverscrollWindowAnimation::OnOverscrollUpdate(float delta_x,
                                                   float delta_y) {
  if (direction_ == SLIDE_NONE)
    return false;
  gfx::Transform transform;
  transform.Translate(GetTranslationForOverscroll(delta_x), 0);
  GetFrontLayer()->SetTransform(transform);
  return true;
}

void OverscrollWindowAnimation::OnImplicitAnimationsCompleted() {
  if (overscroll_cancelled_) {
    slide_window_.reset();
    delegate_->OnOverscrollCancelled();
    overscroll_cancelled_ = false;
  } else {
    delegate_->OnOverscrollCompleted(slide_window_.Pass());
  }
  direction_ = SLIDE_NONE;
}

void OverscrollWindowAnimation::OnOverscrollModeChange(
    OverscrollMode old_mode,
    OverscrollMode new_mode) {
  Direction new_direction = GetDirectionForMode(new_mode);
  if (new_direction == SLIDE_NONE) {
    // The user cancelled the in progress animation.
    if (is_active())
      CancelSlide();
    return;
  }
  if (is_active())
    GetFrontLayer()->GetAnimator()->StopAnimating();

  gfx::Rect slide_window_bounds = gfx::Rect(GetVisibleBounds().size());
  if (new_direction == SLIDE_FRONT) {
    slide_window_bounds.Offset(base::i18n::IsRTL()
                                   ? -slide_window_bounds.width()
                                   : slide_window_bounds.width(),
                               0);
  }
  slide_window_ = new_direction == SLIDE_FRONT
                      ? delegate_->CreateFrontWindow(slide_window_bounds)
                      : delegate_->CreateBackWindow(slide_window_bounds);
  if (!slide_window_) {
    // Cannot navigate, do not start an overscroll gesture.
    direction_ = SLIDE_NONE;
    return;
  }
  overscroll_cancelled_ = false;
  direction_ = new_direction;
  shadow_.reset(new ShadowLayerDelegate(GetFrontLayer()));
}

void OverscrollWindowAnimation::OnOverscrollComplete(
    OverscrollMode overscroll_mode) {
  if (!is_active())
    return;
  delegate_->OnOverscrollCompleting();
  int content_width = GetVisibleBounds().width();
  float translate_x;
  if ((base::i18n::IsRTL() && direction_ == SLIDE_FRONT) ||
      (!base::i18n::IsRTL() && direction_ == SLIDE_BACK)) {
    translate_x = content_width;
  } else {
    translate_x = -content_width;
  }
  ui::Layer* layer = GetFrontLayer();
  gfx::Transform transform;
  transform.Translate(translate_x, 0);
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTweenType(gfx::Tween::EASE_OUT);
  settings.AddObserver(this);
  layer->SetTransform(transform);
}

ui::Layer* OverscrollWindowAnimation::GetFrontLayer() const {
  DCHECK(direction_ != SLIDE_NONE);
  if (direction_ == SLIDE_FRONT) {
    DCHECK(slide_window_);
    return slide_window_->layer();
  }
  return delegate_->GetMainWindow()->layer();
}

}  // namespace content
