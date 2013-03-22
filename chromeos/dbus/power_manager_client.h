// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_POWER_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_POWER_MANAGER_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/time.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"

#include "chromeos/dbus/power_supply_status.h"

namespace dbus {
class Bus;
}

namespace power_manager {
class PowerManagementPolicy;
}

namespace chromeos {

typedef base::Callback<void(void)> IdleNotificationCallback;

// Callback used for getting the current screen brightness.  The param is in the
// range [0.0, 100.0].
typedef base::Callback<void(double)> GetScreenBrightnessPercentCallback;

// PowerManagerClient is used to communicate with the power manager.
class CHROMEOS_EXPORT PowerManagerClient {
 public:
  // Interface for observing changes from the power manager.
  class Observer {
   public:
    enum ScreenDimmingState {
      SCREEN_DIMMING_NONE = 0,
      SCREEN_DIMMING_IDLE,
    };

    virtual ~Observer() {}

    // Called if the power manager process restarts.
    virtual void PowerManagerRestarted() {}

    // Called when the brightness is changed.
    // |level| is of the range [0, 100].
    // |user_initiated| is true if the action is initiated by the user.
    virtual void BrightnessChanged(int level, bool user_initiated) {}

    // Called when power supply polling takes place.  |status| is a data
    // structure that contains the current state of the power supply.
    virtual void PowerChanged(const PowerSupplyStatus& status) {}

    // Called when we go idle for threshold time.
    virtual void IdleNotify(int64 threshold_secs) {}

    // Called when a request is received to dim or undim the screen in software
    // (as opposed to the more-common method of adjusting the backlight).
    virtual void ScreenDimmingRequested(ScreenDimmingState state) {}

    // Called when the system is about to suspend. Suspend is deferred until
    // all observers' implementations of this method have finished running.
    //
    // If an observer wishes to asynchronously delay suspend,
    // PowerManagerClient::GetSuspendReadinessCallback() may be called from
    // within SuspendImminent().  The returned callback must be called once
    // the observer is ready for suspend.
    virtual void SuspendImminent() {}

    // Called when the power button is pressed or released.
    virtual void PowerButtonEventReceived(bool down,
                                          const base::TimeTicks& timestamp) {}

    // Called when the device's lid is opened or closed.
    virtual void LidEventReceived(bool open,
                                  const base::TimeTicks& timestamp) {}

    // Called when the system resumes from sleep.
    virtual void SystemResumed(const base::TimeDelta& sleep_duration) {}

    // Called when the idle action will be performed soon.
    virtual void IdleActionImminent() {}

    // Called after IdleActionImminent() when the inactivity timer is reset
    // before the idle action has been performed.
    virtual void IdleActionDeferred() {}
  };

  enum UpdateRequestType {
    UPDATE_INITIAL,  // Initial update request.
    UPDATE_USER,     // User initialted update request.
    UPDATE_POLL      // Update requested by poll signal.
  };

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(Observer* observer) = 0;

  // Decreases the screen brightness. |allow_off| controls whether or not
  // it's allowed to turn off the back light.
  virtual void DecreaseScreenBrightness(bool allow_off) = 0;

  // Increases the screen brightness.
  virtual void IncreaseScreenBrightness() = 0;

  // Set the screen brightness to |percent|, in the range [0.0, 100.0].
  // If |gradual| is true, the transition will be animated.
  virtual void SetScreenBrightnessPercent(double percent, bool gradual) = 0;

  // Asynchronously gets the current screen brightness, in the range
  // [0.0, 100.0].
  virtual void GetScreenBrightnessPercent(
      const GetScreenBrightnessPercentCallback& callback) = 0;

  // Decreases the keyboard brightness.
  virtual void DecreaseKeyboardBrightness() = 0;

  // Increases the keyboard brightness.
  virtual void IncreaseKeyboardBrightness() = 0;

  // Request for power supply status update.
  virtual void RequestStatusUpdate(UpdateRequestType update_type) = 0;

  // Requests restart of the system.
  virtual void RequestRestart() = 0;

  // Requests shutdown of the system.
  virtual void RequestShutdown() = 0;

  // Idle management functions:

  // Requests notification for Idle at a certain threshold.
  // NOTE: This notification is one shot, once the machine has been idle for
  // threshold time, a notification will be sent and then that request will be
  // removed from the notification queue. If you wish notifications the next
  // time the machine goes idle for that much time, request again.
  virtual void RequestIdleNotification(int64 threshold_secs) = 0;

  // Notifies the power manager that the user is active (i.e. generating input
  // events).
  virtual void NotifyUserActivity() = 0;

  // Notifies the power manager that a video is currently playing. It also
  // includes whether or not the containing window for the video is fullscreen.
  virtual void NotifyVideoActivity(
      const base::TimeTicks& last_activity_time,
      bool is_fullscreen) = 0;

  // Tells the power manager to begin using |policy|.
  virtual void SetPolicy(
      const power_manager::PowerManagementPolicy& policy) = 0;

  // Tells powerd whether or not we are in a projecting mode.  This is used to
  // adjust idleness thresholds and derived, on this side, from the number of
  // video outputs attached.
  virtual void SetIsProjecting(bool is_projecting) = 0;

  // Returns a callback that can be called by an observer to report
  // readiness for suspend.  See Observer::SuspendImminent().
  virtual base::Closure GetSuspendReadinessCallback() = 0;

  // Creates the instance.
  static PowerManagerClient* Create(DBusClientImplementationType type,
                                    dbus::Bus* bus);

  virtual ~PowerManagerClient();

 protected:
  // Create() should be used instead.
  PowerManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_POWER_MANAGER_CLIENT_H_
