// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_TEST_API_H_
#define ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_TEST_API_H_

#include "base/compiler_specific.h"
#include "base/macros.h"

namespace base {
class CommandLine;
}  // namespace base

namespace chromeos {
class AccelerometerReader;
}  // namespace chromeos

namespace ui {
class KeyEvent;
}  // namespace ui

namespace ash {

class TabletPowerButtonController;

// Helper class used by tests to access TabletPowerButtonController's internal
// state.
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

  // Returns true if |controller_| is observing |reader|.
  bool IsObservingAccelerometerReader(
      chromeos::AccelerometerReader* reader) const;

  // Calls |controller_|'s ParseSpuriousPowerButtonSwitches() method.
  void ParseSpuriousPowerButtonSwitches(const base::CommandLine& command_line);

  // Calls |controller_|'s IsSpuriousPowerButtonEvent() method.
  bool IsSpuriousPowerButtonEvent() const;

  // Sends |event| to |controller_->display_controller_|.
  void SendKeyEvent(ui::KeyEvent* event);

 private:
  TabletPowerButtonController* controller_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(TabletPowerButtonControllerTestApi);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_TEST_API_H_
