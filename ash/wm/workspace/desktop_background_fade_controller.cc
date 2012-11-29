// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/desktop_background_fade_controller.h"

#include "ash/wm/window_animations.h"
#include "ash/wm/workspace/colored_window_controller.h"
#include "base/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

DesktopBackgroundFadeController::DesktopBackgroundFadeController(
    aura::Window* parent,
    aura::Window* position_above,
    base::TimeDelta duration,
    Direction direction) {
  SkColor start_color, target_color;
  ui::Tween::Type tween_type;
  if (direction == FADE_OUT) {
    start_color = SkColorSetARGB(0, 0, 0, 0);
    target_color = SK_ColorBLACK;
    tween_type = ui::Tween::EASE_IN_OUT;
  } else {
    start_color = SK_ColorBLACK;
    target_color = SkColorSetARGB(0, 0, 0, 0);
    tween_type = ui::Tween::EASE_IN_OUT;
  }

  window_controller_.reset(
      new ColoredWindowController(parent, "DesktopFade"));

  // Force the window to be directly on top of the desktop.
  aura::Window* fade_window = window_controller_->GetWidget()->GetNativeView();
  parent->StackChildBelow(fade_window, position_above);
  parent->StackChildAbove(fade_window, position_above);
  window_controller_->SetColor(start_color);
  views::corewm::SetWindowVisibilityAnimationTransition(
      window_controller_->GetWidget()->GetNativeView(),
      views::corewm::ANIMATE_NONE);
  window_controller_->GetWidget()->Show();
  {
    ui::ScopedLayerAnimationSettings scoped_setter(
        fade_window->layer()->GetAnimator());
    scoped_setter.AddObserver(this);
    scoped_setter.SetTweenType(tween_type);
    scoped_setter.SetTransitionDuration(duration);
    window_controller_->SetColor(target_color);
  }
}

DesktopBackgroundFadeController::~DesktopBackgroundFadeController() {
  StopObservingImplicitAnimations();
}

void DesktopBackgroundFadeController::OnImplicitAnimationsCompleted() {
  window_controller_.reset();
}

}  // namespace internal
}  // namespace ash
