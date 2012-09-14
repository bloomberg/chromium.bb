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

namespace base {
class TimeTicks;
}
namespace dbus {
class Bus;
}

namespace chromeos {

// Callback used for processing the idle time.  The int64 param is the number of
// seconds the user has been idle.
typedef base::Callback<void(int64)> CalculateIdleTimeCallback;
typedef base::Callback<void(void)> IdleNotificationCallback;
typedef base::Callback<void(uint32)> PowerStateRequestIdCallback;

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

    // Called when the brightness is changed.
    // |level| is of the range [0, 100].
    // |user_initiated| is true if the action is initiated by the user.
    virtual void BrightnessChanged(int level, bool user_initiated) {}

    // Called when a screen is turned on or off to request that Chrome enable or
    // disable the corresponding CRTC for the output.
    // |power_on| The new state of the power setting.
    // |all_displays| True if this applies to all displays or false if it is
    // the internal display only.
    virtual void ScreenPowerSet(bool power_on, bool all_displays) {}

    // Called when power supply polling takes place.  |status| is a data
    // structure that contains the current state of the power supply.
    virtual void PowerChanged(const PowerSupplyStatus& status) {}

    // Called when the system resumes from suspend.
    virtual void SystemResumed() {}

    // Called when the power button is pressed or released.
    virtual void PowerButtonStateChanged(bool down,
                                         const base::TimeTicks& timestamp) {}

    // Called when the lock button is pressed or released.
    virtual void LockButtonStateChanged(bool down,
                                        const base::TimeTicks& timestamp) {}

    // Called when we go idle for threshold time.
    virtual void IdleNotify(int64 threshold_secs) {}

    // Called when a request is received to dim or undim the screen in software
    // (as opposed to the more-common method of adjusting the backlight).
    virtual void ScreenDimmingRequested(ScreenDimmingState state) {}
  };

  enum UpdateRequestType {
    UPDATE_INITIAL,  // Initial update request.
    UPDATE_USER,     // User initialted update request.
    UPDATE_POLL      // Update requested by poll signal.
  };

  enum PowerStateOverrideType {
    DISABLE_IDLE_DIM = 1,  // Disable screen dimming on idle.
    DISABLE_IDLE_BLANK = 2,  // Disable screen blanking on idle.
    DISABLE_IDLE_SUSPEND = 3,  // Disable suspend on idle.
    DISABLE_IDLE_LID_SUSPEND = 4,  // Disable suspend on lid closed.
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

  // Calculates idle time asynchronously, after the idle time request has
  // replied.  It passes the idle time in seconds to |callback|.  If it
  // encounters some error, it passes -1 to |callback|.
  virtual void CalculateIdleTime(const CalculateIdleTimeCallback& callback) = 0;

  // Requests notification for Idle at a certain threshold.
  // NOTE: This notification is one shot, once the machine has been idle for
  // threshold time, a notification will be sent and then that request will be
  // removed from the notification queue. If you wish notifications the next
  // time the machine goes idle for that much time, request again.
  virtual void RequestIdleNotification(int64 threshold_secs) = 0;

  // Notifies the power manager that the user is active (i.e. generating input
  // events).
  virtual void NotifyUserActivity(
      const base::TimeTicks& last_activity_time) = 0;

  // Notifies the power manager that a video is currently playing. It also
  // includes whether or not the containing window for the video is fullscreen.
  virtual void NotifyVideoActivity(
      const base::TimeTicks& last_activity_time,
      bool is_fullscreen) = 0;

  // Override the current power state on the machine. The overrides will be
  // applied to the request ID specified. To specify a new request; use 0 as
  // the request id and the method will call the provided callback with the
  // new request ID for use with further calls.
  // The overrides parameter will & out the PowerStateOverrideType types to
  // allow specific selection of overrides. For example, to override just dim
  // and suspending but leaving blanking in, set overrides to,
  // DISABLE_IDLE_DIM | DISABLE_IDLE_SUSPEND.
  virtual void RequestPowerStateOverrides(
      uint32 request_id,
      uint32 duration,
      int overrides,
      const PowerStateRequestIdCallback& callback) = 0;

  // Cancels the power state override request specified by request_id.
  virtual void CancelPowerStateOverrides(uint32 request_id) = 0;

  // Tells powerd whether or not we are in a projecting mode.  This is used to
  // adjust idleness thresholds and derived, on this side, from the number of
  // video outputs attached.
  virtual void SetIsProjecting(bool is_projecting) = 0;

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
