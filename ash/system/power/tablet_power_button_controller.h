// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_H_
#define ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_H_

#include <memory>
#include <utility>

#include "ash/ash_export.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace base {
class CommandLine;
class TickClock;
}  // namespace base

namespace ash {

class LockStateController;
class PowerButtonDisplayController;

// Handles power button events on convertible/tablet device. This class is
// instantiated and used in PowerButtonController.
class ASH_EXPORT TabletPowerButtonController
    : public chromeos::AccelerometerReader::Observer,
      public chromeos::PowerManagerClient::Observer,
      public TabletModeObserver {
 public:
  // Public for tests.
  static constexpr float kGravity = 9.80665f;

  // Amount of time since last screen state change that power button event needs
  // to be ignored.
  static constexpr base::TimeDelta kScreenStateChangeDelay =
      base::TimeDelta::FromMilliseconds(500);

  // Ignore button-up events occurring within this many milliseconds of the
  // previous button-up event. This prevents us from falling behind if the power
  // button is pressed repeatedly.
  static constexpr base::TimeDelta kIgnoreRepeatedButtonUpDelay =
      base::TimeDelta::FromMilliseconds(500);

  TabletPowerButtonController(PowerButtonDisplayController* display_controller,
                              base::TickClock* tick_clock);
  ~TabletPowerButtonController() override;

  // Returns true if power button events should be handled by this class instead
  // of PowerButtonController.
  bool ShouldHandlePowerButtonEvents() const;

  // Handles a power button event.
  void OnPowerButtonEvent(bool down, const base::TimeTicks& timestamp);

  // Overridden from chromeos::AccelerometerReader::Observer:
  void OnAccelerometerUpdated(
      scoped_refptr<const chromeos::AccelerometerUpdate> update) override;

  // Overridden from chromeos::PowerManagerClient::Observer:
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // Cancel the ongoing tablet power button behavior.
  void CancelTabletPowerButton();

 private:
  friend class TabletPowerButtonControllerTestApi;

  // Parses command-line switches that provide settings used to attempt to
  // ignore accidental power button presses by looking at accelerometer data.
  void ParseSpuriousPowerButtonSwitches(const base::CommandLine& command_line);

  // Returns true if the device's accelerometers have reported enough recent
  // movement that we should consider a power button event that was just
  // received to be accidental and ignore it.
  bool IsSpuriousPowerButtonEvent() const;

  // Starts |shutdown_timer_| when the power button is pressed while in
  // tablet mode.
  void StartShutdownTimer();

  // Called by |shutdown_timer_| to start the pre-shutdown animation.
  void OnShutdownTimeout();

  // Locks the screen if the "require password to wake from sleep" pref is set
  // and locking is possible.
  void LockScreenIfRequired();

  // True if the screen was off when the power button was pressed.
  bool screen_off_when_power_button_down_ = false;

  // True if the last power button down event was deemed spurious and ignored as
  // a result.
  bool power_button_down_was_spurious_ = false;

  // Saves the most recent timestamp that powerd is resuming from suspend,
  // updated in SuspendDone().
  base::TimeTicks last_resume_time_;

  // Saves the most recent timestamp that power button is pressed and released.
  base::TimeTicks last_button_down_time_;
  base::TimeTicks last_button_up_time_;

  // True if power button released should force off display.
  bool force_off_on_button_up_ = true;

  // Started when the tablet power button is pressed and stopped when it's
  // released. Runs OnShutdownTimeout() to start shutdown.
  base::OneShotTimer shutdown_timer_;

  LockStateController* lock_state_controller_;  // Not owned.

  // Used to interact with the display.
  PowerButtonDisplayController* display_controller_;  // Not owned.

  // Time source for performed action times.
  base::TickClock* tick_clock_;  // Not owned.

  ScopedObserver<chromeos::AccelerometerReader, TabletPowerButtonController>
      accelerometer_scoped_observer_;

  // Number of recent screen and keyboard accelerometer samples to retain.
  size_t max_accelerometer_samples_ = 0;

  // Circular buffer of recent (screen, keyboard) accelerometer samples.
  std::vector<std::pair<gfx::Vector3dF, gfx::Vector3dF>> accelerometer_samples_;
  size_t last_accelerometer_sample_index_ = 0;

  // Number of acceleration readings in |accelerometer_samples_| that must
  // exceed the threshold in order for a power button event to be considered
  // spurious.
  size_t spurious_accel_count_ = 0;

  // Thresholds for screen and keyboard accelerations (excluding gravity). See
  // |spurious_accel_count_|.
  float spurious_screen_accel_ = 0;
  float spurious_keyboard_accel_ = 0;

  // Threshold for the lid angle change seen within |accelerometer_samples_|
  // in order for a power button event to be considered spurious.
  float spurious_lid_angle_change_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TabletPowerButtonController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_H_
