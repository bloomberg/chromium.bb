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

// PowerManagerClient is used to communicate with the power manager.
class PowerManagerClient
    : public base::RefCountedThreadSafe<PowerManagerClient> {
 public:
  // Interface for observing changes from the power manager.
  class Observer {
   public:
    // Called when the brightness is changed.
    // |level| is of the range [0, 100].
    // |user_initiated| is true if the action is initiated by the user.
    virtual void BrightnessChanged(int level, bool user_initiated) = 0;
  };

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Decreases the screen brightness. |allow_off| controls whether or not
  // it's allowed to turn off the back light.
  virtual void DecreaseScreenBrightness(bool allow_off) = 0;

  // Increases the screen brightness.
  virtual void IncreaseScreenBrightness() = 0;

  // Creates the instance.
  static PowerManagerClient* Create(dbus::Bus* bus);

 protected:
  // These are protected, so we can define sub classes.
  PowerManagerClient();
  virtual ~PowerManagerClient();

 private:
  friend class base::RefCountedThreadSafe<PowerManagerClient>;
  DISALLOW_COPY_AND_ASSIGN(PowerManagerClient);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_POWER_MANAGER_CLIENT_H_
