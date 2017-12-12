// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_utils.h"

#include "ash/wm/splitview/split_view_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

void AnimateSplitviewLabelOpacity(ui::Layer* layer, bool visible) {
  float target_opacity = visible ? 1.f : 0.f;
  if (layer->GetTargetOpacity() == target_opacity)
    return;

  layer->SetOpacity(1.f - target_opacity);
  {
    ui::LayerAnimator* animator = layer->GetAnimator();
    animator->StopAnimating();
    ui::ScopedLayerAnimationSettings settings(animator);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kSplitviewAnimationDurationMs));
    settings.SetTweenType(gfx::Tween::EASE_OUT);
    layer->SetOpacity(target_opacity);
  }
}

}  // namespace ash
