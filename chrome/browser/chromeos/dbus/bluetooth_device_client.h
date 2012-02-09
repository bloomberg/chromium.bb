// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_DEVICE_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_DEVICE_CLIENT_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "base/observer_list.h"
#include "base/values.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

class BluetoothAdapterClient;

// BluetoothDeviceClient is used to communicate with the bluez Device
// and Input interfaces.
class BluetoothDeviceClient {
 public:
  // Interface for observing changes from a remote bluetooth device.
  class Observer {
   public:
    virtual ~Observer() {}
  };

  virtual ~BluetoothDeviceClient();

  // Adds and removes observers for events on the device with object path
  // |device_path|.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Creates the instance.
  static BluetoothDeviceClient* Create(dbus::Bus* bus,
                                       BluetoothAdapterClient* adapter_client);

 protected:
  BluetoothDeviceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceClient);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_BLUETOOTH_DEVICE_CLIENT_H_
