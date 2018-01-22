// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tablet_power_button_controller_test_api.h"

#include "ash/system/power/power_button_display_controller.h"
#include "ash/system/power/power_button_menu_screen_view.h"
#include "ash/system/power/power_button_menu_view.h"
#include "ash/system/power/tablet_power_button_controller.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

TabletPowerButtonControllerTestApi::TabletPowerButtonControllerTestApi(
    TabletPowerButtonController* controller)
    : controller_(controller) {}

TabletPowerButtonControllerTestApi::~TabletPowerButtonControllerTestApi() =
    default;

bool TabletPowerButtonControllerTestApi::ShutdownTimerIsRunning() const {
  return controller_->shutdown_timer_.IsRunning();
}

bool TabletPowerButtonControllerTestApi::TriggerShutdownTimeout() {
  if (!controller_->shutdown_timer_.IsRunning())
    return false;

  base::Closure task = controller_->shutdown_timer_.user_task();
  controller_->shutdown_timer_.Stop();
  task.Run();
  return true;
}

bool TabletPowerButtonControllerTestApi::PowerButtonMenuTimerIsRunning() const {
  return controller_->power_button_menu_timer_.IsRunning();
}

bool TabletPowerButtonControllerTestApi::TriggerPowerButtonMenuTimeout() {
  if (!controller_->power_button_menu_timer_.IsRunning())
    return false;

  base::Closure task = controller_->power_button_menu_timer_.user_task();
  controller_->power_button_menu_timer_.Stop();
  task.Run();
  return true;
}

void TabletPowerButtonControllerTestApi::SendKeyEvent(ui::KeyEvent* event) {
  controller_->display_controller_->OnKeyEvent(event);
}

gfx::Rect TabletPowerButtonControllerTestApi::GetMenuBoundsInScreen() const {
  return IsMenuOpened() ? GetPowerButtonMenuView()->GetBoundsInScreen()
                        : gfx::Rect();
}

PowerButtonMenuView*
TabletPowerButtonControllerTestApi::GetPowerButtonMenuView() const {
  return IsMenuOpened() ? static_cast<PowerButtonMenuScreenView*>(
                              controller_->menu_widget_->GetContentsView())
                              ->power_button_menu_view()
                        : nullptr;
}

bool TabletPowerButtonControllerTestApi::IsMenuOpened() const {
  return controller_->IsMenuOpened();
}

bool TabletPowerButtonControllerTestApi::MenuHasSignOutItem() const {
  return IsMenuOpened() &&
         GetPowerButtonMenuView()->sign_out_item_for_testing();
}

}  // namespace ash
