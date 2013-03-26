// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter.h"

#include "base/stl_util.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

BluetoothAdapter::BluetoothAdapter() {
}

BluetoothAdapter::~BluetoothAdapter() {
  STLDeleteValues(&devices_);
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

BluetoothAdapter::ConstDeviceList BluetoothAdapter::GetDevices() const {
  ConstDeviceList devices;
  for (DevicesMap::const_iterator iter = devices_.begin();
       iter != devices_.end();
       ++iter)
    devices.push_back(iter->second);

  return devices;
}

BluetoothDevice* BluetoothAdapter::GetDevice(const std::string& address) {
  return const_cast<BluetoothDevice *>(
      const_cast<const BluetoothAdapter *>(this)->GetDevice(address));
}

const BluetoothDevice* BluetoothAdapter::GetDevice(
    const std::string& address) const {
  DevicesMap::const_iterator iter = devices_.find(address);
  if (iter != devices_.end())
    return iter->second;

  return NULL;
}

}  // namespace device
