// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/media_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/config.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/power/power_button_controller_test_api.h"
#include "ash/system/power/power_button_menu_view.h"
#include "ash/system/power/power_button_test_base.h"
#include "ash/test_media_client.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/wm/lock_state_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/test_session_state_animator.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

using PowerManagerClient = chromeos::PowerManagerClient;

namespace ash {

namespace {

// A non-zero brightness used for test.
constexpr double kNonZeroBrightness = 10.;

// Width of the display.
constexpr int kDisplayWidth = 800;

// Height of the display.
constexpr int kDisplayHeight = 600;

// Power button position offset percentage.
constexpr double kPowerButtonPercentage = 0.75f;

// Shorthand for some long constants.
constexpr power_manager::BacklightBrightnessChange_Cause kUserCause =
    power_manager::BacklightBrightnessChange_Cause_USER_REQUEST;
constexpr power_manager::BacklightBrightnessChange_Cause kOtherCause =
    power_manager::BacklightBrightnessChange_Cause_OTHER;

}  // namespace

using PowerButtonPosition = PowerButtonController::PowerButtonPosition;

class PowerButtonControllerTest : public PowerButtonTestBase {
 public:
  PowerButtonControllerTest() = default;
  ~PowerButtonControllerTest() override = default;

  void SetUp() override {
    PowerButtonTestBase::SetUp();
    InitPowerButtonControllerMembers(PowerManagerClient::TabletMode::ON);

    SendBrightnessChange(kNonZeroBrightness, kUserCause);
    EXPECT_FALSE(power_manager_client_->backlights_forced_off());

    // Advance a duration longer than |kIgnorePowerButtonAfterResumeDelay| to
    // avoid events being ignored.
    tick_clock_.Advance(
        PowerButtonController::kIgnorePowerButtonAfterResumeDelay +
        base::TimeDelta::FromMilliseconds(2));

    // Run the event loop so that PowerButtonDisplayController can receive the
    // initial backlights-forced-off state.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void SendBrightnessChange(
      double percent,
      power_manager::BacklightBrightnessChange_Cause cause) {
    power_manager::BacklightBrightnessChange change;
    change.set_percent(percent);
    change.set_cause(cause);
    power_manager_client_->SendScreenBrightnessChanged(change);
  }

  bool GetLockedState() {
    // LockScreen is an async mojo call.
    SessionController* const session_controller =
        Shell::Get()->session_controller();
    session_controller->FlushMojoForTest();
    return session_controller->IsScreenLocked();
  }

  bool GetGlobalTouchscreenEnabled() const {
    return Shell::Get()->touch_devices_controller()->GetTouchscreenEnabled(
        TouchscreenEnabledSource::GLOBAL);
  }

  // Tapping power button when screen is off will turn the screen on but not
  // showing the menu.
  void TappingPowerButtonWhenScreenIsIdleOff() {
    SendBrightnessChange(0, kUserCause);
    PressPowerButton();
    EXPECT_FALSE(power_manager_client_->backlights_forced_off());
    SendBrightnessChange(kNonZeroBrightness, kUserCause);
    ReleasePowerButton();
    EXPECT_FALSE(power_manager_client_->backlights_forced_off());
    EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  }

  // Press the power button to show the menu.
  void OpenPowerButtonMenu() {
    PressPowerButton();
    EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
    ASSERT_TRUE(power_button_test_api_->TriggerPowerButtonMenuTimeout());
    ReleasePowerButton();
    ASSERT_TRUE(power_button_test_api_->IsMenuOpened());
  }

  // Tap outside of the menu view to dismiss the menu.
  void TapToDismissPowerButtonMenu() {
    gfx::Rect menu_bounds = power_button_test_api_->GetMenuBoundsInScreen();
    GetEventGenerator().GestureTapAt(
        gfx::Point(menu_bounds.x() - 5, menu_bounds.y() - 5));
    EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
  }

  void PressLockButton() {
    power_button_controller_->OnLockButtonEvent(true, base::TimeTicks::Now());
  }

  void ReleaseLockButton() {
    power_button_controller_->OnLockButtonEvent(false, base::TimeTicks::Now());
  }

  DISALLOW_COPY_AND_ASSIGN(PowerButtonControllerTest);
};

TEST_F(PowerButtonControllerTest, LockScreenIfRequired) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  SetShouldLockScreenAutomatically(true);
  ASSERT_FALSE(GetLockedState());

  // On User logged in status, power-button-press-release should lock screen if
  // automatic screen-locking was requested.
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_TRUE(GetLockedState());

  // On locked state, power-button-press-release should do nothing.
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_TRUE(GetLockedState());

