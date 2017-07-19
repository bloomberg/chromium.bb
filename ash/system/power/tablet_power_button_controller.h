// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_H_
#define ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_H_

#include <memory>
#include <utility>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/time/tick_clock.h"
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
}  // namespace base

namespace ash {

class LockStateController;

// Handles power button events on convertible/tablet device. This class is
// instantiated and used in PowerButtonController.
class ASH_EXPORT TabletPowerButtonController
    : public chromeos::AccelerometerReader::Observer,
      public chromeos::PowerManagerClient::Observer,
      public ShellObserver,
      public ui::EventHandler,
      public ui::InputDeviceEventObserver {
 public:
  // Helper class used by tablet power button tests to access internal state.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(TabletPowerButtonController* controller);
    ~TestApi();

    // Returns true when |shutdown_timer_| is running.
    bool ShutdownTimerIsRunning() const;

    // Emulates |shutdown_timer_| timeout.
    void TriggerShutdownTimeout();

    // Returns true if |controller_| is observing |reader|.
    bool IsObservingAccelerometerReader(
        chromeos::AccelerometerReader* reader) const;

    // Calls |controller_|'s ParseSpuriousPowerButtonSwitches() method.
    void ParseSpuriousPowerButtonSwitches(
        const base::CommandLine& command_line);

    // Calls |controller_|'s IsSpuriousPowerButtonEvent() method.
    bool IsSpuriousPowerButtonEvent() const;

   private:
    TabletPowerButtonController* controller_;  // Not owned.

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Public for tests.
  static constexpr float kGravity = 9.80665f;

  explicit TabletPowerButtonController(LockStateController* controller);
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
  void PowerManagerRestarted() override;
  void BrightnessChanged(int level, bool user_initiated) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        const base::TimeTicks& timestamp) override;

  // Overridden from ShellObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Overridden from ui::InputDeviceObserver:
  void OnStylusStateChanged(ui::StylusState state) override;

  // Overrides the tick clock used by |this| for testing.
  void SetTickClockForTesting(std::unique_ptr<base::TickClock> tick_clock);

 private:
  // Parses command-line switches that provide settings used to attempt to
  // ignore accidental power button presses by looking at accelerometer data.
  void ParseSpuriousPowerButtonSwitches(const base::CommandLine& command_line);

  // Returns true if the device's accelerometers have reported enough recent
  // movement that we should consider a power button event that was just
  // received to be accidental and ignore it.
  bool IsSpuriousPowerButtonEvent() const;

  // Updates the power manager's backlights-forced-off state and enables or
  // disables the touchscreen. No-op if |backlights_forced_off_| already equals
  // |forced_off|.
  void SetDisplayForcedOff(bool forced_off);

  // Sends a request to powerd to get the backlights forced off state so that
  // |backlights_forced_off_| can be initialized.
  void GetInitialBacklightsForcedOff();

  // Initializes |backlights_forced_off_|.
  void OnGotInitialBacklightsForcedOff(bool is_forced_off);

  // Enables or disables the touchscreen, also writing its state to a pref in
  // local state. The touchscreen is disabled when backlights are forced off.
  void UpdateTouchscreenStatus();

  // Starts |shutdown_timer_| when the power button is pressed while in
  // tablet mode.
  void StartShutdownTimer();

  // Called by |shutdown_timer_| to start the pre-shutdown animation.
  void OnShutdownTimeout();

  // Locks the screen if the "require password to wake from sleep" pref is set
  // and locking is possible.
  void LockScreenIfRequired();

  // Screen state as communicated by D-Bus signals from powerd about backlight
  // brightness changes.
  enum class ScreenState {
    // The screen is on.
    ON,
    // The screen is off.
    OFF,
    // The screen is off, specifically due to an automated change like user
    // inactivity.
    OFF_AUTO,
  };

  // Current screen state.
  ScreenState screen_state_ = ScreenState::ON;

  // Current forced-off state of backlights.
  bool backlights_forced_off_ = false;

  // True if the screen was off when the power button was pressed.
  bool screen_off_when_power_button_down_ = false;

  // True if the last power button down event was deemed spurious and ignored as
  // a result.
  bool power_button_down_was_spurious_ = false;

  // Time source for performed action times.
  std::unique_ptr<base::TickClock> tick_clock_;

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

  LockStateController* controller_;  // Not owned.

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

  base::WeakPtrFactory<TabletPowerButtonController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabletPowerButtonController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_TABLET_POWER_BUTTON_CONTROLLER_H_
