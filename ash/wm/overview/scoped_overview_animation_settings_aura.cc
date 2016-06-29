// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_animation_settings_aura.h"

#include "ash/common/material_design/material_design_controller.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"

namespace ash {

namespace {

// The time duration for transformation animations.
const int kTransitionMilliseconds = 200;

// The time duration for fading out when closing an item. Only used with
// Material Design.
const int kCloseFadeOutMillisecondsMd = 50;

// The time duration for scaling down when an item is closed. Only used with
// Material Design.
const int kCloseScaleMillisecondsMd = 100;

// The time duration for widgets to fade in.
const int kFadeInMilliseconds = 80;

base::TimeDelta GetAnimationDuration(OverviewAnimationType animation_type) {
  switch (animation_type) {
    case OVERVIEW_ANIMATION_NONE:
      return base::TimeDelta();
    case OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN:
      return base::TimeDelta::FromMilliseconds(kFadeInMilliseconds);
    case OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS:
    case OVERVIEW_ANIMATION_RESTORE_WINDOW:
    case OVERVIEW_ANIMATION_HIDE_WINDOW:
      return base::TimeDelta::FromMilliseconds(kTransitionMilliseconds);
    case OVERVIEW_ANIMATION_CLOSING_SELECTOR_ITEM:
      return base::TimeDelta::FromMilliseconds(kCloseScaleMillisecondsMd);
    case OVERVIEW_ANIMATION_CLOSE_SELECTOR_ITEM:
      return base::TimeDelta::FromMilliseconds(kCloseFadeOutMillisecondsMd);
  }
  NOTREACHED();
  return base::TimeDelta();
}

}  // namespace

ScopedOverviewAnimationSettingsAura::ScopedOverviewAnimationSettingsAura(
    OverviewAnimationType animation_type,
    aura::Window* window)
    : animation_settings_(window->layer()->GetAnimator()) {
  switch (animation_type) {
    case OVERVIEW_ANIMATION_NONE:
      animation_settings_.SetPreemptionStrategy(
          ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
      break;
    case OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN:
      window->layer()->GetAnimator()->SchedulePauseForProperties(
          GetAnimationDuration(
              OverviewAnimationType::OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS),
          ui::LayerAnimationElement::OPACITY);
      animation_settings_.SetPreemptionStrategy(
          ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
      break;
    case OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS:
    case OVERVIEW_ANIMATION_RESTORE_WINDOW:
      animation_settings_.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      animation_settings_.SetTweenType(
          ash::MaterialDesignController::IsOverviewMaterial()
              ? gfx::Tween::EASE_IN_2
              : gfx::Tween::FAST_OUT_SLOW_IN);
      break;
    case OVERVIEW_ANIMATION_CLOSING_SELECTOR_ITEM:
    case OVERVIEW_ANIMATION_CLOSE_SELECTOR_ITEM:
      animation_settings_.SetPreemptionStrategy(
          ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
      animation_settings_.SetTweenType(gfx::Tween::EASE_OUT);
      break;
    case OVERVIEW_ANIMATION_HIDE_WINDOW:
      animation_settings_.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
  }
  animation_settings_.SetTransitionDuration(
      GetAnimationDuration(animation_type));
}

ScopedOverviewAnimationSettingsAura::~ScopedOverviewAnimationSettingsAura() {}

}  // namespace ash
