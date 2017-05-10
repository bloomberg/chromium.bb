// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_SCHEDULER_BATTERY_LISTENER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_SCHEDULER_BATTERY_LISTENER_H_

#include <memory>

#include "base/observer_list.h"
#include "base/power_monitor/power_observer.h"
#include "components/download/public/download_params.h"

namespace download {

// Listen to battery charing state changes and notify observers in download
// service.
// TODO(xingliu): Converts to device service when it's fully done.
class BatteryListener : public base::PowerObserver {
 public:
  class Observer {
   public:
    // Called when certain battery requirements are meet.
    virtual void OnBatteryChange(
        SchedulingParams::BatteryRequirements battery_status) = 0;
  };

  BatteryListener();
  ~BatteryListener() override;

  // Retrieves the current minimum battery requirement.
  SchedulingParams::BatteryRequirements CurrentBatteryStatus() const;

  // Start to listen to battery change.
  void Start();

  // Stop to listen to battery change.
  void Stop();

  // Adds/Removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // base::PowerObserver implementation.
  void OnPowerStateChange(bool on_battery_power) override;

  // Notifies |observers_| about battery requirement change.
  void NotifyBatteryChange(SchedulingParams::BatteryRequirements);

  // Observers to monitor battery change in download service.
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(BatteryListener);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_SCHEDULER_BATTERY_LISTENER_H_
