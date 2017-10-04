// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tablet_power_button_controller.h"

#include <memory>

#include "ash/media_controller.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/config.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_media_client.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/lock_state_controller_test_api.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/test_session_state_animator.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

// A non-zero brightness used for test.
constexpr int kNonZeroBrightness = 10;

// Vector pointing up (e.g. keyboard in clamshell).
constexpr gfx::Vector3dF kUpVector = {0, 0,
                                      TabletPowerButtonController::kGravity};

// Vector pointing down (e.g. keyboard in tablet sitting on table).
constexpr gfx::Vector3dF kDownVector = {0, 0,
                                        -TabletPowerButtonController::kGravity};

// Vector pointing sideways (e.g. screen in 90-degree clamshell).
constexpr gfx::Vector3dF kSidewaysVector = {
    0, TabletPowerButtonController::kGravity, 0};

}  // namespace

class TabletPowerButtonControllerTest : public AshTestBase {
 public:
  TabletPowerButtonControllerTest() {}
  ~TabletPowerButtonControllerTest() override {}

  void SetUp() override {
    // This also initializes DBusThreadManager.
    std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    power_manager_client_ = new chromeos::FakePowerManagerClient();
    dbus_setter->SetPowerManagerClient(base::WrapUnique(power_manager_client_));
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnableTabletMode);
    AshTestBase::SetUp();

    InitPowerButtonControllerMembers(true /* send_accelerometer_update */);

    lock_state_controller_ = Shell::Get()->lock_state_controller();
    lock_state_test_api_ =
        std::make_unique<LockStateControllerTestApi>(lock_state_controller_);
    generator_ = &AshTestBase::GetEventGenerator();

    power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, true);
    EXPECT_FALSE(power_manager_client_->backlights_forced_off());

    // Advance a long duration from initialized last resume time in
    // |tablet_controller_| to avoid cross interference.
    tick_clock_->Advance(base::TimeDelta::FromMilliseconds(3000));

    // Run the event loop so that PowerButtonDisplayController can receive the
    // initial backlights-forced-off state.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    generator_ = nullptr;
    const Config config = Shell::GetAshConfig();
    AshTestBase::TearDown();
    // Mash/mus shuts down dbus after each test.
    if (config == Config::CLASSIC)
      chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  // Resets the PowerButtonController and associated members.
  void ResetPowerButtonController() {
    ShellTestApi shell_test_api;
    shell_test_api.ResetPowerButtonControllerForTest();
    InitPowerButtonControllerMembers(false /* send_accelerometer_update */);
  }

  // Initializes |power_button_controller_| and other members that point at
  // objects owned by it. If |send_accelerometer_update| is true, an
  // accelerometer update is sent so that a TabletPowerButtonController will be
  // created.
  void InitPowerButtonControllerMembers(bool send_accelerometer_update) {
    power_button_controller_ = Shell::Get()->power_button_controller();
    tick_clock_ = new base::SimpleTestTickClock;
    power_button_controller_->SetTickClockForTesting(
        base::WrapUnique(tick_clock_));

    if (send_accelerometer_update) {
      SendAccelerometerUpdate(kSidewaysVector, kUpVector);
      tablet_controller_ =
          power_button_controller_->tablet_power_button_controller_for_test();
      test_api_ = std::make_unique<TabletPowerButtonController::TestApi>(
          tablet_controller_);
    } else {
      test_api_ = nullptr;
      tablet_controller_ = nullptr;
    }
  }

  // Sends an update with screen and keyboard accelerometer readings to
  // PowerButtonController, and also |tablet_controller_| if it's non-null and
  // has registered as an observer.
  void SendAccelerometerUpdate(const gfx::Vector3dF& screen,
                               const gfx::Vector3dF& keyboard) {
    scoped_refptr<chromeos::AccelerometerUpdate> update(
        new chromeos::AccelerometerUpdate());
    update->Set(chromeos::ACCELEROMETER_SOURCE_SCREEN, screen.x(), screen.y(),
                screen.z());
    update->Set(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, keyboard.x(),
                keyboard.y(), keyboard.z());

    power_button_controller_->OnAccelerometerUpdated(update);
    // |tablet_controller_| may get initialized from nullptr.
    tablet_controller_ =
        power_button_controller_->tablet_power_button_controller_for_test();

    if (test_api_ && test_api_->IsObservingAccelerometerReader(
                         chromeos::AccelerometerReader::GetInstance()))
      tablet_controller_->OnAccelerometerUpdated(update);
  }

  void PressPowerButton() {
    power_button_controller_->OnPowerButtonEvent(true, tick_clock_->NowTicks());
  }

  void ReleasePowerButton() {
    power_button_controller_->OnPowerButtonEvent(false,
                                                 tick_clock_->NowTicks());
  }

  void UnlockScreen() {
    lock_state_controller_->OnLockStateChanged(false);
    GetSessionControllerClient()->UnlockScreen();
  }

  void Initialize(LoginStatus status) {
    if (status == LoginStatus::NOT_LOGGED_IN) {
      ClearLogin();
    } else {
      CreateUserSessions(1);
    }
  }

  void EnableTabletMode(bool enabled) {
    Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(
        enabled);
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

  // Advance clock to ensure the intended tablet power button display forcing
  // off is not ignored since we will ignore the repeated power button up if
  // they come too close.
  void AdvanceClockToAvoidIgnoring() {
    tick_clock_->Advance(
        TabletPowerButtonController::kIgnoreRepeatedButtonUpDelay +
        base::TimeDelta::FromMilliseconds(1));
  }

  // Ownership is passed on to chromeos::DBusThreadManager.
  chromeos::FakePowerManagerClient* power_manager_client_ = nullptr;

  PowerButtonController* power_button_controller_ = nullptr;  // Not owned.
  LockStateController* lock_state_controller_ = nullptr;      // Not owned.
  TabletPowerButtonController* tablet_controller_ = nullptr;  // Not owned.
  std::unique_ptr<TabletPowerButtonController::TestApi> test_api_;
  std::unique_ptr<LockStateControllerTestApi> lock_state_test_api_;
  base::SimpleTestTickClock* tick_clock_ = nullptr;  // Not owned.
  ui::test::EventGenerator* generator_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TabletPowerButtonControllerTest);
};

