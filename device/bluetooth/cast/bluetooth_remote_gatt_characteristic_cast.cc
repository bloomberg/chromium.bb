// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/cast/bluetooth_remote_gatt_characteristic_cast.h"

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_descriptor.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/cast/bluetooth_remote_gatt_descriptor_cast.h"
#include "device/bluetooth/cast/bluetooth_remote_gatt_service_cast.h"

namespace device {
namespace {

// Called back when subscribing or unsubscribing to a remote characteristic.
// If |success| is true, run |callback|. Otherwise run |error_callback|.
void OnSubscribeOrUnsubscribe(
    const base::Closure& callback,
    const BluetoothGattCharacteristic::ErrorCallback& error_callback,
    bool success) {
  if (success)
    callback.Run();
  else
    error_callback.Run(BluetoothGattService::GATT_ERROR_FAILED);
}

}  // namespace

BluetoothRemoteGattCharacteristicCast::BluetoothRemoteGattCharacteristicCast(
    BluetoothRemoteGattServiceCast* service,
    scoped_refptr<chromecast::bluetooth::RemoteCharacteristic> characteristic)
    : service_(service),
      remote_characteristic_(std::move(characteristic)),
      weak_factory_(this) {
  auto descriptors = remote_characteristic_->GetDescriptors();
  descriptors_.reserve(descriptors.size());
  for (const auto& descriptor : descriptors) {
    descriptors_.push_back(
        std::make_unique<BluetoothRemoteGattDescriptorCast>(this, descriptor));
  }
}

BluetoothRemoteGattCharacteristicCast::
    ~BluetoothRemoteGattCharacteristicCast() {}

std::string BluetoothRemoteGattCharacteristicCast::GetIdentifier() const {
  return chromecast::bluetooth::util::UuidToString(
      remote_characteristic_->uuid());
}

BluetoothUUID BluetoothRemoteGattCharacteristicCast::GetUUID() const {
  return BluetoothUUID(chromecast::bluetooth::util::UuidToString(
      remote_characteristic_->uuid()));
}

BluetoothGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicCast::GetProperties() const {
  // TODO(slan): Convert these from
  // bluetooth_v2_shlib::Characteristic::properties.
  NOTIMPLEMENTED();
  return Properties();
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicCast::GetPermissions() const {
  // TODO(slan): Convert these from
  // bluetooth_v2_shlib::Characteristic::permissions.
  NOTIMPLEMENTED();
  return Permissions();
}

const std::vector<uint8_t>& BluetoothRemoteGattCharacteristicCast::GetValue()
    const {
  return value_;
}

BluetoothRemoteGattService* BluetoothRemoteGattCharacteristicCast::GetService()
    const {
  return service_;
}

std::vector<BluetoothRemoteGattDescriptor*>
BluetoothRemoteGattCharacteristicCast::GetDescriptors() const {
  std::vector<BluetoothRemoteGattDescriptor*> descriptors;
  descriptors.reserve(descriptors_.size());
  for (auto& descriptor : descriptors_) {
    descriptors.push_back(descriptor.get());
  }
  return descriptors;
}

BluetoothRemoteGattDescriptor*
BluetoothRemoteGattCharacteristicCast::GetDescriptor(
    const std::string& identifier) const {
  for (auto& descriptor : descriptors_) {
    if (descriptor->GetIdentifier() == identifier) {
      return descriptor.get();
    }
  }
  return nullptr;
}

void BluetoothRemoteGattCharacteristicCast::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  remote_characteristic_->Read(base::BindOnce(
      &BluetoothRemoteGattCharacteristicCast::OnReadRemoteCharacteristic,
      weak_factory_.GetWeakPtr(), callback, error_callback));
}

void BluetoothRemoteGattCharacteristicCast::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  remote_characteristic_->Write(
      chromecast::bluetooth_v2_shlib::Gatt::WriteType::WRITE_TYPE_DEFAULT,
      value,
      base::BindOnce(
          &BluetoothRemoteGattCharacteristicCast::OnWriteRemoteCharacteristic,
          weak_factory_.GetWeakPtr(), value, callback, error_callback));
}

void BluetoothRemoteGattCharacteristicCast::SubscribeToNotifications(
    BluetoothRemoteGattDescriptor* ccc_descriptor,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // |remote_characteristic_| exposes a method which writes the CCCD after
  // subscribing the GATT client to the notification. This is syntactically
  // nicer and saves us a thread-hop, so we can ignore |ccc_descriptor| for now.
  (void)ccc_descriptor;

  remote_characteristic_->SetRegisterNotification(
      true,
      base::BindOnce(&OnSubscribeOrUnsubscribe, callback, error_callback));
}

void BluetoothRemoteGattCharacteristicCast::UnsubscribeFromNotifications(
    BluetoothRemoteGattDescriptor* ccc_descriptor,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // |remote_characteristic_| exposes a method which writes the CCCD after
  // subscribing the GATT client to the notification. This is syntactically
  // nicer and saves us a thread-hop, so we can ignore |ccc_descriptor| for now.
  (void)ccc_descriptor;

  // TODO(slan|bcf): Should we actually be using SetRegisterNotification() here
  // to unset the CCCD bit on the peripheral? What does the standard say?
  remote_characteristic_->SetNotification(
      false,
      base::BindOnce(&OnSubscribeOrUnsubscribe, callback, error_callback));
}

void BluetoothRemoteGattCharacteristicCast::OnReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback,
    bool success,
    const std::vector<uint8_t>& result) {
  if (success) {
    value_ = result;
    callback.Run(result);
    return;
  }
  error_callback.Run(BluetoothGattService::GATT_ERROR_FAILED);
}

void BluetoothRemoteGattCharacteristicCast::OnWriteRemoteCharacteristic(
    const std::vector<uint8_t>& written_value,
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    bool success) {
  if (success) {
    value_ = written_value;
    callback.Run();
    return;
  }
  error_callback.Run(BluetoothGattService::GATT_ERROR_FAILED);
}

}  // namespace device
