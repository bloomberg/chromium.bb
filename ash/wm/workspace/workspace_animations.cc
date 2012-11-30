// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_animations.h"

#include "ash/wm/window_animations.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/corewm/window_animations.h"

namespace ash {
namespace internal {

const int kWorkspaceSwitchTimeMS = 200;

namespace {

// Tween type used when showing/hiding workspaces.
const ui::Tween::Type kWorkspaceTweenType = ui::Tween::EASE_OUT;

// If |details.duration| is not-empty it is returned, otherwise
// |kWorkspaceSwitchTimeMS| is returned.
base::TimeDelta DurationForWorkspaceShowOrHide(
    const WorkspaceAnimationDetails& details) {
  return details.duration == base::TimeDelta() ?
      base::TimeDelta::FromMilliseconds(kWorkspaceSwitchTimeMS) :
      details.duration;
}

}  // namespace

WorkspaceAnimationDetails::WorkspaceAnimationDetails()
    : direction(WORKSPACE_ANIMATE_UP),
      animate(false),
      animate_opacity(false),
      animate_scale(false),
      pause_time_ms(0) {
}

WorkspaceAnimationDetails::~WorkspaceAnimationDetails() {
}

void ShowWorkspace(aura::Window* window,
                   const WorkspaceAnimationDetails& details) {
  window->Show();

  if (!details.animate || views::corewm::WindowAnimationsDisabled(NULL)) {
    window->layer()->SetOpacity(1.0f);
    window->layer()->SetTransform(gfx::Transform());
    return;
  }

  window->layer()->SetOpacity(details.animate_opacity ? 0.0f : 1.0f);
  if (details.animate_scale) {
    SetTransformForScaleAnimation(window->layer(),
        details.direction == WORKSPACE_ANIMATE_UP ?
            LAYER_SCALE_ANIMATION_BELOW : LAYER_SCALE_ANIMATION_BELOW);
  } else {
    window->layer()->SetTransform(gfx::Transform());
  }

  // In order for pause to work we need to stop animations.
  window->layer()->GetAnimator()->StopAnimating();

  {
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());

    if (details.pause_time_ms > 0) {
      settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
      window->layer()->GetAnimator()->SchedulePauseForProperties(
          base::TimeDelta::FromMilliseconds(details.pause_time_ms),
          ui::LayerAnimationElement::TRANSFORM,
          ui::LayerAnimationElement::OPACITY,
          ui::LayerAnimationElement::BRIGHTNESS,
          ui::LayerAnimationElement::VISIBILITY,
          -1);
    }

    settings.SetTweenType(kWorkspaceTweenType);
    settings.SetTransitionDuration(DurationForWorkspaceShowOrHide(details));
    window->layer()->SetTransform(gfx::Transform());
    window->layer()->SetOpacity(1.0f);
  }
}

void HideWorkspace(aura::Window* window,
                   const WorkspaceAnimationDetails& details) {
  window->layer()->SetTransform(gfx::Transform());
  window->layer()->SetOpacity(1.0f);
  window->layer()->GetAnimator()->StopAnimating();

  if (!details.animate || views::corewm::WindowAnimationsDisabled(NULL)) {
    window->Hide();
    return;
  }

  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  if (details.pause_time_ms > 0) {
    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    window->layer()->GetAnimator()->SchedulePauseForProperties(
        base::TimeDelta::FromMilliseconds(details.pause_time_ms),
        ui::LayerAnimationElement::TRANSFORM,
        ui::LayerAnimationElement::OPACITY,
        ui::LayerAnimationElement::BRIGHTNESS,
        ui::LayerAnimationElement::VISIBILITY,
        -1);
  }

  settings.SetTransitionDuration(DurationForWorkspaceShowOrHide(details));
  settings.SetTweenType(kWorkspaceTweenType);
  if (details.animate_scale) {
    SetTransformForScaleAnimation(window->layer(),
        details.direction == WORKSPACE_ANIMATE_UP ?
            LAYER_SCALE_ANIMATION_ABOVE : LAYER_SCALE_ANIMATION_BELOW);
  } else {
    window->layer()->SetTransform(gfx::Transform());
  }

  // NOTE: Hide() must be before SetOpacity(), else
  // VisibilityController::UpdateLayerVisibility doesn't pass the false to the
  // layer so that the layer and window end up out of sync and confused.
  window->Hide();

  if (details.animate_opacity)
    window->layer()->SetOpacity(0.0f);

  // After the animation completes snap the transform back to the identity,
  // otherwise any one that asks for screen bounds gets a slightly scaled
  // version.
  settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
  settings.SetTransitionDuration(base::TimeDelta());
  window->layer()->SetTransform(gfx::Transform());
}

}  // namespace internal
}  // namespace ash
