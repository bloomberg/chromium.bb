// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/ble/bluetooth_low_energy_characteristics_finder.h"

#include "components/proximity_auth/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_uuid.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothRemoteGattCharacteristic;
using device::BluetoothRemoteGattService;
using device::BluetoothUUID;

namespace cryptauth {

BluetoothLowEnergyCharacteristicsFinder::
    BluetoothLowEnergyCharacteristicsFinder(
        scoped_refptr<BluetoothAdapter> adapter,
        BluetoothDevice* device,
        const RemoteAttribute& remote_service,
        const RemoteAttribute& to_peripheral_char,
        const RemoteAttribute& from_peripheral_char,
        const SuccessCallback& success_callback,
        const ErrorCallback& error_callback)
    : adapter_(adapter),
      remote_service_(remote_service),
      to_peripheral_char_(to_peripheral_char),
      from_peripheral_char_(from_peripheral_char),
      success_callback_(success_callback),
      error_callback_(error_callback) {
  if (!adapter_) {
    error_callback_.Run(to_peripheral_char_, from_peripheral_char_);
    ResetCallbacks();
    return;
  }

  adapter_->AddObserver(this);
  ScanRemoteCharacteristics(device, remote_service_.uuid);

  // TODO(sacomoto): implement a timeout for characteristic discovery.
}

BluetoothLowEnergyCharacteristicsFinder::
    BluetoothLowEnergyCharacteristicsFinder() {}

BluetoothLowEnergyCharacteristicsFinder::
    ~BluetoothLowEnergyCharacteristicsFinder() {
  ResetCallbacks();
  if (adapter_) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
}

void BluetoothLowEnergyCharacteristicsFinder::GattCharacteristicAdded(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic) {
  HandleCharacteristicUpdate(characteristic);
}

void BluetoothLowEnergyCharacteristicsFinder::GattDiscoveryCompleteForService(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattService* service) {
  if (service && service->GetUUID() == remote_service_.uuid) {
    if (to_peripheral_char_.id.empty() || from_peripheral_char_.id.empty()) {
      if (!error_callback_.is_null()) {
        error_callback_.Run(to_peripheral_char_, from_peripheral_char_);
        ResetCallbacks();
      }
    }
  }
}

void BluetoothLowEnergyCharacteristicsFinder::ScanRemoteCharacteristics(
    BluetoothDevice* device,
    const BluetoothUUID& service_uuid) {
  if (device) {
    std::vector<BluetoothRemoteGattService*> services =
        device->GetGattServices();
    for (const auto* service : services) {
      if (service->GetUUID() == service_uuid) {
        // Right service found, now scaning its characteristics.
        std::vector<device::BluetoothRemoteGattCharacteristic*>
            characteristics = service->GetCharacteristics();
        for (auto* characteristic : characteristics) {
          HandleCharacteristicUpdate(characteristic);
        }
        break;
      }
    }
  }
}

void BluetoothLowEnergyCharacteristicsFinder::HandleCharacteristicUpdate(
    BluetoothRemoteGattCharacteristic* characteristic) {
  UpdateCharacteristicsStatus(characteristic);

  if (!to_peripheral_char_.id.empty() && !from_peripheral_char_.id.empty() &&
      !success_callback_.is_null()) {
    success_callback_.Run(remote_service_, to_peripheral_char_,
                          from_peripheral_char_);
    ResetCallbacks();
  }
}

void BluetoothLowEnergyCharacteristicsFinder::UpdateCharacteristicsStatus(
    BluetoothRemoteGattCharacteristic* characteristic) {
  if (characteristic &&
      characteristic->GetService()->GetUUID() == remote_service_.uuid) {
    BluetoothUUID uuid = characteristic->GetUUID();
    if (to_peripheral_char_.uuid == uuid)
      to_peripheral_char_.id = characteristic->GetIdentifier();
    if (from_peripheral_char_.uuid == uuid)
      from_peripheral_char_.id = characteristic->GetIdentifier();

    BluetoothRemoteGattService* service = characteristic->GetService();
    remote_service_.id = service->GetIdentifier();
  }
}

void BluetoothLowEnergyCharacteristicsFinder::ResetCallbacks() {
  success_callback_.Reset();
  error_callback_.Reset();
}

}  // namespace cryptauth