  // Unlock the sceen.
  UnlockScreen();
  ASSERT_FALSE(GetLockedState());

  // power-button-press-release should not lock the screen if automatic
  // screen-locking wasn't requested.
  SetShouldLockScreenAutomatically(false);
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(GetLockedState());
}

// Tests that tapping power button of a clamshell device.
TEST_F(PowerButtonControllerTest, TappingPowerButtonOfClamshell) {
  // Should not turn the screen off when screen is on.
  InitPowerButtonControllerMembers(PowerManagerClient::TabletMode::UNSUPPORTED);
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  PressPowerButton();
  power_button_test_api_->SetShowMenuAnimationDone(false);
  // Start the showing power menu animation immediately as pressing the
  // clamshell power button.
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  // Start the dimissing power menu animation immediately as releasing the
  // clamsehll power button if showing animation hasn't finished.
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());

  AdvanceClockToAvoidIgnoring();
  // Should turn screen on if screen is off.
  TappingPowerButtonWhenScreenIsIdleOff();

  AdvanceClockToAvoidIgnoring();
  // Should not start the dismissing menu animation if showing menu animation
  // has finished.
  PressPowerButton();
  // Start the showing power menu animation immediately as pressing the
  // clamshell power button.
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  power_button_test_api_->SetShowMenuAnimationDone(true);
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  // Power button menu should keep opened if showing animation has finished.
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
}

// Tests that tapping power button of a device that has tablet mode switch.
TEST_F(PowerButtonControllerTest, TappingPowerButtonOfTablet) {
  // Should turn screen off if screen is on and power button menu will not be
  // shown.
  PressPowerButton();
  // Showing power menu animation hasn't started as power menu timer is running.
  EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
  ReleasePowerButton();
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());

  // Should turn screen on if screen is off.
  AdvanceClockToAvoidIgnoring();
  TappingPowerButtonWhenScreenIsIdleOff();

  // Showing power menu animation should start until power menu timer is
  // timeout.
  PressPowerButton();
  power_button_test_api_->SetShowMenuAnimationDone(false);
  EXPECT_TRUE(power_button_test_api_->TriggerPowerButtonMenuTimeout());
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  ReleasePowerButton();
  // Showing animation will continue until show the power button menu even
  // release the power button.
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());

  // Should not turn screen off if clamshell-like power button behavior is
  // requested.
  ForceClamshellPowerButton();
  SetTabletModeSwitchState(PowerManagerClient::TabletMode::ON);
  AdvanceClockToAvoidIgnoring();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  PressPowerButton();
  power_button_test_api_->SetShowMenuAnimationDone(false);
  // Forced clamshell power button device should start showing menu animation
  // immediately as pressing the power button.
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  // Forced clamshell power button device should start dismissing menu animation
  // immediately as releasing the power button.
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests that the mode-specific power button feature causes power button taps to
// turn the screen off while in tablet mode but not in laptop mode.
TEST_F(PowerButtonControllerTest, ModeSpecificPowerButton) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kModeSpecificPowerButton);

  // While the device is in tablet mode, tapping the power button should turn
  // the display off (and then back on if pressed a second time).
  power_button_controller_->OnTabletModeStarted();
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // In laptop mode, tapping the power button shouldn't turn the screen off.
  // Instead, we should start showing the power menu animation.
  power_button_controller_->OnTabletModeEnded();
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
}

// Tests that release power button after menu is opened but before trigger
// shutdown will not turn screen off.
TEST_F(PowerButtonControllerTest, ReleasePowerButtonAfterShowPowerButtonMenu) {
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  EXPECT_TRUE(power_button_test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_TRUE(power_button_test_api_->TriggerPowerButtonMenuTimeout());
  ReleasePowerButton();
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  EXPECT_FALSE(power_button_test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
}

// Tests that the real shutdown is started if the power button is released
// after the timer fires when screen is on.
TEST_F(PowerButtonControllerTest, RealShutdownIfScreenIsOn) {
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->TriggerShutdownTimeout());
  ShutdownSoundPlayed();
  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  ReleasePowerButton();
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
  // Release power button if real shutdown started will not cancel the shutdown.
  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
}

// Tests that the real shutdown is started if the power button is released
// after the timer fires when screen is off.
TEST_F(PowerButtonControllerTest, RealShutdownIfScreenIsOff) {
  // Press power button to turn screen off.
  PressPowerButton();
  ReleaseLockButton();

  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->TriggerShutdownTimeout());
  ShutdownSoundPlayed();
  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  ReleasePowerButton();
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
  // Release power button if real shutdown started will not cancel the shutdown.
  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
}

