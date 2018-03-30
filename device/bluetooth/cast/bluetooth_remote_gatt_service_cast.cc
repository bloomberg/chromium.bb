// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/cast/bluetooth_remote_gatt_service_cast.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_service.h"
#include "device/bluetooth/cast/bluetooth_device_cast.h"
#include "device/bluetooth/cast/bluetooth_remote_gatt_characteristic_cast.h"

namespace device {

BluetoothRemoteGattServiceCast::BluetoothRemoteGattServiceCast(
    BluetoothDeviceCast* device,
    scoped_refptr<chromecast::bluetooth::RemoteService> remote_service)
    : device_(device),
      remote_service_(std::move(remote_service)),
      weak_factory_(this) {
  remote_service_->GetCharacteristics(
      base::BindOnce(&BluetoothRemoteGattServiceCast::OnGetCharacteristics,
                     weak_factory_.GetWeakPtr()));
}

BluetoothRemoteGattServiceCast::~BluetoothRemoteGattServiceCast() {}

std::string BluetoothRemoteGattServiceCast::GetIdentifier() const {
  return chromecast::bluetooth::util::UuidToString(remote_service_->uuid());
}

BluetoothUUID BluetoothRemoteGattServiceCast::GetUUID() const {
  return BluetoothUUID(
      chromecast::bluetooth::util::UuidToString(remote_service_->uuid()));
}

bool BluetoothRemoteGattServiceCast::IsPrimary() const {
  return remote_service_->primary();
};

BluetoothDevice* BluetoothRemoteGattServiceCast::GetDevice() const {
  return device_;
}

std::vector<BluetoothRemoteGattCharacteristic*>
BluetoothRemoteGattServiceCast::GetCharacteristics() const {
  std::vector<BluetoothRemoteGattCharacteristic*> results;
  results.reserve(characteristics_.size());
  for (auto& characteristic : characteristics_)
    results.push_back(characteristic.get());
  return results;
}

std::vector<BluetoothRemoteGattService*>
BluetoothRemoteGattServiceCast::GetIncludedServices() const {
  NOTIMPLEMENTED();
  return std::vector<BluetoothRemoteGattService*>();
}

BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattServiceCast::GetCharacteristic(
    const std::string& identifier) const {
  for (auto& characteristic : characteristics_) {
    if (characteristic->GetIdentifier() == identifier)
      return characteristic.get();
  }
  return nullptr;
}

void BluetoothRemoteGattServiceCast::OnGetCharacteristics(
    std::vector<scoped_refptr<chromecast::bluetooth::RemoteCharacteristic>>
        characteristics) {
  characteristics_.reserve(characteristics.size());
  for (auto& characteristic : characteristics) {
    characteristics_.push_back(
        std::make_unique<BluetoothRemoteGattCharacteristicCast>(
            this, characteristic));
  }
  SetDiscoveryComplete(true);
}

}  // namespace device
