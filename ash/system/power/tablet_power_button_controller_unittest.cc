// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tablet_power_button_controller.h"

#include <memory>

#include "ash/ash_switches.h"
#include "ash/public/cpp/config.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/lock_state_controller_test_api.h"
#include "ash/test/test_session_controller_client.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/wm/power_button_controller.h"
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
namespace test {

namespace {

// A non-zero brightness used for test.
constexpr int kNonZeroBrightness = 10;

void CopyResult(bool* dest, bool src) {
  *dest = src;
}

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
        switches::kAshEnableTouchView);
    AshTestBase::SetUp();
    // Trigger an accelerometer update so that |tablet_controller_| can be
    // initialized.
    SendAccelerometerUpdate();
    tablet_controller_ = Shell::Get()
                             ->power_button_controller()
                             ->tablet_power_button_controller_for_test();

    lock_state_controller_ = Shell::Get()->lock_state_controller();
    test_api_ = base::MakeUnique<TabletPowerButtonController::TestApi>(
        tablet_controller_);
    lock_state_test_api_ =
        base::MakeUnique<LockStateControllerTestApi>(lock_state_controller_);
    tick_clock_ = new base::SimpleTestTickClock;
    tablet_controller_->SetTickClockForTesting(
        std::unique_ptr<base::TickClock>(tick_clock_));
    shell_delegate_ =
        static_cast<TestShellDelegate*>(Shell::Get()->shell_delegate());
    generator_ = &AshTestBase::GetEventGenerator();
    power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, false);
    EXPECT_FALSE(GetBacklightsForcedOff());
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
  void SendAccelerometerUpdate() {
    scoped_refptr<chromeos::AccelerometerUpdate> update(
        new chromeos::AccelerometerUpdate());
    update->Set(chromeos::ACCELEROMETER_SOURCE_SCREEN, 1.0f, 0.0f, 0.0f);
    Shell::Get()->power_button_controller()->OnAccelerometerUpdated(update);
  }

  void PressPowerButton() {
    tablet_controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  }

  void ReleasePowerButton() {
    tablet_controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  }

  void UnlockScreen() {
    lock_state_controller_->OnLockStateChanged(false);
    GetSessionControllerClient()->UnlockScreen();
  }

  void Initialize(LoginStatus status) {
    SetUserLoggedIn(status != LoginStatus::NOT_LOGGED_IN);
  }

  void EnableMaximizeMode(bool enabled) {
    Shell::Get()->maximize_mode_controller()->EnableMaximizeModeWindowManager(
        enabled);
  }

  bool GetLockedState() {
    // LockScreen is an async mojo call. Spin message loop to ensure it is
    // delivered.
    SessionController* const session_controller =
        Shell::Get()->session_controller();
    session_controller->FlushMojoForTest();
    return session_controller->IsScreenLocked();
  }

  bool GetBacklightsForcedOff() WARN_UNUSED_RESULT {
    bool forced_off = false;
    power_manager_client_->GetBacklightsForcedOff(
        base::Bind(&CopyResult, base::Unretained(&forced_off)));
    base::RunLoop().RunUntilIdle();
    return forced_off;
  }

  // Ownership is passed on to chromeos::DBusThreadManager.
  chromeos::FakePowerManagerClient* power_manager_client_;

  LockStateController* lock_state_controller_;      // Not owned.
  TabletPowerButtonController* tablet_controller_;  // Not owned.
  std::unique_ptr<TabletPowerButtonController::TestApi> test_api_;
  std::unique_ptr<LockStateControllerTestApi> lock_state_test_api_;
  base::SimpleTestTickClock* tick_clock_;  // Not owned.
  TestShellDelegate* shell_delegate_;      // Not owned.
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
  EXPECT_FALSE(GetBacklightsForcedOff());
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, false);
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  EXPECT_TRUE(GetBacklightsForcedOff());

  PressPowerButton();
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, false);
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(GetBacklightsForcedOff());
  ReleasePowerButton();
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(GetBacklightsForcedOff());
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
  EXPECT_FALSE(GetBacklightsForcedOff());

  // Test again when backlights is forced off.
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, false);
  EXPECT_TRUE(GetBacklightsForcedOff());

  PressPowerButton();
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, false);
  EXPECT_FALSE(GetBacklightsForcedOff());
  test_api_->TriggerShutdownTimeout();
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  ReleasePowerButton();
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  EXPECT_FALSE(GetBacklightsForcedOff());
}

