// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tablet_power_button_controller.h"

#include "ash/accessibility_delegate.h"
#include "ash/ash_switches.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shutdown_reason.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/chromeos/accelerometer/accelerometer_util.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event.h"

namespace ash {

namespace {

// Amount of time the power button must be held to start the pre-shutdown
// animation when in tablet mode. This differs depending on whether the screen
// is on or off when the power button is initially pressed.
constexpr int kShutdownWhenScreenOnTimeoutMs = 500;
// TODO(derat): This is currently set to a high value to work around delays in
// powerd's reports of button-up events when the preceding button-down event
// turns the display on. Set it to a lower value once powerd no longer blocks on
// asking Chrome to turn the display on: http://crbug.com/685734
constexpr int kShutdownWhenScreenOffTimeoutMs = 2000;

// Amount of time since last SuspendDone() that power button event needs to be
// ignored.
constexpr int kIgnorePowerButtonAfterResumeMs = 2000;

// Ignore button-up events occurring within this many milliseconds of the
// previous button-up event. This prevents us from falling behind if the power
// button is pressed repeatedly.
constexpr int kIgnoreRepeatedButtonUpMs = 500;

// Returns true if device is a convertible/tablet device, otherwise false.
bool IsTabletModeSupported() {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  return tablet_mode_controller && tablet_mode_controller->CanEnterTabletMode();
}

// Returns true if device is currently in tablet/tablet mode, otherwise false.
bool IsTabletModeActive() {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  return tablet_mode_controller &&
         tablet_mode_controller->IsTabletModeWindowManagerEnabled();
}

// Returns the value for the command-line switch identified by |name|. Returns 0
// if the switch was unset or contained a non-float value.
double GetNumSwitch(const base::CommandLine& command_line,
                    const std::string& name) {
  std::string str = command_line.GetSwitchValueASCII(name);
  if (str.empty())
    return 0.0;

  double value = 0.0;
  if (!base::StringToDouble(str, &value)) {
    LOG(WARNING) << "Failed to parse value \"" << str << "\" from switch "
                 << "--" << name << " as float";
    return 0.0;
  }
  return value;
}

// Computes the lid angle based on readings from accelerometers in the screen
// and keyboard panels.
float GetLidAngle(const gfx::Vector3dF& screen,
                  const gfx::Vector3dF& keyboard) {
  constexpr gfx::Vector3dF kHingeVector(1.0f, 0.0f, 0.0f);
  float lid_angle = 180.0f - gfx::ClockwiseAngleBetweenVectorsInDegrees(
                                 keyboard, screen, kHingeVector);
  if (lid_angle < 0.0f)
    lid_angle += 360.0f;

  return lid_angle;
}

}  // namespace

TabletPowerButtonController::TestApi::TestApi(
    TabletPowerButtonController* controller)
    : controller_(controller) {}

TabletPowerButtonController::TestApi::~TestApi() {}

bool TabletPowerButtonController::TestApi::ShutdownTimerIsRunning() const {
  return controller_->shutdown_timer_.IsRunning();
}

void TabletPowerButtonController::TestApi::TriggerShutdownTimeout() {
  DCHECK(ShutdownTimerIsRunning());
  controller_->OnShutdownTimeout();
  controller_->shutdown_timer_.Stop();
}

bool TabletPowerButtonController::TestApi::IsObservingAccelerometerReader(
    chromeos::AccelerometerReader* reader) const {
  DCHECK(reader);
  return controller_->accelerometer_scoped_observer_.IsObserving(reader);
}

void TabletPowerButtonController::TestApi::ParseSpuriousPowerButtonSwitches(
    const base::CommandLine& command_line) {
  controller_->ParseSpuriousPowerButtonSwitches(command_line);
}

bool TabletPowerButtonController::TestApi::IsSpuriousPowerButtonEvent() const {
  return controller_->IsSpuriousPowerButtonEvent();
}

TabletPowerButtonController::TabletPowerButtonController(
    LockStateController* controller)
    : tick_clock_(new base::DefaultTickClock()),
      controller_(controller),
      accelerometer_scoped_observer_(this),
      weak_ptr_factory_(this) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
  Shell::Get()->AddShellObserver(this);
  // TODO(mash): Provide a way for this class to observe stylus events:
  // http://crbug.com/682460
  if (ui::InputDeviceManager::HasInstance())
    ui::InputDeviceManager::GetInstance()->AddObserver(this);
  Shell::Get()->PrependPreTargetHandler(this);

