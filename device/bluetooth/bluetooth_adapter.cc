// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter.h"

#include "device/bluetooth/bluetooth_device.h"

namespace device {

BluetoothAdapter::~BluetoothAdapter() {
}

const std::string& BluetoothAdapter::address() const {
  return address_;
}

const std::string& BluetoothAdapter::name() const {
  return name_;
}

BluetoothAdapter::DeviceList BluetoothAdapter::GetDevices() {
  ConstDeviceList const_devices =
    const_cast<const BluetoothAdapter *>(this)->GetDevices();

  DeviceList devices;
  for (ConstDeviceList::const_iterator i = const_devices.begin();
       i != const_devices.end(); ++i)
    devices.push_back(const_cast<BluetoothDevice *>(*i));

  return devices;
}

}  // namespace device