// Tests tapping power button when screen is idle off.
TEST_F(TabletPowerButtonControllerTest, TappingPowerButtonWhenScreenIsIdleOff) {
  power_manager_client_->SendBrightnessChanged(0, false);
  PressPowerButton();
  EXPECT_FALSE(GetBacklightsForcedOff());
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, false);
  ReleasePowerButton();
  EXPECT_FALSE(GetBacklightsForcedOff());
}

// Tests tapping power button when device is suspended without backlights forced
// off.
TEST_F(TabletPowerButtonControllerTest,
       TappingPowerButtonWhenSuspendedWithoutBacklightsForcedOff) {
  power_manager_client_->SendSuspendImminent();
  power_manager_client_->SendBrightnessChanged(0, false);
  // There is a power button pressed here, but PowerButtonEvent is sent later.
  power_manager_client_->SendSuspendDone();
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, false);

  // Send the power button event after a short delay and check that backlights
  // are not forced off.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(500));
  power_manager_client_->SendPowerButtonEvent(true, tick_clock_->NowTicks());
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  power_manager_client_->SendPowerButtonEvent(false, tick_clock_->NowTicks());
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(GetBacklightsForcedOff());

  // Send the power button event after a longer delay and check that backlights
  // are forced off.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(1600));
  power_manager_client_->SendPowerButtonEvent(true, tick_clock_->NowTicks());
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  power_manager_client_->SendPowerButtonEvent(false, tick_clock_->NowTicks());
  power_manager_client_->SendBrightnessChanged(0, false);
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  EXPECT_TRUE(GetBacklightsForcedOff());
}

// Tests tapping power button when device is suspended with backlights forced
// off.
TEST_F(TabletPowerButtonControllerTest,
       TappingPowerButtonWhenSuspendedWithBacklightsForcedOff) {
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, false);
  ASSERT_TRUE(GetBacklightsForcedOff());
  power_manager_client_->SendSuspendImminent();
  // There is a power button pressed here, but PowerButtonEvent is sent later.
  // Because of backlights forced off, resuming system will not restore
  // brightness.
  power_manager_client_->SendSuspendDone();

  // Send the power button event after a short delay and check that backlights
  // are not forced off.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(500));
  power_manager_client_->SendPowerButtonEvent(true, tick_clock_->NowTicks());
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, false);
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  power_manager_client_->SendPowerButtonEvent(false, tick_clock_->NowTicks());
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  EXPECT_FALSE(GetBacklightsForcedOff());

  // Send the power button event after a longer delay and check that backlights
  // are forced off.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(1600));
  power_manager_client_->SendPowerButtonEvent(true, tick_clock_->NowTicks());
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  power_manager_client_->SendPowerButtonEvent(false, tick_clock_->NowTicks());
  power_manager_client_->SendBrightnessChanged(0, false);
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  EXPECT_TRUE(GetBacklightsForcedOff());
}

// For convertible device working on laptop mode, tests keyboard/mouse event
// when screen is off.
TEST_F(TabletPowerButtonControllerTest, ConvertibleOnLaptopMode) {
  EnableMaximizeMode(false);

  // KeyEvent should SetBacklightsForcedOff(false).
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, false);
  ASSERT_TRUE(GetBacklightsForcedOff());
  generator_->PressKey(ui::VKEY_L, ui::EF_NONE);
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, false);
  EXPECT_FALSE(GetBacklightsForcedOff());

  // Regular mouse event should SetBacklightsForcedOff(false).
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, false);
  ASSERT_TRUE(GetBacklightsForcedOff());
  generator_->MoveMouseBy(1, 1);
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, false);
  EXPECT_FALSE(GetBacklightsForcedOff());

  // Synthesized mouse event should not SetBacklightsForcedOff(false).
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, false);
  ASSERT_TRUE(GetBacklightsForcedOff());
  generator_->set_flags(ui::EF_IS_SYNTHESIZED);
  generator_->MoveMouseBy(1, 1);
  generator_->set_flags(ui::EF_NONE);
  EXPECT_TRUE(GetBacklightsForcedOff());
}