  ParseSpuriousPowerButtonSwitches(*base::CommandLine::ForCurrentProcess());
  GetInitialBacklightsForcedOff();
}

TabletPowerButtonController::~TabletPowerButtonController() {
  Shell::Get()->RemovePreTargetHandler(this);
  if (ui::InputDeviceManager::HasInstance())
    ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

bool TabletPowerButtonController::ShouldHandlePowerButtonEvents() const {
  return IsTabletModeSupported();
}

void TabletPowerButtonController::OnPowerButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  if (down) {
    if ((power_button_down_was_spurious_ = IsSpuriousPowerButtonEvent())) {
      LOG(WARNING) << "Ignoring spurious power button down event";
      return;
    }

    force_off_on_button_up_ = true;
    // When the system resumes in response to the power button being pressed,
    // Chrome receives powerd's SuspendDone signal and notification that the
    // backlight has been turned back on before seeing the power button events
    // that woke the system. Avoid forcing off display just after resuming to
    // ensure that we don't turn the display off in response to the events.
    if (timestamp - last_resume_time_ <=
        base::TimeDelta::FromMilliseconds(kIgnorePowerButtonAfterResumeMs)) {
      force_off_on_button_up_ = false;
    }

    last_button_down_time_ = tick_clock_->NowTicks();
    screen_off_when_power_button_down_ = screen_state_ != ScreenState::ON;
    SetDisplayForcedOff(false);
    StartShutdownTimer();
  } else {
    // Don't process the up event if we previously ignored the down event.
    if (power_button_down_was_spurious_)
      return;

    const base::TimeTicks previous_up_time = last_button_up_time_;
    last_button_up_time_ = tick_clock_->NowTicks();

    if (max_accelerometer_samples_) {
      base::TimeDelta duration = last_button_up_time_ - last_button_down_time_;
      VLOG(1) << "Power button released after " << duration.InMilliseconds()
              << " ms";
    }

    // When power button is released, cancel shutdown animation whenever it is
    // still cancellable.
    if (controller_->CanCancelShutdownAnimation())
      controller_->CancelShutdownAnimation();

    // Ignore the event if it comes too soon after the last one.
    if (timestamp - previous_up_time <=
        base::TimeDelta::FromMilliseconds(kIgnoreRepeatedButtonUpMs)) {
      shutdown_timer_.Stop();
      return;
    }

    if (shutdown_timer_.IsRunning()) {
      shutdown_timer_.Stop();
      if (!screen_off_when_power_button_down_ && force_off_on_button_up_) {
        SetDisplayForcedOff(true);
        LockScreenIfRequired();
      }
    }
  }
}

void TabletPowerButtonController::OnAccelerometerUpdated(
    scoped_refptr<const chromeos::AccelerometerUpdate> update) {
  const gfx::Vector3dF screen = ui::ConvertAccelerometerReadingToVector3dF(
      update->get(chromeos::ACCELEROMETER_SOURCE_SCREEN));
  const gfx::Vector3dF keyboard = ui::ConvertAccelerometerReadingToVector3dF(
      update->get(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));

  DCHECK_GT(max_accelerometer_samples_, 0u);
  if (accelerometer_samples_.size() < max_accelerometer_samples_) {
    accelerometer_samples_.push_back(std::make_pair(screen, keyboard));
    last_accelerometer_sample_index_ = accelerometer_samples_.size() - 1;
  } else {
    last_accelerometer_sample_index_ =
        (last_accelerometer_sample_index_ + 1) % max_accelerometer_samples_;
    accelerometer_samples_[last_accelerometer_sample_index_] =
        std::make_pair(screen, keyboard);
  }
}

void TabletPowerButtonController::PowerManagerRestarted() {
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->SetBacklightsForcedOff(backlights_forced_off_);
}

void TabletPowerButtonController::BrightnessChanged(int level,
                                                    bool user_initiated) {
  const ScreenState old_state = screen_state_;
  if (level != 0)
    screen_state_ = ScreenState::ON;
  else
    screen_state_ = user_initiated ? ScreenState::OFF : ScreenState::OFF_AUTO;

  // Disable the touchscreen when the screen is turned off due to inactivity:
  // https://crbug.com/743291
  if ((screen_state_ == ScreenState::OFF_AUTO) !=
      (old_state == ScreenState::OFF_AUTO))
    UpdateTouchscreenStatus();
}

void TabletPowerButtonController::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  last_resume_time_ = tick_clock_->NowTicks();
  // Stop forcing backlights off on resume to handle situations where the power
  // button resumed but we didn't receive the event (crbug.com/735291).
  SetDisplayForcedOff(false);
}

