// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_utils.h"

#include "ash/wm/splitview/split_view_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

namespace {

// The animation speed at which the highlights fade in or out.
constexpr base::TimeDelta kHighlightsFadeInOutMs =
    base::TimeDelta::FromMilliseconds(250);
// The animation speed which the other highlight fades in or out.
constexpr base::TimeDelta kOtherFadeInOutMs =
    base::TimeDelta::FromMilliseconds(133);
// The delay before the other highlight starts fading in or out.
constexpr base::TimeDelta kOtherFadeOutDelayMs =
    base::TimeDelta::FromMilliseconds(117);

constexpr float kHighlightOpacity = 0.3f;
constexpr float kPhantomHighlightOpacity = 0.18f;

// Gets the duration, tween type and delay before animation based on |type|.
void GetAnimationValuesForType(SplitviewAnimationType type,
                               base::TimeDelta* out_duration,
                               gfx::Tween::Type* out_tween_type,
                               base::TimeDelta* out_delay) {
  switch (type) {
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_PHANTOM_FADE_IN:
    case SPLITVIEW_ANIMATION_PHANTOM_FADE_OUT:
    case SPLITVIEW_ANIMATION_SELECTOR_ITEM_FADE_IN:
    case SPLITVIEW_ANIMATION_SELECTOR_ITEM_FADE_OUT:
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN:
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT:
    case SPLITVIEW_ANIMATION_PHANTOM_SLIDE_IN_OUT:
      *out_duration = kHighlightsFadeInOutMs;
      *out_tween_type = gfx::Tween::FAST_OUT_SLOW_IN;
      return;
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN:
      *out_duration = kOtherFadeInOutMs;
      *out_tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN;
      return;
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT:
      *out_delay = kOtherFadeOutDelayMs;
      *out_duration = kOtherFadeInOutMs;
      *out_tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN;
      return;
  }

  NOTREACHED();
}

// Helper function to apply animation values to |settings|.
void ApplyAnimationSettings(ui::ScopedLayerAnimationSettings* settings,
                            ui::LayerAnimator* animator,
                            base::TimeDelta duration,
                            gfx::Tween::Type tween,
                            base::TimeDelta delay) {
  DCHECK_EQ(settings->GetAnimator(), animator);
  settings->SetTransitionDuration(duration);
  settings->SetTweenType(tween);
  if (!delay.is_zero()) {
    settings->SetPreemptionStrategy(
        ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
    animator->SchedulePauseForProperties(delay,
                                         ui::LayerAnimationElement::OPACITY);
  }
}

}  // namespace

void DoSplitviewOpacityAnimation(ui::Layer* layer,
                                 SplitviewAnimationType type) {
  float target_opacity = 0.f;
  switch (type) {
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT:
    case SPLITVIEW_ANIMATION_SELECTOR_ITEM_FADE_OUT:
    case SPLITVIEW_ANIMATION_TEXT_FADE_OUT:
      target_opacity = 0.f;
      break;
    case SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN:
    case SPLITVIEW_ANIMATION_PHANTOM_FADE_IN:
      target_opacity = kPhantomHighlightOpacity;
      break;
    case SPLITVIEW_ANIMATION_PHANTOM_FADE_OUT:
      target_opacity = kHighlightOpacity;
      break;
    case SPLITVIEW_ANIMATION_SELECTOR_ITEM_FADE_IN:
    case SPLITVIEW_ANIMATION_TEXT_FADE_IN:
      target_opacity = 1.f;
      break;
    default:
      NOTREACHED() << "Not a valid split view opacity animation type.";
      return;
  }

  if (layer->GetTargetOpacity() == target_opacity)
    return;

  base::TimeDelta duration;
  gfx::Tween::Type tween;
  base::TimeDelta delay;
  GetAnimationValuesForType(type, &duration, &tween, &delay);

  ui::LayerAnimator* animator = layer->GetAnimator();
  ui::ScopedLayerAnimationSettings settings(animator);
  ApplyAnimationSettings(&settings, animator, duration, tween, delay);
  layer->SetOpacity(target_opacity);
}

void DoSplitviewTransformAnimation(ui::Layer* layer,
                                   SplitviewAnimationType type,
                                   const gfx::Transform& target_transform,
                                   ui::ImplicitAnimationObserver* observer) {
  if (layer->GetTargetTransform() == target_transform)
    return;

  switch (type) {
    case SPLITVIEW_ANIMATION_PHANTOM_SLIDE_IN_OUT:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN:
    case SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT:
      break;
    default:
      NOTREACHED() << "Not a valid split view transform type.";
      return;
  }

  base::TimeDelta duration;
  gfx::Tween::Type tween;
  base::TimeDelta delay;
  GetAnimationValuesForType(type, &duration, &tween, &delay);

  ui::LayerAnimator* animator = layer->GetAnimator();
  ui::ScopedLayerAnimationSettings settings(animator);
  ApplyAnimationSettings(&settings, animator, duration, tween, delay);
  if (observer)
    settings.AddObserver(observer);
  layer->SetTransform(target_transform);
}

}  // namespace ash
