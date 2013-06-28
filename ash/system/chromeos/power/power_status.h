// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_H_
#define ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/power_supply_status.h"

namespace ash {
namespace internal {

class ASH_EXPORT PowerStatus : public chromeos::PowerManagerClient::Observer {
 public:
  class Observer {
   public:
    // Called when the power status changes.
    virtual void OnPowerStatusChanged(
        const chromeos::PowerSupplyStatus& power_status) = 0;

   protected:
    virtual ~Observer() {}
  };

  virtual ~PowerStatus();

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Returns true if the global instance is initialized.
  static bool IsInitialized();

  // Gets the global instance. Initialize must be called first.
  static PowerStatus* Get();

  // Adds an observer.
  virtual void AddObserver(Observer* observer);

  // Removes an observer.
  virtual void RemoveObserver(Observer* observer);

  // Requests power status update.
  void RequestStatusUpdate();

  // Gets the current power supply status.
  chromeos::PowerSupplyStatus GetPowerSupplyStatus() const;

 protected:
  PowerStatus();

 private:
  // Overriden from PowerManagerClient::Observer.
  virtual void PowerChanged(
      const chromeos::PowerSupplyStatus& power_status) OVERRIDE;

  ObserverList<Observer> observers_;

  // PowerSupplyStatus state.
  chromeos::PowerSupplyStatus power_supply_status_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatus);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_H_
