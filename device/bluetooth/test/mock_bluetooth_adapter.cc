// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_adapter.h"

#include "device/bluetooth/test/mock_bluetooth_advertisement.h"

namespace device {

using testing::Invoke;
using testing::_;

MockBluetoothAdapter::Observer::Observer() {}
MockBluetoothAdapter::Observer::~Observer() {}

MockBluetoothAdapter::MockBluetoothAdapter() {
  ON_CALL(*this, AddObserver(_))
      .WillByDefault(Invoke([this](BluetoothAdapter::Observer* observer) {
        this->BluetoothAdapter::AddObserver(observer);
      }));
  ON_CALL(*this, RemoveObserver(_))
      .WillByDefault(Invoke([this](BluetoothAdapter::Observer* observer) {
        this->BluetoothAdapter::RemoveObserver(observer);
      }));
}

MockBluetoothAdapter::~MockBluetoothAdapter() {}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
void MockBluetoothAdapter::Shutdown() {
}
#endif

void MockBluetoothAdapter::AddDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {}

void MockBluetoothAdapter::RemoveDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {}

void MockBluetoothAdapter::SetDiscoveryFilter(
    scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  SetDiscoveryFilterRaw(discovery_filter.get(), callback, error_callback);
}

void MockBluetoothAdapter::StartDiscoverySessionWithFilter(
    scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const DiscoverySessionCallback& callback,
    const ErrorCallback& error_callback) {
  StartDiscoverySessionWithFilterRaw(discovery_filter.get(), callback,
                                     error_callback);
}

void MockBluetoothAdapter::AddMockDevice(
    scoped_ptr<MockBluetoothDevice> mock_device) {
  mock_devices_.push_back(mock_device.Pass());
}

BluetoothAdapter::ConstDeviceList MockBluetoothAdapter::GetConstMockDevices() {
  BluetoothAdapter::ConstDeviceList devices;
  for (auto& it : mock_devices_) {
    devices.push_back(it);
  }
  return devices;
}

BluetoothAdapter::DeviceList MockBluetoothAdapter::GetMockDevices() {
  BluetoothAdapter::DeviceList devices;
  for (auto& it : mock_devices_) {
    devices.push_back(it);
  }
  return devices;
}

void MockBluetoothAdapter::RegisterAdvertisement(
    scoped_ptr<BluetoothAdvertisement::Data> advertisement_data,
    const CreateAdvertisementCallback& callback,
    const CreateAdvertisementErrorCallback& error_callback) {
  callback.Run(new MockBluetoothAdvertisement);
}

}  // namespace device
