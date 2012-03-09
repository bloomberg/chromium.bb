// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_DEVICE_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_DEVICE_H_
#pragma once

#include <string>

#include "base/basictypes.h"

namespace chromeos {

class BluetoothDevice {
 public:
  virtual ~BluetoothDevice();

  // Returns the MAC address for this device.
  virtual const std::string& GetAddress() const = 0;

  // Returns the bluetooth class for this device.
  // This is a bitfield consisting of 3 subfields representing service class,
  // major device class, and minor device class.
  // See www.bluetooth.org/spec/ for details.
  // TODO(vlaviano): provide a better API for this.
  virtual uint32 GetBluetoothClass() const = 0;

  // Returns the suggested icon type for this device.
  // See http://standards.freedesktop.org/icon-naming-spec/ for details.
  virtual const std::string& GetIcon() const = 0;

  // Returns the name for this device.
  virtual const std::string& GetName() const = 0;

  // Returns whether or not this device is paired.
  virtual bool IsPaired() const = 0;

  // Returns whether or not this device is connected.
  virtual bool IsConnected() const = 0;

  // Creates a device with property values based on |properties|.
  static BluetoothDevice* Create(const std::string& address,
                                 uint32 bluetooth_class,
                                 const std::string& icon,
                                 const std::string& name,
                                 bool paired,
                                 bool connected);

 protected:
  BluetoothDevice();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothDevice);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_DEVICE_H_
