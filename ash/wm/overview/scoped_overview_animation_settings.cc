// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_animation_settings.h"

#include "ash/wm/overview/overview_animation_type.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"

namespace ash {

namespace {

// The time duration for transformation animations.
const int kTransitionMilliseconds = 200;

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
  }
  NOTREACHED();
  return base::TimeDelta();
}

}  // namespace

ScopedOverviewAnimationSettings::ScopedOverviewAnimationSettings(
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
      animation_settings_.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
      break;
    case OVERVIEW_ANIMATION_HIDE_WINDOW:
      animation_settings_.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
  }
  animation_settings_.SetTransitionDuration(
      GetAnimationDuration(animation_type));
}

ScopedOverviewAnimationSettings::~ScopedOverviewAnimationSettings() {
}

}  // namespace ash