// Should dismiss the menu if locking screen when menu is opened.
TEST_F(PowerButtonControllerTest, LockScreenIfMenuIsOpened) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  OpenPowerButtonMenu();
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  PressLockButton();
  ReleaseLockButton();
  EXPECT_TRUE(lock_state_test_api_->is_animating_lock());
  EXPECT_EQ(1, session_manager_client_->request_lock_screen_call_count());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests press lock button and power button in sequence.
TEST_F(PowerButtonControllerTest, PressAfterAnotherReleased) {
  // Tap power button after press lock button should still turn screen off.
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  PressLockButton();
  ReleaseLockButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());

  // Press lock button after tap power button should still lock screen.
  PressPowerButton();
  ReleasePowerButton();
  PressLockButton();
  ReleaseLockButton();
  EXPECT_TRUE(lock_state_test_api_->is_animating_lock());
  EXPECT_EQ(1, session_manager_client_->request_lock_screen_call_count());
}

// Tests press lock/power button before release power/lock button.
TEST_F(PowerButtonControllerTest, PressBeforeAnotherReleased) {
  // Press lock button when power button is still being pressed will be ignored
  // and continue to turn screen off.
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  PressPowerButton();
  PressLockButton();
  ReleaseLockButton();
  ReleasePowerButton();
  EXPECT_FALSE(lock_state_test_api_->is_animating_lock());
  EXPECT_EQ(0, session_manager_client_->request_lock_screen_call_count());
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());

  // Turn the screen on.
  PressPowerButton();
  ReleasePowerButton();
  // Press power button when lock button is still being pressed. The pressing of
  // power button will be ignored and continue to lock screen.
  PressLockButton();
  PressPowerButton();
  ReleasePowerButton();
  ReleaseLockButton();
  EXPECT_TRUE(lock_state_test_api_->is_animating_lock());
  EXPECT_EQ(1, session_manager_client_->request_lock_screen_call_count());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
}

// Tests tapping power button when device is suspended without backlights forced
// off.
TEST_F(PowerButtonControllerTest,
       TappingPowerButtonWhenSuspendedWithoutBacklightsForcedOff) {
  power_manager_client_->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  SendBrightnessChange(0, kUserCause);
  // There is a power button pressed here, but PowerButtonEvent is sent later.
  power_manager_client_->SendSuspendDone();
  SendBrightnessChange(kNonZeroBrightness, kUserCause);

  // Send the power button event after a short delay and check that backlights
  // are not forced off.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(500));
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->ShutdownTimerIsRunning());
  ReleasePowerButton();
  EXPECT_FALSE(power_button_test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Send the power button event after a longer delay and check that backlights
  // are forced off.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(1600));
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->ShutdownTimerIsRunning());
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_FALSE(power_button_test_api_->ShutdownTimerIsRunning());
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
}

// Tests tapping power button when device is suspended with backlights forced
// off.
TEST_F(PowerButtonControllerTest,
       TappingPowerButtonWhenSuspendedWithBacklightsForcedOff) {
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  power_manager_client_->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  // There is a power button pressed here, but PowerButtonEvent is sent later.
  // Because of backlights forced off, resuming system will not restore
  // brightness.
  power_manager_client_->SendSuspendDone();

  // Send the power button event after a short delay and check that backlights
  // are not forced off.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(500));
  PressPowerButton();
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  EXPECT_TRUE(power_button_test_api_->ShutdownTimerIsRunning());
  ReleasePowerButton();
  EXPECT_FALSE(power_button_test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Send the power button event after a longer delay and check that backlights
  // are forced off.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(1600));
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->ShutdownTimerIsRunning());
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_FALSE(power_button_test_api_->ShutdownTimerIsRunning());
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
}

// For convertible device working on laptop mode, tests keyboard/mouse event
// when screen is off.
TEST_F(PowerButtonControllerTest, ConvertibleOnLaptopMode) {
  EnableTabletMode(false);

  // KeyEvent should SetBacklightsForcedOff(false).
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  PressKey(ui::VKEY_L);
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Regular mouse event should SetBacklightsForcedOff(false).
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  GenerateMouseMoveEvent();
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Synthesized mouse event should not SetBacklightsForcedOff(false).
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  GetEventGenerator().set_flags(ui::EF_IS_SYNTHESIZED);
  GenerateMouseMoveEvent();
  GetEventGenerator().set_flags(ui::EF_NONE);
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
}

// For convertible device working on tablet mode, keyboard/mouse event should
// not SetBacklightsForcedOff(false) when screen is off.
TEST_F(PowerButtonControllerTest, ConvertibleOnTabletMode) {
  EnableTabletMode(true);

  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  PressKey(ui::VKEY_L);
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());

  GenerateMouseMoveEvent();
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
}

