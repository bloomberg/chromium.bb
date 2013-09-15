// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_bounds_animation.h"

#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"

namespace {

// These values are experimental and subjective.
const int kDefaultFramerateHz = 50;
const int kSetBoundsAnimationMs = 180;
const int kSetBoundsAnimationBigMinimizeMs = 1500;

}

PanelBoundsAnimation::PanelBoundsAnimation(gfx::AnimationDelegate* target,
                                           Panel* panel,
                                           const gfx::Rect& initial_bounds,
                                           const gfx::Rect& final_bounds)
    : gfx::LinearAnimation(kDefaultFramerateHz, target),
      panel_(panel),
      for_big_minimize_(false),
      animation_stop_to_show_titlebar_(0) {
  // Detect animation that happens when expansion state is set to MINIMIZED
  // and there is relatively big portion of the panel to hide from view.
  // Initialize animation differently in this case, using fast-pause-slow
  // method, see below for more details.
  int duration = kSetBoundsAnimationMs;
  if (initial_bounds.height() > final_bounds.height() &&
      panel_->expansion_state() == Panel::MINIMIZED) {
    double hidden_title_height =
        panel_->TitleOnlyHeight() - final_bounds.height();
    double distance_y = initial_bounds.height() - final_bounds.height();
    animation_stop_to_show_titlebar_ = 1.0 - hidden_title_height / distance_y;
    if (animation_stop_to_show_titlebar_ > 0.7) {  // Relatively big movement.
      for_big_minimize_ = true;
      duration = kSetBoundsAnimationBigMinimizeMs;
    }
  }
  SetDuration(PanelManager::AdjustTimeInterval(duration));
}

PanelBoundsAnimation::~PanelBoundsAnimation() {
}

double PanelBoundsAnimation::GetCurrentValue() const {
  return ComputeAnimationValue(gfx::LinearAnimation::GetCurrentValue(),
                               for_big_minimize_,
                               animation_stop_to_show_titlebar_);
}

double PanelBoundsAnimation::ComputeAnimationValue(double progress,
    bool for_big_minimize, double animation_stop_to_show_titlebar) {
  if (!for_big_minimize) {
    // Cubic easing out.
    double value = 1.0 - progress;
    return 1.0 - value * value * value;
  }

  // Minimize animation is done in 3 steps:
  // 1. Quickly (0 -> 0.15) make only titlebar visible.
  // 2. Freeze for a little bit (0.15->0.6), just showing titlebar.
  // 3. Slowly minimize to thin strip (0.6->1.0)
  const double kProgressAtFreezeStart = 0.15;
  const double kProgressAtFreezeEnd = 0.6;
  double value;
  if (progress <= kProgressAtFreezeStart) {
    value = animation_stop_to_show_titlebar *
        (progress / kProgressAtFreezeStart);
  } else if (progress <= kProgressAtFreezeEnd) {
    value = animation_stop_to_show_titlebar;
  } else {
    value = animation_stop_to_show_titlebar +
        (1.0 - animation_stop_to_show_titlebar) *
        ((progress - kProgressAtFreezeEnd) / (1.0 - kProgressAtFreezeEnd));
  }
  return value;
}
