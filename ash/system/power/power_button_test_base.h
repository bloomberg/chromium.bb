// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_TEST_BASE_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_TEST_BASE_H_

#include <memory>

#include "ash/system/power/power_button_controller.h"
#include "ash/system/power/tablet_power_button_controller.h"
#include "ash/test/ash_test_base.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace base {
class SimpleTestTickClock;
}  // namespace base

namespace chromeos {
class FakePowerManagerClient;
class FakeSessionManagerClient;
}  // namespace chromeos

namespace gfx {
class Vector3dF;
}  // namespace gfx

namespace ash {

class LockStateController;
class LockStateControllerTestApi;
class PowerButtonScreenshotController;
class TabletPowerButtonControllerTestApi;
enum class LoginStatus;

// Base test fixture and utils for testing power button related functions.
class PowerButtonTestBase : public AshTestBase {
 public:
  PowerButtonTestBase();
  ~PowerButtonTestBase() override;

  using ButtonType = PowerButtonController::ButtonType;

  // Vector pointing up (e.g. keyboard in clamshell).
  static constexpr gfx::Vector3dF kUpVector = {
      0, 0, TabletPowerButtonController::kGravity};

  // Vector pointing down (e.g. keyboard in tablet sitting on table).
  static constexpr gfx::Vector3dF kDownVector = {
      0, 0, -TabletPowerButtonController::kGravity};

  // Vector pointing sideways (e.g. screen in 90-degree clamshell).
  static constexpr gfx::Vector3dF kSidewaysVector = {
      0, TabletPowerButtonController::kGravity, 0};

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

 protected:
  // Resets the PowerButtonController and associated members.
  void ResetPowerButtonController();

  // Initializes |power_button_controller_| and other members that point at
  // objects owned by it. If |send_accelerometer_update| is true, an
  // accelerometer update is sent to create TabletPowerButtonController and
  // PowerButtonScreenshotController.
  void InitPowerButtonControllerMembers(bool send_accelerometer_update);

  // Sends an update with screen and keyboard accelerometer readings to
  // PowerButtonController, and also |tablet_controller_| if it's non-null and
  // has registered as an observer.
  void SendAccelerometerUpdate(const gfx::Vector3dF& screen,
                               const gfx::Vector3dF& keyboard);

  // Sets the flag for forcing clamshell-like power button behavior and resets
  // |power_button_controller_|.
  void ForceClamshellPowerButton();

  // Simulates a power button press.
  void PressPowerButton();

  // Simulates a power button release.
  void ReleasePowerButton();

  // Simulates a key press based on the given |key_code|.
  virtual void PressKey(ui::KeyboardCode key_code);

  // Simulates a key release based on the given |key_code|.
  virtual void ReleaseKey(ui::KeyboardCode key_code);

  // Simulates a mouse move event.
  void GenerateMouseMoveEvent();

  // Initializes login status and sets power button type.
  void Initialize(ButtonType button_type, LoginStatus status);

  // Triggers a lock screen operation.
  void LockScreen();

  // Triggers a unlock screen operation.
  void UnlockScreen();

  // Enables or disables tablet mode based on |enable|.
  void EnableTabletMode(bool enable);

  void set_has_tablet_mode_switch(bool has_tablet_mode_switch) {
    has_tablet_mode_switch_ = has_tablet_mode_switch;
  }

  // Ownership is passed on to chromeos::DBusThreadManager.
  chromeos::FakePowerManagerClient* power_manager_client_ = nullptr;
  chromeos::FakeSessionManagerClient* session_manager_client_ = nullptr;

  PowerButtonController* power_button_controller_ = nullptr;  // Not owned.
  LockStateController* lock_state_controller_ = nullptr;      // Not owned.
  TabletPowerButtonController* tablet_controller_ = nullptr;  // Not owned.
  PowerButtonScreenshotController* screenshot_controller_ =
      nullptr;  // Not owned.
  std::unique_ptr<LockStateControllerTestApi> lock_state_test_api_;
  std::unique_ptr<TabletPowerButtonControllerTestApi> tablet_test_api_;
  base::SimpleTestTickClock* tick_clock_ = nullptr;  // Not owned.

  // Indicates whether switches::kAshEnableTabletMode is appended.
  bool has_tablet_mode_switch_ = true;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonTestBase);
};

// Base test fixture same as PowerButtonTestBase excepts that tablet mode swtich
// is not set.
class NoTabletModePowerButtonTestBase : public PowerButtonTestBase {
 public:
  NoTabletModePowerButtonTestBase() { set_has_tablet_mode_switch(false); }
  ~NoTabletModePowerButtonTestBase() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NoTabletModePowerButtonTestBase);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_TEST_BASE_H_
