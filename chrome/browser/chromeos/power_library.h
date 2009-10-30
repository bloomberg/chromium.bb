// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_POWER_LIBRARY_H_

#include "base/observer_list.h"
#include "base/singleton.h"
#include "base/time.h"
#include "third_party/cros/chromeos_power.h"

namespace chromeos {

// This class handles the interaction with the ChromeOS power library APIs.
// Classes can add themselves as observers. Users can get an instance of this
// library class like this: PowerLibrary::Get()
class PowerLibrary {
 public:
  class Observer {
   public:
    virtual void PowerChanged(PowerLibrary* obj) = 0;
  };

  // This gets the singleton PowerLibrary
  static PowerLibrary* Get();

  // Returns true if the ChromeOS library was loaded.
  static bool loaded();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Whether or not the line power is connected.
  bool line_power_on() const;

  // Whether or not the battery is fully charged..
  bool battery_fully_charged() const;

  // The percentage (0-100) of remaining battery.
  double battery_percentage() const;

  // Whether there is a battery present.
  bool battery_is_present() const;

  // The amount of time until battery is empty.
  base::TimeDelta battery_time_to_empty() const;

  // The amount of time until battery is full.
  base::TimeDelta battery_time_to_full() const;

 private:
  friend struct DefaultSingletonTraits<PowerLibrary>;

  PowerLibrary();
  ~PowerLibrary();

  // This method is called when there's a change in power status.
  // This method is called on a background thread.
  static void PowerStatusChangedHandler(void* object,
                                        const chromeos::PowerStatus& status);

  // This methods starts the monitoring of power changes.
  void Init();

  // Called by the handler to update the power status.
  // This will notify all the Observers.
  void UpdatePowerStatus(const chromeos::PowerStatus& status);

  ObserverList<Observer> observers_;

  // A reference to the battery power api, to allow callbacks when the battery
  // status changes.
  chromeos::PowerStatusConnection power_status_connection_;

  // The latest power status.
  chromeos::PowerStatus status_;

  DISALLOW_COPY_AND_ASSIGN(PowerLibrary);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_LIBRARY_H_
