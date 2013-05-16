// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_POWER_POWER_MANAGER_HANDLER_H_
#define CHROMEOS_POWER_POWER_MANAGER_HANDLER_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "chromeos/dbus/power_manager_client.h"

namespace chromeos {

class PowerManagerHandlerTest;

class CHROMEOS_EXPORT PowerManagerHandler
    : public PowerManagerClient::Observer {
 public:
  class Observer {
   public:
    // Called when power status changed.
    virtual void OnPowerStatusChanged(
        const PowerSupplyStatus& power_status);

   protected:
    Observer();
    virtual ~Observer();
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  virtual ~PowerManagerHandler();

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Returns true if the global instance is initialized.
  static bool IsInitialized();

  // Gets the global instance. Initialize must be called first.
  static PowerManagerHandler* Get();

  // Adds an observer.
  virtual void AddObserver(Observer* observer);

  // Removes an observer.
  virtual void RemoveObserver(Observer* observer);

  // Requests power status update.
  void RequestStatusUpdate();

  // Gets the current power supply status.
  PowerSupplyStatus GetPowerSupplyStatus() const;

 protected:
  PowerManagerHandler();

 private:
  friend class PowerManagerHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(PowerManagerHandlerTest,
                           PowerManagerInitializeAndUpdate);

  // Overriden from PowerManagerClient::Observer.
  virtual void PowerChanged(
      const PowerSupplyStatus& power_status) OVERRIDE;

  ObserverList<Observer> observers_;

  // PowerSupplyStatus state.
  PowerSupplyStatus power_supply_status_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_POWER_POWER_MANAGER_HANDLER_H_