// Tests that a single set of power button pressed-and-released operation should
// cause only one SetBacklightsForcedOff call.
TEST_F(PowerButtonControllerTest, IgnorePowerOnKeyEvent) {
  ui::KeyEvent power_key_pressed(ui::ET_KEY_PRESSED, ui::VKEY_POWER,
                                 ui::EF_NONE);
  ui::KeyEvent power_key_released(ui::ET_KEY_RELEASED, ui::VKEY_POWER,
                                  ui::EF_NONE);

  // There are two |power_key_pressed| events and |power_key_released| events
  // generated for each pressing and releasing, and multiple repeating pressed
  // events depending on holding.
  ASSERT_EQ(0, power_manager_client_->num_set_backlights_forced_off_calls());
  power_button_test_api_->SendKeyEvent(&power_key_pressed);
  power_button_test_api_->SendKeyEvent(&power_key_pressed);
  PressPowerButton();
  power_button_test_api_->SendKeyEvent(&power_key_pressed);
  power_button_test_api_->SendKeyEvent(&power_key_pressed);
  power_button_test_api_->SendKeyEvent(&power_key_pressed);
  ReleasePowerButton();
  power_button_test_api_->SendKeyEvent(&power_key_released);
  power_button_test_api_->SendKeyEvent(&power_key_released);
  EXPECT_EQ(1, power_manager_client_->num_set_backlights_forced_off_calls());
}

// Tests that under (1) tablet power button pressed/released, (2) keyboard/mouse
// events on laptop mode when screen is off, requesting/stopping backlights
// forced off should update the global touchscreen enabled status.
TEST_F(PowerButtonControllerTest, DisableTouchscreenWhileForcedOff) {
  // Tests tablet power button.
  ASSERT_TRUE(GetGlobalTouchscreenEnabled());
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_FALSE(GetGlobalTouchscreenEnabled());

  PressPowerButton();
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  ReleasePowerButton();
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());

  EnableTabletMode(false);
  // KeyEvent on laptop mode when screen is off.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  ASSERT_FALSE(GetGlobalTouchscreenEnabled());
  PressKey(ui::VKEY_L);
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());

  // MouseEvent on laptop mode when screen is off.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  ASSERT_FALSE(GetGlobalTouchscreenEnabled());
  GenerateMouseMoveEvent();
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
}

// When the screen is turned off automatically, the touchscreen should also be
// disabled.
TEST_F(PowerButtonControllerTest, DisableTouchscreenForInactivity) {
  ASSERT_TRUE(GetGlobalTouchscreenEnabled());

  // Turn screen off for automated change (e.g. user is inactive).
  SendBrightnessChange(0, kOtherCause);
  EXPECT_FALSE(GetGlobalTouchscreenEnabled());
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());

  // After decreasing the brightness to zero for a user request, the touchscreen
  // should remain enabled.
  SendBrightnessChange(0, kUserCause);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
}

// When user switches convertible device between laptop mode and tablet mode,
// power button may be pressed and held, which may cause unwanted unclean
// shutdown.
TEST_F(PowerButtonControllerTest,
       EnterOrLeaveTabletModeWhilePressingPowerButton) {
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->ShutdownTimerIsRunning());
  EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  power_button_controller_->OnTabletModeStarted();
  EXPECT_FALSE(power_button_test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(1500));
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());

  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->ShutdownTimerIsRunning());
  EXPECT_TRUE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  power_button_controller_->OnTabletModeEnded();
  EXPECT_FALSE(power_button_test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(power_button_test_api_->PowerButtonMenuTimerIsRunning());
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(2500));
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests that repeated power button releases are ignored (crbug.com/675291).
TEST_F(PowerButtonControllerTest, IgnoreRepeatedPowerButtonReleases) {
  // Set backlights forced off for starting point.
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());

  // Test that a pressing-releasing operation after a short duration, backlights
  // forced off is stopped since we don't drop request for power button pressed.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(200));
  PressPowerButton();
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Test that after another short duration, backlights will not be forced off
  // since this immediately following forcing off request needs to be dropped.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(200));
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Test that after another long duration, backlights should be forced off.
  tick_clock_.Advance(base::TimeDelta::FromMilliseconds(800));
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
}

