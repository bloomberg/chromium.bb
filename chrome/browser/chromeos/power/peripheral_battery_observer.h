// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_PERIPHERAL_BATTERY_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_PERIPHERAL_BATTERY_OBSERVER_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "chromeos/dbus/power_manager_client.h"
#include "device/bluetooth/bluetooth_adapter.h"

class Profile;

namespace chromeos {

class BluetoothDevice;
class PeripheralBatteryObserverTest;

// This observer listens for peripheral device battery status and shows
// notifications for low battery conditions.
class PeripheralBatteryObserver : public PowerManagerClient::Observer,
                                  public device::BluetoothAdapter::Observer {
 public:
  // This class registers/unregisters itself as an observer in ctor/dtor.
  PeripheralBatteryObserver();
  ~PeripheralBatteryObserver() override;

  void set_testing_clock(base::TickClock* clock) { testing_clock_ = clock; }

  // PowerManagerClient::Observer implementation.
  void PeripheralBatteryStatusReceived(const std::string& path,
                                       const std::string& name,
                                       int level) override;

  // device::BluetoothAdapter::Observer implementation.
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

 private:
  friend class PeripheralBatteryObserverTest;
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryObserverTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryObserverTest, InvalidBatteryInfo);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryObserverTest, DeviceRemove);

  struct BatteryInfo {
    BatteryInfo() : level(-1) {}
    BatteryInfo(const std::string& name,
                int level,
                base::TimeTicks notification_timestamp)
        : name(name),
          level(level),
          last_notification_timestamp(notification_timestamp) {
    }

    // Human readable name for the device. It is changeable.
    std::string name;
    // Battery level within range [0, 100], and -1 for unknown level.
    int level;
    base::TimeTicks last_notification_timestamp;
  };

  void InitializeOnBluetoothReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  void RemoveBattery(const std::string& address);

  // Posts a low battery notification with unique id |address|. Returns true
  // if the notification is posted, false if not.
  bool PostNotification(const std::string& address, const BatteryInfo& battery);

  void CancelNotification(const std::string& address);

  // Record of existing battery infomation. For bluetooth HID device, the key
  // is the address of the bluetooth device.
  std::map<std::string, BatteryInfo> batteries_;

  // PeripheralBatteryObserver is an observer of |bluetooth_adapter_| for
  // bluetooth device change/remove events.
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  // Used only for helping test. Not owned and can be NULL.
  base::TickClock* testing_clock_;

  // Record the profile used when adding message center notifications.
  Profile* notification_profile_;

  std::unique_ptr<base::WeakPtrFactory<PeripheralBatteryObserver>>
      weakptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PeripheralBatteryObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_PERIPHERAL_BATTERY_OBSERVER_H_
