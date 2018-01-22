// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_BUTTON_MENU_VIEW_H_
#define ASH_SYSTEM_POWER_BUTTON_MENU_VIEW_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {
class PowerButtonMenuItemView;

// PowerButtonMenuView displays the menu items of the power button menu. It
// includes power off and sign out items currently.
class ASH_EXPORT PowerButtonMenuView : public views::View,
                                       public views::ButtonListener,
                                       public ui::ImplicitAnimationObserver {
 public:
  // The duration of the animation to show or hide the power button menu view.
  static constexpr int kAnimationTimeoutMs = 500;

  // Top padding of the menu view.
  static constexpr int kMenuViewTopPadding = 16;

  PowerButtonMenuView();
  ~PowerButtonMenuView() override;

  bool sign_out_item_for_testing() const { return sign_out_item_; }

  // Schedules an animation to show or hide the view.
  void ScheduleShowHideAnimation(bool show);

 private:
  // Creates the items that in the menu.
  void CreateItems();

  // views::View:
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Items in the menu. Owned by views hierarchy.
  PowerButtonMenuItemView* power_off_item_ = nullptr;
  PowerButtonMenuItemView* sign_out_item_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonMenuView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_BUTTON_MENU_VIEW_H_