TEST_F(TabletPowerButtonControllerTest, LockScreenIfRequired) {
  Initialize(LoginStatus::USER);
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

// Tests that shutdown animation is not started if the power button is released
// quickly.
TEST_F(TabletPowerButtonControllerTest,
       ReleasePowerButtonBeforeStartingShutdownAnimation) {
  PressPowerButton();
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());

  PressPowerButton();
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, true);
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  ReleasePowerButton();
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
}

// Tests that the shutdown animation is started when the power button is
// released after the timer fires.
TEST_F(TabletPowerButtonControllerTest,
       ReleasePowerButtonDuringShutdownAnimation) {
  PressPowerButton();
  test_api_->TriggerShutdownTimeout();
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  ReleasePowerButton();
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Test again when backlights is forced off.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());

  PressPowerButton();
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, true);
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  test_api_->TriggerShutdownTimeout();
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  ReleasePowerButton();
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
}

// Tests tapping power button when screen is idle off.
TEST_F(TabletPowerButtonControllerTest, TappingPowerButtonWhenScreenIsIdleOff) {
  power_manager_client_->SendBrightnessChanged(0, true);
  PressPowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, true);
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
}

// Tests tapping power button when device is suspended without backlights forced
// off.
TEST_F(TabletPowerButtonControllerTest,
       TappingPowerButtonWhenSuspendedWithoutBacklightsForcedOff) {
  power_manager_client_->SendSuspendImminent();
  power_manager_client_->SendBrightnessChanged(0, true);
  // There is a power button pressed here, but PowerButtonEvent is sent later.
  power_manager_client_->SendSuspendDone();
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, true);

  // Send the power button event after a short delay and check that backlights
  // are not forced off.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(500));
  PressPowerButton();
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  ReleasePowerButton();
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Send the power button event after a longer delay and check that backlights
  // are forced off.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(1600));
  PressPowerButton();
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
}

// Tests tapping power button when device is suspended with backlights forced
// off.
TEST_F(TabletPowerButtonControllerTest,
       TappingPowerButtonWhenSuspendedWithBacklightsForcedOff) {
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  power_manager_client_->SendSuspendImminent();
  // There is a power button pressed here, but PowerButtonEvent is sent later.
  // Because of backlights forced off, resuming system will not restore
  // brightness.
  power_manager_client_->SendSuspendDone();

  // Send the power button event after a short delay and check that backlights
  // are not forced off.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(500));
  PressPowerButton();
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, true);
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  ReleasePowerButton();
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Send the power button event after a longer delay and check that backlights
  // are forced off.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(1600));
  PressPowerButton();
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
}

