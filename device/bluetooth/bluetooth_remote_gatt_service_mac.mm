// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_service_mac.h"

#import <CoreBluetooth/CoreBluetooth.h>
#include <vector>

#include "base/logging.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_low_energy_device_mac.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

BluetoothRemoteGattServiceMac::BluetoothRemoteGattServiceMac(
    BluetoothLowEnergyDeviceMac* bluetooth_device_mac,
    CBService* service,
    bool is_primary)
    : bluetooth_device_mac_(bluetooth_device_mac),
      service_(service, base::scoped_policy::RETAIN),
      is_primary_(is_primary) {
  uuid_ = BluetoothAdapterMac::BluetoothUUIDWithCBUUID([service_.get() UUID]);
  identifier_ =
      [NSString stringWithFormat:@"%s-%p", uuid_.canonical_value().c_str(),
                                 (void*)service_]
          .UTF8String;
}

BluetoothRemoteGattServiceMac::~BluetoothRemoteGattServiceMac() {}

std::string BluetoothRemoteGattServiceMac::GetIdentifier() const {
  return identifier_;
}

BluetoothUUID BluetoothRemoteGattServiceMac::GetUUID() const {
  return uuid_;
}

bool BluetoothRemoteGattServiceMac::IsPrimary() const {
  return is_primary_;
}

BluetoothDevice* BluetoothRemoteGattServiceMac::GetDevice() const {
  return bluetooth_device_mac_;
}

std::vector<BluetoothRemoteGattCharacteristic*>
BluetoothRemoteGattServiceMac::GetCharacteristics() const {
  NOTIMPLEMENTED();
  return std::vector<BluetoothRemoteGattCharacteristic*>();
}

std::vector<BluetoothRemoteGattService*>
BluetoothRemoteGattServiceMac::GetIncludedServices() const {
  NOTIMPLEMENTED();
  return std::vector<BluetoothRemoteGattService*>();
}

BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattServiceMac::GetCharacteristic(
    const std::string& identifier) const {
  NOTIMPLEMENTED();
  return nullptr;
}

CBService* BluetoothRemoteGattServiceMac::GetService() const {
  return service_.get();
}

}  // namespace device
