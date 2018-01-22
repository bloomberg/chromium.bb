// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_TEST_API_H_
#define ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_TEST_API_H_

#include "base/compiler_specific.h"
#include "base/macros.h"

namespace ui {
class KeyEvent;
}  // namespace ui

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {
class PowerButtonMenuView;
class TabletPowerButtonController;

// Helper class used by tests to access TabletPowerButtonController's
// internal state.
class TabletPowerButtonControllerTestApi {
 public:
  explicit TabletPowerButtonControllerTestApi(
      TabletPowerButtonController* controller);
  ~TabletPowerButtonControllerTestApi();

  // Returns true when |controller_->shutdown_timer_| is running.
  bool ShutdownTimerIsRunning() const;

  // If |controller_->shutdown_timer_| is running, stops it, runs its task, and
  // returns true. Otherwise, returns false.
  bool TriggerShutdownTimeout() WARN_UNUSED_RESULT;

  // Returns true when |power_button_menu_timer_| is running.
  bool PowerButtonMenuTimerIsRunning() const;

  // If |controller_->power_button_menu_timer_| is running, stops it, runs its
  // task, and returns true. Otherwise, returns false.
  bool TriggerPowerButtonMenuTimeout() WARN_UNUSED_RESULT;

  // Sends |event| to |controller_->display_controller_|.
  void SendKeyEvent(ui::KeyEvent* event);

  // Gets the bounds of the menu view in screen.
  gfx::Rect GetMenuBoundsInScreen() const;

  // Gets the PowerButtonMenuView of the |controller_|'s menu, which is used by
  // GetMenuBoundsInScreen.
  PowerButtonMenuView* GetPowerButtonMenuView() const;

  // True if the menu is opened.
  bool IsMenuOpened() const;

  // True if |controller_|'s menu has a sign out item.
  bool MenuHasSignOutItem() const;

 private:
  TabletPowerButtonController* controller_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(TabletPowerButtonControllerTestApi);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_TEST_API_H_
