// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_POWER_TABLET_POWER_BUTTON_CONTROLLER_H_
#define ASH_SYSTEM_CHROMEOS_POWER_TABLET_POWER_BUTTON_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/common/shell_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/event_handler.h"

namespace ash {

class LockStateController;

// Handles power button events on convertible/tablet device. This class is
// instantiated and used in PowerButtonController.
class ASH_EXPORT TabletPowerButtonController
    : public chromeos::PowerManagerClient::Observer,
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

   private:
    TabletPowerButtonController* controller_;  // Not owned.

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  explicit TabletPowerButtonController(LockStateController* controller);
  ~TabletPowerButtonController() override;

  // Returns true if power button events should be handled by this class instead
  // of PowerButtonController.
  bool ShouldHandlePowerButtonEvents() const;

  // Handles a power button event.
  void OnPowerButtonEvent(bool down, const base::TimeTicks& timestamp);

  // Overridden from chromeos::PowerManagerClient::Observer:
  void PowerManagerRestarted() override;
  void BrightnessChanged(int level, bool user_initiated) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // Overridden from ShellObserver:
  void OnMaximizeModeStarted() override;
  void OnMaximizeModeEnded() override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Overridden from ui::InputDeviceObserver:
  void OnStylusStateChanged(ui::StylusState state) override;

  // Overrides the tick clock used by |this| for testing.
  void SetTickClockForTesting(std::unique_ptr<base::TickClock> tick_clock);

 private:
  // Updates the power manager's backlights-forced-off state and enables or
  // disables the touchscreen. No-op if |backlights_forced_off_| already equals
  // |forced_off|.
  void SetDisplayForcedOff(bool forced_off);

  // Sends a request to powerd to get the backlights forced off state so that
  // |backlights_forced_off_| can be initialized.
  void GetInitialBacklightsForcedOff();

  // Initializes |backlights_forced_off_|.
  void OnGotInitialBacklightsForcedOff(bool is_forced_off);

  // Starts |shutdown_timer_| when the power button is pressed while in
  // tablet mode.
  void StartShutdownTimer();

  // Called by |shutdown_timer_| to start the pre-shutdown animation.
  void OnShutdownTimeout();

  // Locks the screen if the "require password to wake from sleep" pref is set
  // and locking is possible.
  void LockScreenIfRequired();

  // True if the brightness level is currently set to off.
  bool brightness_level_is_zero_ = false;

  // Current forced-off state of backlights.
  bool backlights_forced_off_ = false;

  // True if the screen was off when the power button was pressed.
  bool screen_off_when_power_button_down_ = false;

  // Time source for performed action times.
  std::unique_ptr<base::TickClock> tick_clock_;

  // Saves the most recent timestamp that powerd is resuming from suspend,
  // updated in SuspendDone().
  base::TimeTicks last_resume_time_;

  // Saves the most recent timestamp that power button is released.
  base::TimeTicks last_button_up_time_;

  // True if power button released should force off display.
  bool force_off_on_button_up_;

  // Started when the tablet power button is pressed and stopped when it's
  // released. Runs OnShutdownTimeout() to start shutdown.
  base::OneShotTimer shutdown_timer_;

  LockStateController* controller_;  // Not owned.

  base::WeakPtrFactory<TabletPowerButtonController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabletPowerButtonController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_POWER_TABLET_POWER_BUTTON_CONTROLLER_H_