// For convertible device working on laptop mode, tests keyboard/mouse event
// when screen is off.
TEST_F(TabletPowerButtonControllerTest, ConvertibleOnLaptopMode) {
  EnableTabletMode(false);

  // KeyEvent should SetBacklightsForcedOff(false).
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  generator_->PressKey(ui::VKEY_L, ui::EF_NONE);
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, true);
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Regular mouse event should SetBacklightsForcedOff(false).
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  generator_->MoveMouseBy(1, 1);
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, true);
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Synthesized mouse event should not SetBacklightsForcedOff(false).
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  generator_->set_flags(ui::EF_IS_SYNTHESIZED);
  generator_->MoveMouseBy(1, 1);
  generator_->set_flags(ui::EF_NONE);
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
}

// For convertible device working on tablet mode, keyboard/mouse event should
// not SetBacklightsForcedOff(false) when screen is off.
TEST_F(TabletPowerButtonControllerTest, ConvertibleOnTabletMode) {
  EnableTabletMode(true);

  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  generator_->PressKey(ui::VKEY_L, ui::EF_NONE);
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());

  generator_->MoveMouseBy(1, 1);
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
}

// Tests that a single set of power button pressed-and-released operation should
// cause only one SetBacklightsForcedOff call.
TEST_F(TabletPowerButtonControllerTest, IgnorePowerOnKeyEvent) {
  ui::KeyEvent power_key_pressed(ui::ET_KEY_PRESSED, ui::VKEY_POWER,
                                 ui::EF_NONE);
  ui::KeyEvent power_key_released(ui::ET_KEY_RELEASED, ui::VKEY_POWER,
                                  ui::EF_NONE);

  // There are two |power_key_pressed| events and |power_key_released| events
  // generated for each pressing and releasing, and multiple repeating pressed
  // events depending on holding.
  ASSERT_EQ(0, power_manager_client_->num_set_backlights_forced_off_calls());
  test_api_->SendKeyEvent(&power_key_pressed);
  test_api_->SendKeyEvent(&power_key_pressed);
  PressPowerButton();
  test_api_->SendKeyEvent(&power_key_pressed);
  test_api_->SendKeyEvent(&power_key_pressed);
  test_api_->SendKeyEvent(&power_key_pressed);
  ReleasePowerButton();
  test_api_->SendKeyEvent(&power_key_released);
  test_api_->SendKeyEvent(&power_key_released);
  EXPECT_EQ(1, power_manager_client_->num_set_backlights_forced_off_calls());
}

// Tests that under (1) tablet power button pressed/released, (2) keyboard/mouse
// events on laptop mode when screen is off, requesting/stopping backlights
// forced off should update the global touchscreen enabled status.
TEST_F(TabletPowerButtonControllerTest, DisableTouchscreenWhileForcedOff) {
  // Tests tablet power button.
  ASSERT_TRUE(GetGlobalTouchscreenEnabled());
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  EXPECT_FALSE(GetGlobalTouchscreenEnabled());

  PressPowerButton();
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, true);
  ReleasePowerButton();
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());

  EnableTabletMode(false);
  // KeyEvent on laptop mode when screen is off.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  ASSERT_FALSE(GetGlobalTouchscreenEnabled());
  generator_->PressKey(ui::VKEY_L, ui::EF_NONE);
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, true);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());

  // MouseEvent on laptop mode when screen is off.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  ASSERT_FALSE(GetGlobalTouchscreenEnabled());
  generator_->MoveMouseBy(1, 1);
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, true);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
}

// When the screen is turned off automatically, the touchscreen should also be
// disabled.
TEST_F(TabletPowerButtonControllerTest, DisableTouchscreenForInactivity) {
  ASSERT_TRUE(GetGlobalTouchscreenEnabled());

  // Turn screen off for automated change (e.g. user is inactive).
  power_manager_client_->SendBrightnessChanged(0, false);
  EXPECT_FALSE(GetGlobalTouchscreenEnabled());
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, true);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());

  // After decreasing the brightness to zero for a user request, the touchscreen
  // should remain enabled.
  power_manager_client_->SendBrightnessChanged(0, true);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
}

