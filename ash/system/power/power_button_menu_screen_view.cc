// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_menu_screen_view.h"

#include "ash/shell.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/power/power_button_menu_view.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Color of the fullscreen background shield.
constexpr SkColor kShieldColor = SkColorSetARGBMacro(0xFF, 0x00, 0x00, 0x00);

// Opacity of the power button menu fullscreen background shield.
constexpr float kPowerButtonMenuOpacity = 0.6f;

}  // namespace

class PowerButtonMenuScreenView::PowerButtonMenuBackgroundView
    : public views::View,
      public ui::ImplicitAnimationObserver {
 public:
  PowerButtonMenuBackgroundView(base::RepeatingClosure show_animation_done)
      : show_animation_done_(show_animation_done) {
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    layer()->SetColor(kShieldColor);
  }

  ~PowerButtonMenuBackgroundView() override = default;

  void OnImplicitAnimationsCompleted() override {
    PowerButtonController* power_button_controller =
        Shell::Get()->power_button_controller();
    if (layer()->opacity() == 0.f) {
      SetVisible(false);
      power_button_controller->DismissMenu();
    }

    if (layer()->opacity() == kPowerButtonMenuOpacity)
      show_animation_done_.Run();
  }

  void ScheduleShowHideAnimation(bool show) {
    layer()->GetAnimator()->AbortAllAnimations();
    layer()->SetOpacity(show ? 0.f : layer()->opacity());

    ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
    animation.AddObserver(this);
    animation.SetTweenType(show ? gfx::Tween::EASE_IN_2
                                : gfx::Tween::FAST_OUT_LINEAR_IN);
    animation.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        PowerButtonMenuView::kAnimationTimeoutMs));

    layer()->SetOpacity(show ? kPowerButtonMenuOpacity : 0.f);
  }

 private:
  // A callback for when the animation that shows the power menu has finished.
  base::RepeatingClosure show_animation_done_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonMenuBackgroundView);
};

PowerButtonMenuScreenView::PowerButtonMenuScreenView(
    base::RepeatingClosure show_animation_done) {
  power_button_screen_background_shield_ =
      new PowerButtonMenuBackgroundView(show_animation_done);
  AddChildView(power_button_screen_background_shield_);

  power_button_menu_view_ = new PowerButtonMenuView();
  AddChildView(power_button_menu_view_);

  display::Screen::GetScreen()->AddObserver(this);
}

PowerButtonMenuScreenView::~PowerButtonMenuScreenView() {
  display::Screen::GetScreen()->RemoveObserver(this);
}

void PowerButtonMenuScreenView::ScheduleShowHideAnimation(bool show) {
  power_button_screen_background_shield_->ScheduleShowHideAnimation(show);
  power_button_menu_view_->ScheduleShowHideAnimation(show);
}

void PowerButtonMenuScreenView::Layout() {
  const gfx::Rect contents_bounds = GetContentsBounds();
  power_button_screen_background_shield_->SetBoundsRect(contents_bounds);

  // TODO(minch): Get the menu bounds according to the power button position.
  gfx::Rect menu_bounds = contents_bounds;
  menu_bounds.ClampToCenteredSize(power_button_menu_view_->GetPreferredSize());
  menu_bounds.set_y(menu_bounds.y() - PowerButtonMenuView::kMenuViewTopPadding);
  power_button_menu_view_->SetBoundsRect(menu_bounds);
}

bool PowerButtonMenuScreenView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

void PowerButtonMenuScreenView::OnMouseReleased(const ui::MouseEvent& event) {
  ScheduleShowHideAnimation(false);
}

void PowerButtonMenuScreenView::OnGestureEvent(ui::GestureEvent* event) {
  // Dismisses the menu if tap anywhere on the background shield.
  if (event->type() == ui::ET_GESTURE_TAP_DOWN)
    ScheduleShowHideAnimation(false);
}

void PowerButtonMenuScreenView::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  GetWidget()->SetBounds(
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
}

}  // namespace ash
