// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_CONVERTIBLE_POWER_BUTTON_CONTROLLER_TEST_API_H_
#define ASH_SYSTEM_POWER_CONVERTIBLE_POWER_BUTTON_CONTROLLER_TEST_API_H_

#include "base/compiler_specific.h"
#include "base/macros.h"

namespace ui {
class KeyEvent;
}  // namespace ui

namespace ash {

class ConvertiblePowerButtonController;

// Helper class used by tests to access ConvertiblePowerButtonController's
// internal state.
class ConvertiblePowerButtonControllerTestApi {
 public:
  explicit ConvertiblePowerButtonControllerTestApi(
      ConvertiblePowerButtonController* controller);
  ~ConvertiblePowerButtonControllerTestApi();

  // Returns true when |controller_->shutdown_timer_| is running.
  bool ShutdownTimerIsRunning() const;

  // If |controller_->shutdown_timer_| is running, stops it, runs its task, and
  // returns true. Otherwise, returns false.
  bool TriggerShutdownTimeout() WARN_UNUSED_RESULT;

  // Sends |event| to |controller_->display_controller_|.
  void SendKeyEvent(ui::KeyEvent* event);

 private:
  ConvertiblePowerButtonController* controller_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(ConvertiblePowerButtonControllerTestApi);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_CONVERTIBLE_POWER_BUTTON_CONTROLLER_TEST_API_H_
