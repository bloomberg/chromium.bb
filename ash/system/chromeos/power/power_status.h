// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_H_
#define ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// PowerStatus is a singleton that receives updates about the system's
// power status from chromeos::PowerManagerClient and makes the information
// available to interested classes within Ash.
class ASH_EXPORT PowerStatus : public chromeos::PowerManagerClient::Observer {
 public:
  // Different styles of battery icons.
  enum IconSet {
    ICON_LIGHT,
    ICON_DARK
  };

  // Interface for classes that wish to be notified when the power status
  // has changed.
  class Observer {
   public:
    // Called when the power status changes.
    virtual void OnPowerStatusChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  // Maximum battery time-to-full or time-to-empty that should be displayed
  // in the UI. If the current is close to zero, battery time estimates can
  // get very large; avoid displaying these large numbers.
  static const int kMaxBatteryTimeToDisplaySec;

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Returns true if the global instance is initialized.
  static bool IsInitialized();

  // Gets the global instance. Initialize must be called first.
  static PowerStatus* Get();

  // Returns true if |time|, a time returned by GetBatteryTimeToEmpty() or
  // GetBatteryTimeToFull(), should be displayed in the UI.
  // Less-than-a-minute or very large values aren't displayed.
  static bool ShouldDisplayBatteryTime(const base::TimeDelta& time);

  // Copies the hour and minute components of |time| to |hours| and |minutes|.
  // The minute component is rounded rather than truncated: a |time| value
  // corresponding to 92 seconds will produce a |minutes| value of 2, for
  // example.
  static void SplitTimeIntoHoursAndMinutes(const base::TimeDelta& time,
                                           int* hours,
                                           int* minutes);

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Requests updated status from the power manager.
  void RequestStatusUpdate();

  // Returns true if a battery is present.
  bool IsBatteryPresent() const;

  // Returns true if the battery is full. This also implies that a charger
  // is connected.
  bool IsBatteryFull() const;

  // Returns true if the battery is charging. Note that this implies that a
  // charger is connected but the converse is not necessarily true: the
  // battery may be discharging even while a (perhaps low-power) charger is
  // connected. Use Is*Connected() to test for the presence of a charger
  // and also see IsBatteryDischargingOnLinePower().
  bool IsBatteryCharging() const;

  // Returns true if the battery is discharging (or neither charging nor
  // discharging while not being full) while line power is connected.
  bool IsBatteryDischargingOnLinePower() const;

  // Returns the battery's remaining charge as a value in the range [0.0,
  // 100.0].
  double GetBatteryPercent() const;

  // Returns the battery's remaining charge, rounded to an integer with a
  // maximum value of 100.
  int GetRoundedBatteryPercent() const;

  // Returns true if the battery's time-to-full and time-to-empty estimates
  // should not be displayed because the power manager is still calculating
  // them.
  bool IsBatteryTimeBeingCalculated() const;

  // Returns the estimated time until the battery is empty (if line power
  // is disconnected) or full (if line power is connected). These estimates
  // should only be used if IsBatteryTimeBeingCalculated() returns false.
  base::TimeDelta GetBatteryTimeToEmpty() const;
  base::TimeDelta GetBatteryTimeToFull() const;

  // Returns true if line power (including a charger of any type) is connected.
  bool IsLinePowerConnected() const;

  // Returns true if an official, non-USB charger is connected.
  bool IsMainsChargerConnected() const;

  // Returns true if a USB charger (which is likely to only support a low
  // charging rate) is connected.
  bool IsUsbChargerConnected() const;

  // Returns true if an original spring charger is connected.
  bool IsOriginalSpringChargerConnected() const;

  // Returns the image that should be shown for the battery's current state.
  gfx::ImageSkia GetBatteryImage(IconSet icon_set) const;

  // Returns an string describing the current state for accessibility.
  base::string16 GetAccessibleNameString(bool full_description) const;

  // Updates |proto_|. Does not notify observers.
  void SetProtoForTesting(const power_manager::PowerSupplyProperties& proto);

 protected:
  PowerStatus();
  virtual ~PowerStatus();

 private:
  // Overriden from PowerManagerClient::Observer.
  virtual void PowerChanged(
      const power_manager::PowerSupplyProperties& proto) OVERRIDE;

  ObserverList<Observer> observers_;

  // Current state.
  power_manager::PowerSupplyProperties proto_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatus);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_H_