void TabletPowerButtonController::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    const base::TimeTicks& timestamp) {
  SetDisplayForcedOff(false);
}

void TabletPowerButtonController::OnTabletModeStarted() {
  shutdown_timer_.Stop();
  if (controller_->CanCancelShutdownAnimation())
    controller_->CancelShutdownAnimation();
}

void TabletPowerButtonController::OnTabletModeEnded() {
  shutdown_timer_.Stop();
  if (controller_->CanCancelShutdownAnimation())
    controller_->CancelShutdownAnimation();
}

void TabletPowerButtonController::OnKeyEvent(ui::KeyEvent* event) {
  // Ignore key events generated by the power button since power button activity
  // is already handled by OnPowerButtonEvent().
  if (event->key_code() == ui::VKEY_POWER)
    return;

  if (!IsTabletModeActive() && backlights_forced_off_)
    SetDisplayForcedOff(false);
}

void TabletPowerButtonController::OnMouseEvent(ui::MouseEvent* event) {
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;

  if (!IsTabletModeActive() && backlights_forced_off_)
    SetDisplayForcedOff(false);
}

void TabletPowerButtonController::OnStylusStateChanged(ui::StylusState state) {
  if (IsTabletModeSupported() && state == ui::StylusState::REMOVED &&
      backlights_forced_off_) {
    SetDisplayForcedOff(false);
  }
}

void TabletPowerButtonController::SetTickClockForTesting(
    std::unique_ptr<base::TickClock> tick_clock) {
  DCHECK(tick_clock);
  tick_clock_ = std::move(tick_clock);
}

void TabletPowerButtonController::ParseSpuriousPowerButtonSwitches(
    const base::CommandLine& command_line) {
  // Support being called multiple times from tests.
  max_accelerometer_samples_ = 0;
  accelerometer_samples_.clear();
  accelerometer_scoped_observer_.RemoveAll();

  int window = static_cast<int>(
      GetNumSwitch(command_line, switches::kSpuriousPowerButtonWindow));
  if (window <= 0)
    return;

  max_accelerometer_samples_ = static_cast<size_t>(window);
  accelerometer_samples_.reserve(max_accelerometer_samples_);
  accelerometer_scoped_observer_.Add(
      chromeos::AccelerometerReader::GetInstance());

  spurious_accel_count_ = static_cast<size_t>(
      GetNumSwitch(command_line, switches::kSpuriousPowerButtonAccelCount));
  spurious_screen_accel_ =
      GetNumSwitch(command_line, switches::kSpuriousPowerButtonScreenAccel);
  spurious_keyboard_accel_ =
      GetNumSwitch(command_line, switches::kSpuriousPowerButtonKeyboardAccel);
  spurious_lid_angle_change_ =
      GetNumSwitch(command_line, switches::kSpuriousPowerButtonLidAngleChange);
}

