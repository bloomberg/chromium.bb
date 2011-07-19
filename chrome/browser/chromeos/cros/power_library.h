// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_POWER_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_POWER_LIBRARY_H_
#pragma once

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "third_party/cros/chromeos_power.h"

namespace chromeos {

// This interface defines interaction with the ChromeOS power library APIs.
// Classes can add themselves as observers. Users can get an instance of this
// library class like this: chromeos::CrosLibrary::Get()->GetPowerLibrary()
class PowerLibrary {
 public:
  class Observer {
   public:
    virtual void PowerChanged(PowerLibrary* obj) = 0;
    virtual void SystemResumed() = 0;
  };
  virtual ~PowerLibrary() {}
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  // Whether or not the line power is connected.
  virtual bool line_power_on() const = 0;

  // Whether or not the battery is fully charged..
  virtual bool battery_fully_charged() const = 0;

  // The percentage (0-100) of remaining battery.
  virtual double battery_percentage() const = 0;

  // Whether there is a battery present.
  virtual bool battery_is_present() const = 0;

  // The amount of time until battery is empty.
  virtual base::TimeDelta battery_time_to_empty() const = 0;

  // The amount of time until battery is full.
  virtual base::TimeDelta battery_time_to_full() const = 0;

  // Enable/disable screen lock for current session.
  virtual void EnableScreenLock(bool enable) = 0;

  // Requests restart of the system.
  virtual void RequestRestart() = 0;

  // Requests shutdown of the system.
  virtual void RequestShutdown() = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static PowerLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_POWER_LIBRARY_H_