// When user switches convertible device between laptop mode and tablet mode,
// power button may be pressed and held, which may cause unwanted shutdown.
TEST_F(TabletPowerButtonControllerTest,
       EnterOrLeaveTabletModeWhilePressingPowerButton) {
  Initialize(LoginStatus::USER);
  SetShouldLockScreenAutomatically(true);
  ASSERT_FALSE(GetLockedState());

  PressPowerButton();
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  tablet_controller_->OnTabletModeStarted();
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(1500));
  ReleasePowerButton();
  EXPECT_FALSE(GetLockedState());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  PressPowerButton();
  test_api_->TriggerShutdownTimeout();
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  tablet_controller_->OnTabletModeStarted();
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(2500));
  ReleasePowerButton();
  EXPECT_FALSE(GetLockedState());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  PressPowerButton();
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  tablet_controller_->OnTabletModeEnded();
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(3500));
  ReleasePowerButton();
  EXPECT_FALSE(GetLockedState());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  PressPowerButton();
  test_api_->TriggerShutdownTimeout();
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  tablet_controller_->OnTabletModeEnded();
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(4500));
  ReleasePowerButton();
  EXPECT_FALSE(GetLockedState());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
}

// Tests that repeated power button releases are ignored (crbug.com/675291).
TEST_F(TabletPowerButtonControllerTest, IgnoreRepeatedPowerButtonReleases) {
  // Set backlights forced off for starting point.
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());

  // Test that a pressing-releasing operation after a short duration, backlights
  // forced off is stopped since we don't drop request for power button pressed.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(200));
  PressPowerButton();
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, true);
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Test that after another short duration, backlights will not be forced off
  // since this immediately following forcing off request needs to be dropped.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(200));
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Test that after another long duration, backlights should be forced off.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(800));
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
}

// Tests that lid closed/open events stop forcing off backlights.
TEST_F(TabletPowerButtonControllerTest, LidEventsStopForcingOff) {
  // Pressing/releasing power button to set backlights forced off.
  PressPowerButton();
  ReleasePowerButton();
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());

  // A lid closed event is received, we should stop forcing off backlights.
  power_manager_client_->SetLidState(
      chromeos::PowerManagerClient::LidState::CLOSED, tick_clock_->NowTicks());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Pressing/releasing power button again to set backlights forced off. This is
  // for testing purpose. In real life, powerd would not repond to this event
  // with lid closed state.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());

  // A lid open event is received, we should stop forcing off backlights.
  power_manager_client_->SetLidState(
      chromeos::PowerManagerClient::LidState::OPEN, tick_clock_->NowTicks());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
}

// Tests that with system reboot, the global touchscreen enabled status should
// be synced with new backlights forced off state from powerd.
TEST_F(TabletPowerButtonControllerTest, SyncTouchscreenEnabled) {
  Shell::Get()->touch_devices_controller()->SetTouchscreenEnabled(
      false, TouchscreenEnabledSource::GLOBAL);
  ASSERT_FALSE(GetGlobalTouchscreenEnabled());

  // Simulate system reboot by resetting backlights forced off state in powerd
  // and PowerButtonController.
  power_manager_client_->SetBacklightsForcedOff(false);
  ResetPowerButtonController();
  SendAccelerometerUpdate(kSidewaysVector, kSidewaysVector);

  // Run the event loop for PowerButtonDisplayController to get backlight state
  // and check that the global touchscreen status is correct.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
}

// Tests that tablet power button behavior is enabled on having seen
// accelerometer update, otherwise it is disabled.
TEST_F(TabletPowerButtonControllerTest, EnableOnAccelerometerUpdate) {
  ASSERT_TRUE(tablet_controller_);
  ResetPowerButtonController();
  EXPECT_FALSE(tablet_controller_);

  SendAccelerometerUpdate(kSidewaysVector, kSidewaysVector);
  EXPECT_TRUE(tablet_controller_);

  // If clamshell-like power button behavior is requested via a flag, the
  // TabletPowerButtonController shouldn't be initialized in response to
  // accelerometer events.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceClamshellPowerButton);
  ResetPowerButtonController();
  SendAccelerometerUpdate(kSidewaysVector, kSidewaysVector);
  EXPECT_FALSE(tablet_controller_);
}

