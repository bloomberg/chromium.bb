// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_menu_screen_view.h"

#include "ash/system/power/power_button_menu_view.h"
#include "ash/system/power/tablet_power_button_controller.h"
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
  explicit PowerButtonMenuBackgroundView(
      TabletPowerButtonController* controller)
      : controller_(controller) {
    DCHECK(controller_);
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    layer()->SetColor(kShieldColor);
  }

  ~PowerButtonMenuBackgroundView() override = default;

  void OnImplicitAnimationsCompleted() override {
    if (layer()->opacity() == 0.f) {
      SetVisible(false);
      controller_->DismissMenu();
    }
  }

  void ScheduleShowHideAnimation(bool show) {
    layer()->GetAnimator()->StopAnimating();
    layer()->SetOpacity(show ? 0.f : kPowerButtonMenuOpacity);

    ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
    animation.AddObserver(this);
    animation.SetTweenType(show ? gfx::Tween::EASE_IN_2
                                : gfx::Tween::FAST_OUT_LINEAR_IN);
    animation.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        PowerButtonMenuView::kAnimationTimeoutMs));

    layer()->SetOpacity(show ? kPowerButtonMenuOpacity : 0.f);
  }

 private:
  TabletPowerButtonController* controller_ = nullptr;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(PowerButtonMenuBackgroundView);
};

PowerButtonMenuScreenView::PowerButtonMenuScreenView(
    TabletPowerButtonController* controller) {
  power_button_screen_background_shield_ =
      new PowerButtonMenuBackgroundView(controller);
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