// Tests that lid closed/open events stop forcing off backlights.
TEST_F(PowerButtonControllerTest, LidEventsStopForcingOff) {
  // Pressing/releasing power button to set backlights forced off.
  PressPowerButton();
  ReleasePowerButton();
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());

  // A lid closed event is received, we should stop forcing off backlights.
  power_manager_client_->SetLidState(PowerManagerClient::LidState::CLOSED,
                                     tick_clock_.NowTicks());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Pressing/releasing power button again to set backlights forced off. This is
  // for testing purpose. In real life, powerd would not repond to this event
  // with lid closed state.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());

  // A lid open event is received, we should stop forcing off backlights.
  power_manager_client_->SetLidState(PowerManagerClient::LidState::OPEN,
                                     tick_clock_.NowTicks());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
}

// Tests that tablet mode events from powerd stop forcing off backlights.
TEST_F(PowerButtonControllerTest, TabletModeEventsStopForcingOff) {
  PressPowerButton();
  ReleasePowerButton();
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  power_manager_client_->SetTabletMode(PowerManagerClient::TabletMode::ON,
                                       tick_clock_.NowTicks());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  power_manager_client_->SetTabletMode(PowerManagerClient::TabletMode::OFF,
                                       tick_clock_.NowTicks());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
}

// Tests that with system reboot, the global touchscreen enabled status should
// be synced with new backlights forced off state from powerd.
TEST_F(PowerButtonControllerTest, SyncTouchscreenEnabled) {
  Shell::Get()->touch_devices_controller()->SetTouchscreenEnabled(
      false, TouchscreenEnabledSource::GLOBAL);
  ASSERT_FALSE(GetGlobalTouchscreenEnabled());

  // Simulate system reboot by resetting backlights forced off state in powerd
  // and PowerButtonController.
  power_manager_client_->SetBacklightsForcedOff(false);
  ResetPowerButtonController();
  SetTabletModeSwitchState(PowerManagerClient::TabletMode::ON);

  // Run the event loop for PowerButtonDisplayController to get backlight state
  // and check that the global touchscreen status is correct.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
}

// Tests that when backlights get forced off due to tablet power button, media
// sessions should be suspended.
TEST_F(PowerButtonControllerTest, SuspendMediaSessions) {
  TestMediaClient client;
  Shell::Get()->media_controller()->SetClient(client.CreateAssociatedPtrInfo());
  ASSERT_FALSE(client.media_sessions_suspended());

  PressPowerButton();
  ReleasePowerButton();
  // Run the event loop for PowerButtonDisplayController to get backlight state.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  EXPECT_TRUE(client.media_sessions_suspended());
}

// Tests that when system is suspended with backlights forced off, and then
// system resumes due to power button pressed without power button event fired
// (crbug.com/735291), that we stop forcing off backlights.
TEST_F(PowerButtonControllerTest, SuspendDoneStopsForcingOff) {
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());

  // Simulate an edge case that system resumes because of tablet power button
  // pressed, but power button event is not delivered.
  power_manager_client_->SendSuspendDone();

  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
}

// Tests that updating power button behavior from tablet behavior to clamshell
// behavior will initially enable touchscreen on global touchscreen enabled
// status (b/64972736).
TEST_F(PowerButtonControllerTest, TouchscreenEnabledClamshell) {
  PressPowerButton();
  ReleasePowerButton();
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  ASSERT_FALSE(GetGlobalTouchscreenEnabled());

  // Simulates a system reboot with |kForceClamshellPowerButton| requested by
  // flag, resetting backlights forced off state in powerd and
  // PowerButtonController.
  power_manager_client_->SetBacklightsForcedOff(false);
  ForceClamshellPowerButton();
  // Run the event loop for PowerButtonDisplayController to get backlight state.
  base::RunLoop().RunUntilIdle();
  SetTabletModeSwitchState(PowerManagerClient::TabletMode::ON);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
}

// Tests that during the interval that the display is turning on, tablet power
// button should not set display off (crbug.com/735225).
TEST_F(PowerButtonControllerTest, IgnoreForcingOffWhenDisplayIsTurningOn) {
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());

  // Trigger a key event to stop backlights forcing off. Chrome will receive
  // brightness changed signal. But we may still have display off state.
  PressKey(ui::VKEY_L);
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Since display could still be off, ignore forcing off.
  tick_clock_.Advance(PowerButtonController::kScreenStateChangeDelay -
                      base::TimeDelta::FromMilliseconds(1));
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // After avoiding ignoring the repeated power button releases, we should be
  // able to set display off.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
}

// Tests that a11y alert is sent on tablet power button induced screen state
// change.
TEST_F(PowerButtonControllerTest, A11yAlert) {
  TestAccessibilityControllerClient client;
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->SetClient(client.CreateInterfacePtrAndBind());
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  controller->FlushMojoForTest();
  EXPECT_EQ(mojom::AccessibilityAlert::SCREEN_OFF, client.last_a11y_alert());

  PressPowerButton();
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  controller->FlushMojoForTest();
  EXPECT_EQ(mojom::AccessibilityAlert::SCREEN_ON, client.last_a11y_alert());
  ReleasePowerButton();
}