// For convertible device working on tablet mode, keyboard/mouse event should
// not SetBacklightsForcedOff(false) when screen is off.
TEST_F(TabletPowerButtonControllerTest, ConvertibleOnMaximizeMode) {
  EnableMaximizeMode(true);

  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, false);
  ASSERT_TRUE(GetBacklightsForcedOff());
  generator_->PressKey(ui::VKEY_L, ui::EF_NONE);
  EXPECT_TRUE(GetBacklightsForcedOff());

  generator_->MoveMouseBy(1, 1);
  EXPECT_TRUE(GetBacklightsForcedOff());
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
  tablet_controller_->OnKeyEvent(&power_key_pressed);
  tablet_controller_->OnKeyEvent(&power_key_pressed);
  PressPowerButton();
  tablet_controller_->OnKeyEvent(&power_key_pressed);
  tablet_controller_->OnKeyEvent(&power_key_pressed);
  tablet_controller_->OnKeyEvent(&power_key_pressed);
  ReleasePowerButton();
  tablet_controller_->OnKeyEvent(&power_key_released);
  tablet_controller_->OnKeyEvent(&power_key_released);
  EXPECT_EQ(1, power_manager_client_->num_set_backlights_forced_off_calls());
}

// Tests that under (1) tablet power button pressed/released, (2) keyboard/mouse
// events on laptop mode when screen is off, requesting/stopping backlights
// forced off should also set corresponding touchscreen state in local pref.
TEST_F(TabletPowerButtonControllerTest, TouchscreenState) {
  // Tests tablet power button.
  ASSERT_TRUE(shell_delegate_->IsTouchscreenEnabledInPrefs(true));
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, false);
  EXPECT_FALSE(shell_delegate_->IsTouchscreenEnabledInPrefs(true));

  PressPowerButton();
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, false);
  ReleasePowerButton();
  EXPECT_TRUE(shell_delegate_->IsTouchscreenEnabledInPrefs(true));

  EnableMaximizeMode(false);
  // KeyEvent on laptop mode when screen is off.
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, false);
  ASSERT_TRUE(GetBacklightsForcedOff());
  ASSERT_FALSE(shell_delegate_->IsTouchscreenEnabledInPrefs(true));
  generator_->PressKey(ui::VKEY_L, ui::EF_NONE);
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, false);
  EXPECT_TRUE(shell_delegate_->IsTouchscreenEnabledInPrefs(true));

  // MouseEvent on laptop mode when screen is off.
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, false);
  ASSERT_TRUE(GetBacklightsForcedOff());
  ASSERT_FALSE(shell_delegate_->IsTouchscreenEnabledInPrefs(true));
  generator_->MoveMouseBy(1, 1);
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, false);
  EXPECT_TRUE(shell_delegate_->IsTouchscreenEnabledInPrefs(true));
}

// When user switches convertible device between laptop mode and tablet mode,
// power button may be pressed and held, which may cause unwanted shutdown.
TEST_F(TabletPowerButtonControllerTest,
       EnterOrLeaveMaximizeModeWhilePressingPowerButton) {
  Initialize(LoginStatus::USER);
  SetShouldLockScreenAutomatically(true);
  ASSERT_FALSE(GetLockedState());

  power_manager_client_->SendPowerButtonEvent(true, tick_clock_->NowTicks());
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  tablet_controller_->OnMaximizeModeStarted();
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(1500));
  power_manager_client_->SendPowerButtonEvent(false, tick_clock_->NowTicks());
  EXPECT_FALSE(GetLockedState());
  EXPECT_FALSE(GetBacklightsForcedOff());

  power_manager_client_->SendPowerButtonEvent(true, tick_clock_->NowTicks());
  test_api_->TriggerShutdownTimeout();
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  tablet_controller_->OnMaximizeModeStarted();
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(2500));
  power_manager_client_->SendPowerButtonEvent(false, tick_clock_->NowTicks());
  EXPECT_FALSE(GetLockedState());
  EXPECT_FALSE(GetBacklightsForcedOff());

  power_manager_client_->SendPowerButtonEvent(true, tick_clock_->NowTicks());
  EXPECT_TRUE(test_api_->ShutdownTimerIsRunning());
  tablet_controller_->OnMaximizeModeEnded();
  EXPECT_FALSE(test_api_->ShutdownTimerIsRunning());
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(3500));
  power_manager_client_->SendPowerButtonEvent(false, tick_clock_->NowTicks());
  EXPECT_FALSE(GetLockedState());
  EXPECT_FALSE(GetBacklightsForcedOff());

  power_manager_client_->SendPowerButtonEvent(true, tick_clock_->NowTicks());
  test_api_->TriggerShutdownTimeout();
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  tablet_controller_->OnMaximizeModeEnded();
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(4500));
  power_manager_client_->SendPowerButtonEvent(false, tick_clock_->NowTicks());
  EXPECT_FALSE(GetLockedState());
  EXPECT_FALSE(GetBacklightsForcedOff());
}

