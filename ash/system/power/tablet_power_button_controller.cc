// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/tablet_power_button_controller.h"

#include "ash/accessibility_delegate.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shutdown_reason.h"
#include "ash/system/power/power_button_display_controller.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/tick_clock.h"
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
constexpr base::TimeDelta kShutdownWhenScreenOnTimeout =
    base::TimeDelta::FromMilliseconds(500);
// TODO(derat): This is currently set to a high value to work around delays in
// powerd's reports of button-up events when the preceding button-down event
// turns the display on. Set it to a lower value once powerd no longer blocks on
// asking Chrome to turn the display on: http://crbug.com/685734
constexpr base::TimeDelta kShutdownWhenScreenOffTimeout =
    base::TimeDelta::FromMilliseconds(2000);

// Amount of time since last SuspendDone() that power button event needs to be
// ignored.
constexpr base::TimeDelta kIgnorePowerButtonAfterResumeDelay =
    base::TimeDelta::FromSeconds(2);

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

constexpr base::TimeDelta TabletPowerButtonController::kScreenStateChangeDelay;

constexpr base::TimeDelta
    TabletPowerButtonController::kIgnoreRepeatedButtonUpDelay;

TabletPowerButtonController::TestApi::TestApi(
    TabletPowerButtonController* controller)
    : controller_(controller) {}

TabletPowerButtonController::TestApi::~TestApi() = default;

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

void TabletPowerButtonController::TestApi::SendKeyEvent(ui::KeyEvent* event) {
  controller_->display_controller_->OnKeyEvent(event);
}

TabletPowerButtonController::TabletPowerButtonController(
    PowerButtonDisplayController* display_controller,
    base::TickClock* tick_clock)
    : lock_state_controller_(Shell::Get()->lock_state_controller()),
      display_controller_(display_controller),
      tick_clock_(tick_clock),
      accelerometer_scoped_observer_(this) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);

  ParseSpuriousPowerButtonSwitches(*base::CommandLine::ForCurrentProcess());
}

TabletPowerButtonController::~TabletPowerButtonController() {
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
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
    if (timestamp - last_resume_time_ <= kIgnorePowerButtonAfterResumeDelay)
      force_off_on_button_up_ = false;

    // The actual display may remain off for a short period after powerd asks
    // Chrome to turn it on. If the user presses the power button again during
    // this time, they probably intend to turn the display on. Avoid forcing off
    // in this case.
    if (timestamp - display_controller_->screen_state_last_changed() <=
        kScreenStateChangeDelay) {
      force_off_on_button_up_ = false;
    }

    last_button_down_time_ = timestamp;
    screen_off_when_power_button_down_ =
        display_controller_->screen_state() !=
        PowerButtonDisplayController::ScreenState::ON;
    display_controller_->SetDisplayForcedOff(false);
    StartShutdownTimer();
  } else {
    // Don't process the up event if we previously ignored the down event.
    if (power_button_down_was_spurious_)
      return;

    const base::TimeTicks previous_up_time = last_button_up_time_;
    last_button_up_time_ = timestamp;

    if (max_accelerometer_samples_) {
      base::TimeDelta duration = last_button_up_time_ - last_button_down_time_;
      VLOG(1) << "Power button released after " << duration.InMilliseconds()
              << " ms";
    }

    // When power button is released, cancel shutdown animation whenever it is
    // still cancellable.
    if (lock_state_controller_->CanCancelShutdownAnimation())
      lock_state_controller_->CancelShutdownAnimation();

    // Ignore the event if it comes too soon after the last one.
    if (timestamp - previous_up_time <= kIgnoreRepeatedButtonUpDelay) {
      shutdown_timer_.Stop();
      return;
    }

    if (shutdown_timer_.IsRunning()) {
      shutdown_timer_.Stop();
      if (!screen_off_when_power_button_down_ && force_off_on_button_up_) {
        display_controller_->SetDisplayForcedOff(true);
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

void TabletPowerButtonController::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  last_resume_time_ = tick_clock_->NowTicks();
}

void TabletPowerButtonController::OnTabletModeStarted() {
  shutdown_timer_.Stop();
  if (lock_state_controller_->CanCancelShutdownAnimation())
    lock_state_controller_->CancelShutdownAnimation();
}

void TabletPowerButtonController::OnTabletModeEnded() {
  shutdown_timer_.Stop();
  if (lock_state_controller_->CanCancelShutdownAnimation())
    lock_state_controller_->CancelShutdownAnimation();
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

void TabletPowerButtonController::StartShutdownTimer() {
  base::TimeDelta timeout = screen_off_when_power_button_down_
                                ? kShutdownWhenScreenOffTimeout
                                : kShutdownWhenScreenOnTimeout;
  shutdown_timer_.Start(FROM_HERE, timeout, this,
                        &TabletPowerButtonController::OnShutdownTimeout);
}

void TabletPowerButtonController::OnShutdownTimeout() {
  lock_state_controller_->StartShutdownAnimation(ShutdownReason::POWER_BUTTON);
}

void TabletPowerButtonController::LockScreenIfRequired() {
  SessionController* session_controller = Shell::Get()->session_controller();
  if (session_controller->ShouldLockScreenAutomatically() &&
      session_controller->CanLockScreen() &&
      !session_controller->IsUserSessionBlocked() &&
      !lock_state_controller_->LockRequested()) {
    lock_state_controller_->LockWithoutAnimation();
  }
}

}  // namespace ash