TEST_F(TabletPowerButtonControllerTest, IgnoreSpuriousEventsForAcceleration) {
  base::CommandLine cl(base::CommandLine::NO_PROGRAM);
  cl.AppendSwitchASCII(switches::kSpuriousPowerButtonWindow, "3");
  cl.AppendSwitchASCII(switches::kSpuriousPowerButtonAccelCount, "2");
  cl.AppendSwitchASCII(switches::kSpuriousPowerButtonKeyboardAccel, "4.5");
  cl.AppendSwitchASCII(switches::kSpuriousPowerButtonScreenAccel, "8.0");
  test_api_->ParseSpuriousPowerButtonSwitches(cl);
  ASSERT_FALSE(test_api_->IsSpuriousPowerButtonEvent());

  // Vectors with varying amounts of acceleration beyond gravity.
  static constexpr gfx::Vector3dF kVector0 = {
      0, 0, TabletPowerButtonController::kGravity};
  static constexpr gfx::Vector3dF kVector3 = {
      0, 0, TabletPowerButtonController::kGravity + 3};
  static constexpr gfx::Vector3dF kVector5 = {
      0, 0, TabletPowerButtonController::kGravity + 5};
  static constexpr gfx::Vector3dF kVector9 = {
      0, 0, TabletPowerButtonController::kGravity + 9};

  // Send two keyboard readings with vectors that exceed the threshold after
  // subtracting gravity.
  SendAccelerometerUpdate(kVector0, kVector5);
  SendAccelerometerUpdate(kVector0, kVector9);
  EXPECT_TRUE(test_api_->IsSpuriousPowerButtonEvent());

  // Now send two more keyboard readings that are close to gravity. We only have
  // one large reading saved now, so we should permit power button events again.
  SendAccelerometerUpdate(kVector0, kVector0);
  SendAccelerometerUpdate(kVector0, kVector0);
  EXPECT_FALSE(test_api_->IsSpuriousPowerButtonEvent());

  // Send a few large screen vectors and check that the button is again blocked.
  SendAccelerometerUpdate(kVector9, kVector0);
  SendAccelerometerUpdate(kVector9, kVector0);
  EXPECT_TRUE(test_api_->IsSpuriousPowerButtonEvent());
}

TEST_F(TabletPowerButtonControllerTest, IgnoreSpuriousEventsForLidAngle) {
  base::CommandLine cl(base::CommandLine::NO_PROGRAM);
  cl.AppendSwitchASCII(switches::kSpuriousPowerButtonWindow, "5");
  cl.AppendSwitchASCII(switches::kSpuriousPowerButtonLidAngleChange, "200");
  test_api_->ParseSpuriousPowerButtonSwitches(cl);
  ASSERT_FALSE(test_api_->IsSpuriousPowerButtonEvent());

  // Send two updates in tablet mode with the screen facing up and the keyboard
  // facing down (i.e. 360 degrees between the two).
  SendAccelerometerUpdate(kUpVector, kDownVector);
  SendAccelerometerUpdate(kUpVector, kDownVector);
  EXPECT_FALSE(test_api_->IsSpuriousPowerButtonEvent());

  // Now keep the screen facing up and report the keyboard as being sideways, as
  // if it's been rotated 90 degrees.
  SendAccelerometerUpdate(kUpVector, kSidewaysVector);
  EXPECT_FALSE(test_api_->IsSpuriousPowerButtonEvent());

  // Make the keyboard also face up (180 degrees from start).
  SendAccelerometerUpdate(kUpVector, kUpVector);
  EXPECT_FALSE(test_api_->IsSpuriousPowerButtonEvent());

  // Now make the screen face sideways, completing the 270-degree change to
  // a clamshell orientation. We've exceeded the threshold over the last four
  // samples, so events should be ignored.
  SendAccelerometerUpdate(kSidewaysVector, kUpVector);
  EXPECT_TRUE(test_api_->IsSpuriousPowerButtonEvent());

  // Make the screen travel 90 more degrees so the lid is closed (360 degrees
  // from start).
  SendAccelerometerUpdate(kDownVector, kUpVector);
  EXPECT_TRUE(test_api_->IsSpuriousPowerButtonEvent());

  // After two more closed samples, the 5-sample buffer just contains a
  // 180-degree transition, so events should be accepted again.
  SendAccelerometerUpdate(kDownVector, kUpVector);
  EXPECT_TRUE(test_api_->IsSpuriousPowerButtonEvent());
  SendAccelerometerUpdate(kDownVector, kUpVector);
  EXPECT_FALSE(test_api_->IsSpuriousPowerButtonEvent());
}