// Tests that repeated power button releases are ignored (crbug.com/675291).
TEST_F(TabletPowerButtonControllerTest, IgnoreRepeatedPowerButtonReleases) {
  // Advance a long duration from initialized last resume time in
  // |tablet_controller_| to avoid cross interference.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(2000));

  // Set backlights forced off for starting point.
  PressPowerButton();
  ReleasePowerButton();
  power_manager_client_->SendBrightnessChanged(0, false);
  ASSERT_TRUE(GetBacklightsForcedOff());

  // Test that a pressing-releasing operation after a short duration, backlights
  // forced off is stopped since we don't drop request for power button pressed.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(200));
  power_manager_client_->SendPowerButtonEvent(true, tick_clock_->NowTicks());
  power_manager_client_->SendBrightnessChanged(kNonZeroBrightness, false);
  power_manager_client_->SendPowerButtonEvent(false, tick_clock_->NowTicks());
  EXPECT_FALSE(GetBacklightsForcedOff());

  // Test that after another short duration, backlights will not be forced off
  // since this immediately following forcing off request needs to be dropped.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(200));
  power_manager_client_->SendPowerButtonEvent(true, tick_clock_->NowTicks());
  power_manager_client_->SendPowerButtonEvent(false, tick_clock_->NowTicks());
  EXPECT_FALSE(GetBacklightsForcedOff());

  // Test that after another long duration, backlights should be forced off.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(800));
  power_manager_client_->SendPowerButtonEvent(true, tick_clock_->NowTicks());
  power_manager_client_->SendPowerButtonEvent(false, tick_clock_->NowTicks());
  power_manager_client_->SendBrightnessChanged(0, false);
  EXPECT_TRUE(GetBacklightsForcedOff());
}

// Tests that lid closed/open events stop forcing off backlights.
TEST_F(TabletPowerButtonControllerTest, LidEventsStopForcingOff) {
  // Pressing/releasing power button to set backlights forced off.
  PressPowerButton();
  ReleasePowerButton();
  ASSERT_TRUE(GetBacklightsForcedOff());

  // A lid closed event is received, we should stop forcing off backlights.
  power_manager_client_->SetLidState(
      chromeos::PowerManagerClient::LidState::CLOSED, tick_clock_->NowTicks());
  EXPECT_FALSE(GetBacklightsForcedOff());

  // Pressing/releasing power button again to set backlights forced off. This is
  // for testing purpose. In real life, powerd would not repond to this event
  // with lid closed state.
  PressPowerButton();
  ReleasePowerButton();
  ASSERT_TRUE(GetBacklightsForcedOff());

  // A lid open event is received, we should stop forcing off backlights.
  power_manager_client_->SetLidState(
      chromeos::PowerManagerClient::LidState::OPEN, tick_clock_->NowTicks());
  EXPECT_FALSE(GetBacklightsForcedOff());
}

// Tests that with system reboot, the local state of touchscreen enabled state
// should be synced with new backlights forced off state from powerd.
TEST_F(TabletPowerButtonControllerTest, SyncTouchscreenStatus) {
  shell_delegate_->SetTouchscreenEnabledInPrefs(false,
                                                true /* use_local_state */);
  ASSERT_FALSE(shell_delegate_->IsTouchscreenEnabledInPrefs(true));

  // Simulate system reboot by resetting backlights forced off state in powerd
  // and TabletPowerButtonController.
  power_manager_client_->SetBacklightsForcedOff(false);
  Shell::Get()
      ->power_button_controller()
      ->ResetTabletPowerButtonControllerForTest();
  SendAccelerometerUpdate();

  // Check that the local state of touchscreen enabled state is in line with
  // backlights forced off state.
  EXPECT_FALSE(GetBacklightsForcedOff());
  EXPECT_TRUE(shell_delegate_->IsTouchscreenEnabledInPrefs(true));
}

// Tests that tablet power button behavior is enabled on having seen
// accelerometer update, otherwise it is disabled.
TEST_F(TabletPowerButtonControllerTest, EnableOnAccelerometerUpdate) {
  ASSERT_TRUE(tablet_controller_);
  Shell::Get()
      ->power_button_controller()
      ->ResetTabletPowerButtonControllerForTest();
  tablet_controller_ = Shell::Get()
                           ->power_button_controller()
                           ->tablet_power_button_controller_for_test();
  EXPECT_FALSE(tablet_controller_);

  SendAccelerometerUpdate();
  tablet_controller_ = Shell::Get()
                           ->power_button_controller()
                           ->tablet_power_button_controller_for_test();
  EXPECT_TRUE(tablet_controller_);
}

}  // namespace test
}  // namespace ash
