// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_MENU_SCREEN_VIEW_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_MENU_SCREEN_VIEW_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/display/display_observer.h"
#include "ui/views/view.h"

namespace ash {
class PowerButtonMenuView;

// PowerButtonMenuScreenView is the top-level view of power button menu UI. It
// creates a PowerButtonMenuBackgroundView to display the fullscreen background
// and a PowerButtonMenuView to display the menu.
class ASH_EXPORT PowerButtonMenuScreenView : public views::View,
                                             public display::DisplayObserver {
 public:
  // |show_animation_done| is a callback for when the animation that shows the
  // power menu has finished.
  explicit PowerButtonMenuScreenView(
      base::RepeatingClosure show_animation_done);
  ~PowerButtonMenuScreenView() override;

  PowerButtonMenuView* power_button_menu_view() const {
    return power_button_menu_view_;
  }

  // Schedules an animation to show or hide the view.
  void ScheduleShowHideAnimation(bool show);

 private:
  class PowerButtonMenuBackgroundView;

  // views::View:
  void Layout() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Created by PowerButtonMenuScreenView. Owned by views hierarchy.
  PowerButtonMenuView* power_button_menu_view_ = nullptr;
  PowerButtonMenuBackgroundView* power_button_screen_background_shield_ =
      nullptr;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonMenuScreenView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_MENU_SCREEN_VIEW_H_
