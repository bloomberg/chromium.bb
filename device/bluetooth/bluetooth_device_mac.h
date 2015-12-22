// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_MAC_H_

#include "base/macros.h"
#include "device/bluetooth/bluetooth_device.h"

@class NSDate;

namespace device {

class BluetoothAdapterMac;

class BluetoothDeviceMac : public BluetoothDevice {
 public:
  ~BluetoothDeviceMac() override;

  // Returns the time of the most recent interaction with the device.  Returns
  // nil if the device has never been seen.
  virtual NSDate* GetLastUpdateTime() const = 0;

 protected:
  BluetoothDeviceMac(BluetoothAdapterMac* adapter);

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_MAC_H_