// Tests that tap outside of the menu bounds should dismiss the menu.
TEST_F(PowerButtonControllerTest, TapToDismissMenu) {
  OpenPowerButtonMenu();
  TapToDismissPowerButtonMenu();
}

// Test that mouse click outside of the menu bounds should dismiss the menu.
TEST_F(PowerButtonControllerTest, MouseClickToDismissMenu) {
  OpenPowerButtonMenu();
  gfx::Rect menu_bounds = power_button_test_api_->GetMenuBoundsInScreen();
  ui::test::EventGenerator& generator(GetEventGenerator());
  generator.MoveMouseTo(gfx::Point(menu_bounds.x() - 5, menu_bounds.y() - 5));
  generator.ClickLeftButton();
  generator.ReleaseLeftButton();
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests the menu items according to the login status.
TEST_F(PowerButtonControllerTest, MenuItemsToLoginStatus) {
  // No sign out item if user is not logged in.
  ClearLogin();
  Shell::Get()->UpdateAfterLoginStatusChange(LoginStatus::NOT_LOGGED_IN);
  OpenPowerButtonMenu();
  EXPECT_FALSE(power_button_test_api_->MenuHasSignOutItem());
  TapToDismissPowerButtonMenu();

  // Should have sign out item if user is login.
  CreateUserSessions(1);
  Shell::Get()->UpdateAfterLoginStatusChange(LoginStatus::USER);
  OpenPowerButtonMenu();
  EXPECT_TRUE(power_button_test_api_->MenuHasSignOutItem());
  TapToDismissPowerButtonMenu();

  // Should have sign out item if user is logged in but screen is locked.
  Shell::Get()->UpdateAfterLoginStatusChange(LoginStatus::LOCKED);
  OpenPowerButtonMenu();
  EXPECT_TRUE(power_button_test_api_->MenuHasSignOutItem());
}

// Repeat long press should redisplay the menu.
TEST_F(PowerButtonControllerTest, PressButtonWhenMenuIsOpened) {
  OpenPowerButtonMenu();
  AdvanceClockToAvoidIgnoring();
  OpenPowerButtonMenu();
}

// Tests that switches between laptop mode and tablet mode should dismiss the
// opened menu.
TEST_F(PowerButtonControllerTest, EnterOrLeaveTabletModeDismissMenu) {
  OpenPowerButtonMenu();
  power_button_controller_->OnTabletModeEnded();
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());

  OpenPowerButtonMenu();
  power_button_controller_->OnTabletModeStarted();
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests that screen changes to idle off will dismiss the opened menu.
TEST_F(PowerButtonControllerTest, DismissMenuWhenScreenIsIdleOff) {
  OpenPowerButtonMenu();
  // Mock screen idle off.
  SendBrightnessChange(0, kUserCause);
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests that tapping the power button should dimiss the opened menu.
TEST_F(PowerButtonControllerTest, TappingPowerButtonWhenMenuIsOpened) {
  OpenPowerButtonMenu();

  // Tapping the power button when menu is opened will dismiss the menu.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());

  // Long press the power button when backlights are off will show the menu.
  PressPowerButton();
  SendBrightnessChange(kNonZeroBrightness, kUserCause);
  EXPECT_TRUE(power_button_test_api_->TriggerPowerButtonMenuTimeout());
  ReleasePowerButton();
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  // Tapping the power button will dismiss the menu.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  SendBrightnessChange(0, kUserCause);
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests that suspend will dismiss the opened menu.
TEST_F(PowerButtonControllerTest, SuspendWithMenuOn) {
  OpenPowerButtonMenu();
  power_manager_client_->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
  power_manager_client_->SendSuspendDone();
  EXPECT_FALSE(power_button_test_api_->IsMenuOpened());
}

// Tests that the menu is inactive by default.
TEST_F(PowerButtonControllerTest, InactivePowerMenuByDefault) {
  aura::Window* window = CreateTestWindowInShellWithId(0);
  wm::ActivateWindow(window);
  ASSERT_EQ(window, wm::GetActiveWindow());

  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->TriggerPowerButtonMenuTimeout());
  ReleasePowerButton();
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  EXPECT_EQ(window, wm::GetActiveWindow());
  EXPECT_FALSE(
      wm::IsActiveWindow(power_button_test_api_->GetPowerButtonMenuView()
                             ->GetWidget()
                             ->GetNativeWindow()));
}

// Tests that cursor is hidden after show the menu and should reappear if mouse
// moves.
TEST_F(PowerButtonControllerTest, HideCursorAfterShowMenu) {
  // Cursor is hidden after show the menu.
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  PressPowerButton();
  EXPECT_TRUE(power_button_test_api_->TriggerPowerButtonMenuTimeout());
  ReleasePowerButton();
  EXPECT_TRUE(power_button_test_api_->IsMenuOpened());
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  // Cursor reappears if mouse moves.
  GenerateMouseMoveEvent();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
}

class PowerButtonControllerWithPositionTest
    : public PowerButtonControllerTest,
      public testing::WithParamInterface<PowerButtonPosition> {
 public:
  PowerButtonControllerWithPositionTest() : power_button_position_(GetParam()) {
    base::DictionaryValue position_info;
    switch (power_button_position_) {
      case PowerButtonPosition::LEFT:
        position_info.SetString(PowerButtonController::kPositionField,
                                PowerButtonController::kLeftPosition);
        break;
      case PowerButtonPosition::RIGHT:
        position_info.SetString(PowerButtonController::kPositionField,
                                PowerButtonController::kRightPosition);
        break;
      case PowerButtonPosition::TOP:
        position_info.SetString(PowerButtonController::kPositionField,
                                PowerButtonController::kTopPosition);
        break;
      case PowerButtonPosition::BOTTOM:
        position_info.SetString(PowerButtonController::kPositionField,
                                PowerButtonController::kBottomPosition);
        break;
      default:
        return;
    }
    if (IsLeftOrRightPosition()) {
      position_info.SetDouble(PowerButtonController::kYField,
                              kPowerButtonPercentage);
    } else {
      position_info.SetDouble(PowerButtonController::kXField,
                              kPowerButtonPercentage);
    }

    std::string json_position_info;
    base::JSONWriter::Write(position_info, &json_position_info);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAshPowerButtonPosition, json_position_info);
  }

  bool IsLeftOrRightPosition() const {
    return power_button_position_ == PowerButtonPosition::LEFT ||
           power_button_position_ == PowerButtonPosition::RIGHT;
  }

  // Returns true if it is in tablet mode.
  bool IsTabletMode() const {
    return Shell::Get()
        ->tablet_mode_controller()
        ->IsTabletModeWindowManagerEnabled();
  }

  // Returns true if the menu is at the center of the display.
  bool IsMenuCentered() const {
    return power_button_test_api_->GetMenuBoundsInScreen().CenterPoint() ==
           display::Screen::GetScreen()
               ->GetPrimaryDisplay()
               .bounds()
               .CenterPoint();
  }

  PowerButtonPosition power_button_position() const {
    return power_button_position_;
  }

 private:
  PowerButtonPosition power_button_position_;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonControllerWithPositionTest);
};