bool TabletPowerButtonController::IsSpuriousPowerButtonEvent() const {
  if (max_accelerometer_samples_ <= 0)
    return false;

  // Number of screen and keyboard accelerations exceeding the threshold.
  size_t num_big_screen_accels = 0;
  size_t num_big_keyboard_accels = 0;

  // The last lid angle that we saw.
  float last_angle = 0.0f;
  // The current distance (in degrees) that we've traveled from the starting
  // angle and the max distance that we've seen so far.
  float cur_angle_dist = 0.0f, max_angle_dist = 0.0f;

  std::string screen_debug, keyboard_debug, angle_debug;

  // Get the index of the oldest pair of samples.
  const size_t start =
      accelerometer_samples_.size() == max_accelerometer_samples_
          ? (last_accelerometer_sample_index_ + 1) % max_accelerometer_samples_
          : 0;
  for (size_t i = 0; i < accelerometer_samples_.size(); ++i) {
    const auto& pair =
        accelerometer_samples_[(start + i) % max_accelerometer_samples_];
    const gfx::Vector3dF& screen = pair.first;
    const gfx::Vector3dF& keyboard = pair.second;

    const float screen_accel = std::abs(screen.Length() - kGravity);
    if (spurious_screen_accel_ > 0 && screen_accel >= spurious_screen_accel_)
      num_big_screen_accels++;

    const float keyboard_accel = std::abs(keyboard.Length() - kGravity);
    if (spurious_keyboard_accel_ > 0 &&
        keyboard_accel >= spurious_keyboard_accel_) {
      num_big_keyboard_accels++;
    }

    float angle = GetLidAngle(screen, keyboard);
    if (i > 0u) {
      // Lid angle readings are computed based on the screen and keyboard
      // acceleration vectors and can be noisy. Compute the minimum angle
      // difference between the previous reading and the current one and use it
      // to maintain a running total of how far the lid has traveled, also
      // keeping track of the max distance from the start that we've seen.
      float min_diff = angle - last_angle;
      if (min_diff < -180.0f)
        min_diff += 360.0f;
      else if (min_diff > 180.0f)
        min_diff -= 360.0f;

      cur_angle_dist += min_diff;
      max_angle_dist =
          std::min(std::max(max_angle_dist, std::abs(cur_angle_dist)), 360.0f);
    }
    last_angle = angle;

    if (VLOG_IS_ON(1)) {
      screen_debug = base::StringPrintf("%0.1f", screen_accel) +
                     (screen_debug.empty() ? "" : " " + screen_debug);
      keyboard_debug = base::StringPrintf("%0.1f", keyboard_accel) +
                       (keyboard_debug.empty() ? "" : " " + keyboard_debug);
      angle_debug = base::IntToString(static_cast<int>(angle + 0.5)) +
                    (angle_debug.empty() ? "" : " " + angle_debug);
    }
  }

  VLOG(1) << "Screen accelerations (" << num_big_screen_accels << " big): "
          << "[" << screen_debug << "]";
  VLOG(1) << "Keyboard accelerations (" << num_big_keyboard_accels << " big): "
          << "[" << keyboard_debug << "]";
  VLOG(1) << "Lid angles (" << base::StringPrintf("%0.1f", max_angle_dist)
          << " degrees): [" << angle_debug << "]";

  return (spurious_screen_accel_ > 0 &&
          num_big_screen_accels >= spurious_accel_count_) ||
         (spurious_keyboard_accel_ > 0 &&
          num_big_keyboard_accels >= spurious_accel_count_) ||
         (spurious_lid_angle_change_ > 0 &&
          max_angle_dist >= spurious_lid_angle_change_);
}

void TabletPowerButtonController::SetDisplayForcedOff(bool forced_off) {
  if (backlights_forced_off_ == forced_off)
    return;

  // Set the display and keyboard backlights (if present) to |forced_off|.
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->SetBacklightsForcedOff(forced_off);
  backlights_forced_off_ = forced_off;
  UpdateTouchscreenStatus();

  if (backlights_forced_off_)
    Shell::Get()->shell_delegate()->SuspendMediaSessions();

  // Send an a11y alert.
  Shell::Get()->accessibility_delegate()->TriggerAccessibilityAlert(
      forced_off ? A11Y_ALERT_SCREEN_OFF : A11Y_ALERT_SCREEN_ON);
}

void TabletPowerButtonController::GetInitialBacklightsForcedOff() {
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->GetBacklightsForcedOff(base::Bind(
          &TabletPowerButtonController::OnGotInitialBacklightsForcedOff,
          weak_ptr_factory_.GetWeakPtr()));
}

void TabletPowerButtonController::OnGotInitialBacklightsForcedOff(
    bool is_forced_off) {
  backlights_forced_off_ = is_forced_off;
  UpdateTouchscreenStatus();
}

void TabletPowerButtonController::UpdateTouchscreenStatus() {
  const bool enable_touchscreen =
      !backlights_forced_off_ && (screen_state_ != ScreenState::OFF_AUTO);
  ShellDelegate* delegate = Shell::Get()->shell_delegate();
  delegate->SetTouchscreenEnabledInPrefs(enable_touchscreen,
                                         true /* use_local_state */);
  delegate->UpdateTouchscreenStatusFromPrefs();
}

void TabletPowerButtonController::StartShutdownTimer() {
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
      screen_off_when_power_button_down_ ? kShutdownWhenScreenOffTimeoutMs
                                         : kShutdownWhenScreenOnTimeoutMs);
  shutdown_timer_.Start(FROM_HERE, timeout, this,
                        &TabletPowerButtonController::OnShutdownTimeout);
}

void TabletPowerButtonController::OnShutdownTimeout() {
  controller_->StartShutdownAnimation(ShutdownReason::POWER_BUTTON);
}

void TabletPowerButtonController::LockScreenIfRequired() {
  SessionController* session_controller = Shell::Get()->session_controller();
  if (session_controller->ShouldLockScreenAutomatically() &&
      session_controller->CanLockScreen() &&
      !session_controller->IsUserSessionBlocked() &&
      !controller_->LockRequested()) {
    session_controller->LockScreen();
  }
}

}  // namespace ash
