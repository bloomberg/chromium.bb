// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"

#include "base/callback.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"

namespace bluez {

BluetoothLocalGattServiceBlueZ::BluetoothLocalGattServiceBlueZ(
    BluetoothAdapterBlueZ* adapter,
    const device::BluetoothUUID& uuid,
    bool is_primary,
    device::BluetoothLocalGattService::Delegate* delegate)
    : BluetoothGattServiceBlueZ(adapter),
      uuid_(uuid),
      is_primary_(is_primary),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  // TODO(rkc): Move this code in a common location. It is used by
  // BluetoothAdvertisementBlueZ() also.
  std::string GuidString = base::GenerateGUID();
  base::RemoveChars(GuidString, "-", &GuidString);
  object_path_ = dbus::ObjectPath(adapter_->object_path().value() +
                                  "/service/" + GuidString);
  VLOG(1) << "Creating local GATT service with identifier: "
          << object_path_.value();
}

BluetoothLocalGattServiceBlueZ::~BluetoothLocalGattServiceBlueZ() {}

device::BluetoothUUID BluetoothLocalGattServiceBlueZ::GetUUID() const {
  return uuid_;
}

bool BluetoothLocalGattServiceBlueZ::IsPrimary() const {
  return is_primary_;
}

// static
base::WeakPtr<device::BluetoothLocalGattService>
BluetoothLocalGattServiceBlueZ::Create(
    device::BluetoothAdapter* adapter,
    const device::BluetoothUUID& uuid,
    bool is_primary,
    BluetoothLocalGattService* /* included_service */,
    BluetoothLocalGattService::Delegate* delegate) {
  BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<BluetoothAdapterBlueZ*>(adapter);
  BluetoothLocalGattServiceBlueZ* service = new BluetoothLocalGattServiceBlueZ(
      adapter_bluez, uuid, is_primary, delegate);
  adapter_bluez->AddLocalGattService(base::WrapUnique(service));
  return service->weak_ptr_factory_.GetWeakPtr();
}

void BluetoothLocalGattServiceBlueZ::Register(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // TODO(rkc): Call adapter_->RegisterGattService.
}

void BluetoothLocalGattServiceBlueZ::Unregister(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // TODO(rkc): Call adapter_->UnregisterGattService.
}

void BluetoothLocalGattServiceBlueZ::OnRegistrationError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(1) << "[Un]Register Service failed: " << error_name
          << ", message: " << error_message;
  error_callback.Run(
      BluetoothGattServiceBlueZ::DBusErrorToServiceError(error_name));
}

}  // namespace bluez