TEST_P(PowerButtonControllerWithPositionTest,
       MenuNextToPowerButtonInTabletMode) {
  std::string display =
      std::to_string(kDisplayWidth) + "x" + std::to_string(kDisplayHeight);
  UpdateDisplay(display);
  display::test::ScopedSetInternalDisplayId set_internal(
      display_manager(), GetPrimaryDisplay().id());

  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  // Set the screen orientation to LANDSCAPE_PRIMARY.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapePrimary);

  // Menu is set at the center of the display if it is not in tablet mode.
  OpenPowerButtonMenu();
  ASSERT_FALSE(IsTabletMode());
  EXPECT_TRUE(IsMenuCentered());
  TapToDismissPowerButtonMenu();

  int animation_transform = PowerButtonMenuView::kMenuViewTransformDistanceDp;
  EnableTabletMode(true);
  EXPECT_TRUE(IsTabletMode());
  OpenPowerButtonMenu();
  EXPECT_FALSE(IsMenuCentered());
  if (power_button_position() == PowerButtonPosition::LEFT) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().x());
  } else if (power_button_position() == PowerButtonPosition::RIGHT) {
    EXPECT_EQ(animation_transform,
              kDisplayWidth -
                  power_button_test_api_->GetMenuBoundsInScreen().right());
  } else if (power_button_position() == PowerButtonPosition::TOP) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().y());
  } else if (power_button_position() == PowerButtonPosition::BOTTOM) {
    EXPECT_EQ(animation_transform,
              kDisplayHeight -
                  power_button_test_api_->GetMenuBoundsInScreen().bottom());
  }

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitPrimary);
  EXPECT_FALSE(IsMenuCentered());
  if (power_button_position() == PowerButtonPosition::LEFT) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().y());
  } else if (power_button_position() == PowerButtonPosition::RIGHT) {
    EXPECT_EQ(animation_transform,
              kDisplayWidth -
                  power_button_test_api_->GetMenuBoundsInScreen().bottom());
  } else if (power_button_position() == PowerButtonPosition::TOP) {
    EXPECT_EQ(animation_transform,
              kDisplayHeight -
                  power_button_test_api_->GetMenuBoundsInScreen().right());
  } else if (power_button_position() == PowerButtonPosition::BOTTOM) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().x());
  }

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapeSecondary);
  EXPECT_FALSE(IsMenuCentered());
  if (power_button_position() == PowerButtonPosition::LEFT) {
    EXPECT_EQ(animation_transform,
              kDisplayWidth -
                  power_button_test_api_->GetMenuBoundsInScreen().right());
  } else if (power_button_position() == PowerButtonPosition::RIGHT) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().x());
  } else if (power_button_position() == PowerButtonPosition::TOP) {
    EXPECT_EQ(animation_transform,
              kDisplayHeight -
                  power_button_test_api_->GetMenuBoundsInScreen().bottom());
  } else if (power_button_position() == PowerButtonPosition::BOTTOM) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().y());
  }

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitSecondary);
  EXPECT_FALSE(IsMenuCentered());
  if (power_button_position() == PowerButtonPosition::LEFT) {
    EXPECT_EQ(animation_transform,
              kDisplayWidth -
                  power_button_test_api_->GetMenuBoundsInScreen().bottom());
  } else if (power_button_position() == PowerButtonPosition::RIGHT) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().y());
  } else if (power_button_position() == PowerButtonPosition::TOP) {
    EXPECT_EQ(animation_transform,
              power_button_test_api_->GetMenuBoundsInScreen().x());
  } else if (power_button_position() == PowerButtonPosition::BOTTOM) {
    EXPECT_EQ(animation_transform,
              kDisplayHeight -
                  power_button_test_api_->GetMenuBoundsInScreen().right());
  }
}

