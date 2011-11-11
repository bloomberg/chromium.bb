// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_POWER_MANAGER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_POWER_MANAGER_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"

#include <string>

namespace dbus {
class Bus;
class ObjectProxy;
class Response;
class Signal;
}  // namespace

namespace chromeos {

// This is the local struct that is used in Chrome.
struct PowerSupplyStatus {
  bool line_power_on;

  bool battery_is_present;
  bool battery_is_full;

  // Time in seconds until the battery is empty or full, 0 for unknown.
  int64 battery_seconds_to_empty;
  int64 battery_seconds_to_full;

  double battery_percentage;

  PowerSupplyStatus();
  const std::string& ToString() const;
};

// PowerManagerClient is used to communicate with the power manager.
class PowerManagerClient {
 public:
  // Interface for observing changes from the power manager.
  class Observer {
   public:
    // Called when the brightness is changed.
    // |level| is of the range [0, 100].
    // |user_initiated| is true if the action is initiated by the user.
    virtual void BrightnessChanged(int level, bool user_initiated) {}

    // Called when power supply polling takes place.  |status| is a data
    // structure that contains the current state of the power supply.
    virtual void PowerChanged(const PowerSupplyStatus& status) {}
  };

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Decreases the screen brightness. |allow_off| controls whether or not
  // it's allowed to turn off the back light.
  virtual void DecreaseScreenBrightness(bool allow_off) = 0;

  // Increases the screen brightness.
  virtual void IncreaseScreenBrightness() = 0;

  // UI initiated request for power supply status update.
  virtual void RequestStatusUpdate() = 0;

  // Creates the instance.
  static PowerManagerClient* Create(dbus::Bus* bus);

  virtual ~PowerManagerClient();

 protected:
  // Create() should be used instead.
  PowerManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerManagerClient);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_POWER_MANAGER_CLIENT_H_
