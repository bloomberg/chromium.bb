// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_POWER_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_POWER_MANAGER_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace power_manager {
class PowerManagementPolicy;
class PowerSupplyProperties;
}

namespace chromeos {

// Callback used for getting the current screen brightness.  The param is in the
// range [0.0, 100.0].
typedef base::Callback<void(double)> GetScreenBrightnessPercentCallback;

// PowerManagerClient is used to communicate with the power manager.
class CHROMEOS_EXPORT PowerManagerClient : public DBusClient {
 public:
  // Interface for observing changes from the power manager.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called if the power manager process restarts.
    virtual void PowerManagerRestarted() {}

    // Called when the brightness is changed.
    // |level| is of the range [0, 100].
    // |user_initiated| is true if the action is initiated by the user.
    virtual void BrightnessChanged(int level, bool user_initiated) {}

    // Called when peripheral device battery status is received.
    // |path| is the sysfs path for the battery of the peripheral device.
    // |name| is the human readble name of the device.
    // |level| within [0, 100] represents the device battery level and -1
    // means an unknown level or device is disconnected.
    virtual void PeripheralBatteryStatusReceived(const std::string& path,
                                                 const std::string& name,
                                                 int level) {}

    // Called when updated information about the power supply is available.
    // The status is automatically updated periodically, but
    // RequestStatusUpdate() can be used to trigger an immediate update.
    virtual void PowerChanged(
        const power_manager::PowerSupplyProperties& proto) {}

    // Called when the system is about to suspend. Suspend is deferred until
    // all observers' implementations of this method have finished running.
    //
    // If an observer wishes to asynchronously delay suspend,
    // PowerManagerClient::GetSuspendReadinessCallback() may be called from
    // within SuspendImminent().  The returned callback must be called once
    // the observer is ready for suspend.
    virtual void SuspendImminent() {}

    // Called when a suspend attempt (previously announced via
    // SuspendImminent()) has completed. The system may not have actually
    // suspended (if e.g. the user canceled the suspend attempt).
    virtual void SuspendDone(const base::TimeDelta& sleep_duration) {}

    // Called when the system is about to resuspend from a dark resume.  Like
    // SuspendImminent(), the suspend will be deferred until all observers have
    // finished running and those observers that wish to asynchronously delay
    // the suspend should call PowerManagerClient::GetSuspendReadinessCallback()
    // from within this method.  The returned callback should be run once the
    // observer is ready for suspend.
    virtual void DarkSuspendImminent() {}

    // Called when the power button is pressed or released.
    virtual void PowerButtonEventReceived(bool down,
                                          const base::TimeTicks& timestamp) {}

    // Called when the device's lid is opened or closed.
    virtual void LidEventReceived(bool open,
                                  const base::TimeTicks& timestamp) {}

    // Called when the idle action will be performed after
    // |time_until_idle_action|.
    virtual void IdleActionImminent(
        const base::TimeDelta& time_until_idle_action) {}

    // Called after IdleActionImminent() when the inactivity timer is reset
    // before the idle action has been performed.
    virtual void IdleActionDeferred() {}
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

  // Requests an updated copy of the power status. Observer::PowerChanged()
  // will be called asynchronously.
  virtual void RequestStatusUpdate() = 0;

  // Requests restart of the system.
  virtual void RequestRestart() = 0;

  // Requests shutdown of the system.
  virtual void RequestShutdown() = 0;

  // Notifies the power manager that the user is active (i.e. generating input
  // events).
  virtual void NotifyUserActivity(power_manager::UserActivityType type) = 0;

  // Notifies the power manager that a video is currently playing. It also
  // includes whether or not the containing window for the video is fullscreen.
  virtual void NotifyVideoActivity(bool is_fullscreen) = 0;

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

  // Returns the number of callbacks returned by GetSuspendReadinessCallback()
  // for the current suspend attempt but not yet called. Used by tests.
  virtual int GetNumPendingSuspendReadinessCallbacks() = 0;

  // Creates the instance.
  static PowerManagerClient* Create(DBusClientImplementationType type);

  virtual ~PowerManagerClient();

 protected:
  // Create() should be used instead.
  PowerManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_POWER_MANAGER_CLIENT_H_