// Tests that the menu is always shown at the percentage of position when
// display has different scale factors.
TEST_P(PowerButtonControllerWithPositionTest, MenuShownAtPercentageOfPosition) {
  const int scale_factor = 2;
  std::string display = "1000x500*" + std::to_string(scale_factor);
  UpdateDisplay(display);
  int64_t primary_id = GetPrimaryDisplay().id();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         primary_id);
  ASSERT_EQ(scale_factor, GetPrimaryDisplay().device_scale_factor());

  EnableTabletMode(true);
  OpenPowerButtonMenu();
  EXPECT_FALSE(IsMenuCentered());
  gfx::Point menu_center_point =
      power_button_test_api_->GetMenuBoundsInScreen().CenterPoint();
  gfx::Rect display_bounds = GetPrimaryDisplay().bounds();
  int original_width = display_bounds.width();
  int original_height = display_bounds.height();
  if (IsLeftOrRightPosition()) {
    EXPECT_EQ(menu_center_point.y(), static_cast<int>(display_bounds.height() *
                                                      kPowerButtonPercentage));
  } else {
    EXPECT_EQ(menu_center_point.x(), static_cast<int>(display_bounds.width() *
                                                      kPowerButtonPercentage));
  }
  TapToDismissPowerButtonMenu();

  ASSERT_TRUE(display::test::DisplayManagerTestApi(display_manager())
                  .SetDisplayUIScale(primary_id, scale_factor));
  ASSERT_EQ(1.0f, GetPrimaryDisplay().device_scale_factor());
  display_bounds = GetPrimaryDisplay().bounds();
  int scale_up_width = display_bounds.width();
  int scale_up_height = display_bounds.height();
  EXPECT_EQ(scale_up_width, original_width * scale_factor);
  EXPECT_EQ(scale_up_height, original_height * scale_factor);
  OpenPowerButtonMenu();
  menu_center_point =
      power_button_test_api_->GetMenuBoundsInScreen().CenterPoint();
  // Menu is still at the kPowerButtonPercentage position after scale up screen.
  if (IsLeftOrRightPosition()) {
    EXPECT_EQ(menu_center_point.y(), static_cast<int>(display_bounds.height() *
                                                      kPowerButtonPercentage));
  } else {
    EXPECT_EQ(menu_center_point.x(), static_cast<int>(display_bounds.width() *
                                                      kPowerButtonPercentage));
  }
}

INSTANTIATE_TEST_CASE_P(AshPowerButtonPosition,
                        PowerButtonControllerWithPositionTest,
                        testing::Values(PowerButtonPosition::LEFT,
                                        PowerButtonPosition::RIGHT,
                                        PowerButtonPosition::TOP,
                                        PowerButtonPosition::BOTTOM));

}  // namespace ash