// Tests that when backlights get forced off due to tablet power button, media
// sessions should be suspended.
TEST_F(TabletPowerButtonControllerTest, SuspendMediaSessions) {
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
TEST_F(TabletPowerButtonControllerTest, SuspendDoneStopsForcingOff) {
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());

  // Simulate an edge case that system resumes because of tablet power button
  // pressed, but power button event is not delivered.
  power_manager_client_->SendSuspendDone();

  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
}

// Tests that for tablet power button induced locking screen, locking animations
// are immediate.
TEST_F(TabletPowerButtonControllerTest, ImmediateLockAnimations) {
  TestSessionStateAnimator* test_animator = new TestSessionStateAnimator;
  lock_state_controller_->set_animator_for_test(test_animator);
  Initialize(LoginStatus::USER);
  SetShouldLockScreenAutomatically(true);
  ASSERT_FALSE(GetLockedState());

  PressPowerButton();
  ReleasePowerButton();
  // Tests that locking animation starts.
  EXPECT_TRUE(lock_state_test_api_->is_animating_lock());

  // Tests that we have two active animation containers for pre-lock animation,
  // which are non lock screen containers and shelf container.
  EXPECT_EQ(2u, test_animator->GetAnimationCount());
  test_animator->AreContainersAnimated(
      LockStateController::kPreLockContainersMask,
      SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY);
  // Tests that after finishing immediate animation, we have no active
  // animations left.
  test_animator->Advance(test_animator->GetDuration(
      SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE));
  EXPECT_EQ(0u, test_animator->GetAnimationCount());

  // Flushes locking screen async request to start post-lock animation.
  EXPECT_TRUE(GetLockedState());
  EXPECT_TRUE(lock_state_test_api_->is_animating_lock());
  // Tests that we have two active animation container for post-lock animation,
  // which are lock screen containers and shelf container.
  EXPECT_EQ(2u, test_animator->GetAnimationCount());
  test_animator->AreContainersAnimated(
      SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
      SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN);
  test_animator->AreContainersAnimated(SessionStateAnimator::SHELF,
                                       SessionStateAnimator::ANIMATION_FADE_IN);
  // Tests that after finishing immediate animation, we have no active
  // animations left. Also checks that animation ends.
  test_animator->Advance(test_animator->GetDuration(
      SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE));
  EXPECT_EQ(0u, test_animator->GetAnimationCount());
  EXPECT_FALSE(lock_state_test_api_->is_animating_lock());
}

// Tests that updating power button behavior from tablet behavior to clamshell
// behavior will initially enable touchscreen on global touchscreen enabled
// status (b/64972736).
TEST_F(TabletPowerButtonControllerTest, TouchscreenEnabledClamshell) {
  PressPowerButton();
  ReleasePowerButton();
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());
  ASSERT_FALSE(GetGlobalTouchscreenEnabled());

  // Simulates a system reboot with |kForceClamshellPowerButton| requested by
  // flag, resetting backlights forced off state in powerd and
  // PowerButtonController.
  power_manager_client_->SetBacklightsForcedOff(false);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceClamshellPowerButton);
  ResetPowerButtonController();
  // Run the event loop for PowerButtonDisplayController to get backlight state.
  base::RunLoop().RunUntilIdle();
  SendAccelerometerUpdate(kSidewaysVector, kSidewaysVector);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
}

// Tests that during the interval that the display is turning on, tablet power
// button should not set display off (crbug.com/735225).
TEST_F(TabletPowerButtonControllerTest,
       IgnoreForcingOffWhenDisplayIsTurningOn) {
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  ASSERT_TRUE(power_manager_client_->backlights_forced_off());

  // Trigger a key event to stop backlights forcing off. Chrome will receive
  // brightness changed signal. But we may still have display off state.
  generator_->PressKey(ui::VKEY_L, ui::EF_NONE);
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, true);
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Since display could still be off, ignore forcing off.
  tick_clock_->Advance(TabletPowerButtonController::kScreenStateChangeDelay -
                       base::TimeDelta::FromMilliseconds(1));
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // After avoiding ignoring the repeated power button releases, we should be
  // able to set display off.
  AdvanceClockToAvoidIgnoring();
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, true);
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());
}

}  // namespace ash
