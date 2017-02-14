// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_mac.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_device_mac.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_mac.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_mac.h"

namespace device {

namespace {

static BluetoothGattCharacteristic::Properties ConvertProperties(
    CBCharacteristicProperties cb_property) {
  BluetoothGattCharacteristic::Properties result =
      BluetoothGattCharacteristic::PROPERTY_NONE;
  if (cb_property & CBCharacteristicPropertyBroadcast) {
    result |= BluetoothGattCharacteristic::PROPERTY_BROADCAST;
  }
  if (cb_property & CBCharacteristicPropertyRead) {
    result |= BluetoothGattCharacteristic::PROPERTY_READ;
  }
  if (cb_property & CBCharacteristicPropertyWriteWithoutResponse) {
    result |= BluetoothGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE;
  }
  if (cb_property & CBCharacteristicPropertyWrite) {
    result |= BluetoothGattCharacteristic::PROPERTY_WRITE;
  }
  if (cb_property & CBCharacteristicPropertyNotify) {
    result |= BluetoothGattCharacteristic::PROPERTY_NOTIFY;
  }
  if (cb_property & CBCharacteristicPropertyIndicate) {
    result |= BluetoothGattCharacteristic::PROPERTY_INDICATE;
  }
  if (cb_property & CBCharacteristicPropertyAuthenticatedSignedWrites) {
    result |= BluetoothGattCharacteristic::PROPERTY_AUTHENTICATED_SIGNED_WRITES;
  }
  if (cb_property & CBCharacteristicPropertyExtendedProperties) {
    result |= BluetoothGattCharacteristic::PROPERTY_EXTENDED_PROPERTIES;
  }
  if (cb_property & CBCharacteristicPropertyNotifyEncryptionRequired) {
    // This property is used only in CBMutableCharacteristic
    // (local characteristic). So this value should never appear for
    // CBCharacteristic (remote characteristic). Apple is not able to send
    // this value over BLE since it is not part of the spec.
    DCHECK(false);
    result |= BluetoothGattCharacteristic::PROPERTY_NOTIFY;
  }
  if (cb_property & CBCharacteristicPropertyIndicateEncryptionRequired) {
    // This property is used only in CBMutableCharacteristic
    // (local characteristic). So this value should never appear for
    // CBCharacteristic (remote characteristic). Apple is not able to send
    // this value over BLE since it is not part of the spec.
    DCHECK(false);
    result |= BluetoothGattCharacteristic::PROPERTY_INDICATE;
  }
  return result;
}
}  // namespace

BluetoothRemoteGattCharacteristicMac::BluetoothRemoteGattCharacteristicMac(
    BluetoothRemoteGattServiceMac* gatt_service,
    CBCharacteristic* cb_characteristic)
    : gatt_service_(gatt_service),
      cb_characteristic_(cb_characteristic, base::scoped_policy::RETAIN),
      characteristic_value_read_or_write_in_progress_(false) {
  uuid_ = BluetoothAdapterMac::BluetoothUUIDWithCBUUID(
      [cb_characteristic_.get() UUID]);
  identifier_ = base::SysNSStringToUTF8(
      [NSString stringWithFormat:@"%s-%p", uuid_.canonical_value().c_str(),
                                 (void*)cb_characteristic_]);
}

BluetoothRemoteGattCharacteristicMac::~BluetoothRemoteGattCharacteristicMac() {
  if (!read_characteristic_value_callbacks_.first.is_null()) {
    std::pair<ValueCallback, ErrorCallback> callbacks;
    callbacks.swap(read_characteristic_value_callbacks_);
    callbacks.second.Run(BluetoothGattService::GATT_ERROR_FAILED);
  }
  if (!write_characteristic_value_callbacks_.first.is_null()) {
    std::pair<base::Closure, ErrorCallback> callbacks;
    callbacks.swap(write_characteristic_value_callbacks_);
    callbacks.second.Run(BluetoothGattService::GATT_ERROR_FAILED);
  }
}

std::string BluetoothRemoteGattCharacteristicMac::GetIdentifier() const {
  return identifier_;
}

BluetoothUUID BluetoothRemoteGattCharacteristicMac::GetUUID() const {
  return uuid_;
}

BluetoothGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicMac::GetProperties() const {
  return ConvertProperties(cb_characteristic_.get().properties);
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicMac::GetPermissions() const {
  // Not supported for remote characteristics for CoreBluetooth.
  NOTIMPLEMENTED();
  return PERMISSION_NONE;
}

const std::vector<uint8_t>& BluetoothRemoteGattCharacteristicMac::GetValue()
    const {
  return value_;
}

BluetoothRemoteGattService* BluetoothRemoteGattCharacteristicMac::GetService()
    const {
  return static_cast<BluetoothRemoteGattService*>(gatt_service_);
}

bool BluetoothRemoteGattCharacteristicMac::IsNotifying() const {
  return cb_characteristic_.get().isNotifying == YES;
}

std::vector<BluetoothRemoteGattDescriptor*>
BluetoothRemoteGattCharacteristicMac::GetDescriptors() const {
  std::vector<BluetoothRemoteGattDescriptor*> gatt_descriptors;
  for (const auto& iter : gatt_descriptor_macs_) {
    BluetoothRemoteGattDescriptor* gatt_descriptor =
        static_cast<BluetoothRemoteGattDescriptor*>(iter.second.get());
    gatt_descriptors.push_back(gatt_descriptor);
  }
  return gatt_descriptors;
}

BluetoothRemoteGattDescriptor*
BluetoothRemoteGattCharacteristicMac::GetDescriptor(
    const std::string& identifier) const {
  auto searched_pair = gatt_descriptor_macs_.find(identifier);
  if (searched_pair == gatt_descriptor_macs_.end()) {
    return nullptr;
  }
  return static_cast<BluetoothRemoteGattDescriptor*>(
      searched_pair->second.get());
}

void BluetoothRemoteGattCharacteristicMac::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  if (!IsReadable()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED));
    return;
  }
  if (characteristic_value_read_or_write_in_progress_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }
  characteristic_value_read_or_write_in_progress_ = true;
  read_characteristic_value_callbacks_ =
      std::make_pair(callback, error_callback);
  [GetCBPeripheral() readValueForCharacteristic:cb_characteristic_];
}

void BluetoothRemoteGattCharacteristicMac::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!IsWritable()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   BluetoothRemoteGattService::GATT_ERROR_NOT_PERMITTED));
    return;
  }
  if (characteristic_value_read_or_write_in_progress_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }
  characteristic_value_read_or_write_in_progress_ = true;
  write_characteristic_value_callbacks_ =
      std::make_pair(callback, error_callback);
  base::scoped_nsobject<NSData> nsdata_value(
      [[NSData alloc] initWithBytes:value.data() length:value.size()]);
  CBCharacteristicWriteType write_type = GetCBWriteType();
  [GetCBPeripheral() writeValue:nsdata_value
              forCharacteristic:cb_characteristic_
                           type:write_type];
  if (write_type == CBCharacteristicWriteWithoutResponse) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&BluetoothRemoteGattCharacteristicMac::DidWriteValue,
                   base::Unretained(this), nil));
  }
}

void BluetoothRemoteGattCharacteristicMac::SubscribeToNotifications(
    BluetoothRemoteGattDescriptor* ccc_descriptor,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK(subscribe_to_notification_callbacks_.first.is_null());
  DCHECK(subscribe_to_notification_callbacks_.second.is_null());
  DCHECK(unsubscribe_from_notification_callbacks_.first.is_null());
  DCHECK(unsubscribe_from_notification_callbacks_.second.is_null());
  subscribe_to_notification_callbacks_ =
      std::make_pair(callback, error_callback);
  [GetCBPeripheral() setNotifyValue:YES
                  forCharacteristic:cb_characteristic_.get()];
}

void BluetoothRemoteGattCharacteristicMac::UnsubscribeFromNotifications(
    BluetoothRemoteGattDescriptor* ccc_descriptor,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DCHECK(subscribe_to_notification_callbacks_.first.is_null());
  DCHECK(subscribe_to_notification_callbacks_.second.is_null());
  DCHECK(unsubscribe_from_notification_callbacks_.first.is_null());
  DCHECK(unsubscribe_from_notification_callbacks_.second.is_null());
  unsubscribe_from_notification_callbacks_ =
      std::make_pair(callback, error_callback);
  [GetCBPeripheral() setNotifyValue:NO
                  forCharacteristic:cb_characteristic_.get()];
}

void BluetoothRemoteGattCharacteristicMac::DiscoverDescriptors() {
  is_discovery_complete_ = false;
  [GetCBPeripheral()
      discoverDescriptorsForCharacteristic:cb_characteristic_.get()];
}

void BluetoothRemoteGattCharacteristicMac::DidUpdateValue(NSError* error) {
  CHECK_EQ(GetCBPeripheral().state, CBPeripheralStateConnected);
  // This method is called when the characteristic is read and when a
  // notification is received.
  if (characteristic_value_read_or_write_in_progress_) {
    std::pair<ValueCallback, ErrorCallback> callbacks;
    callbacks.swap(read_characteristic_value_callbacks_);
    characteristic_value_read_or_write_in_progress_ = false;
    if (error) {
      VLOG(1) << "Bluetooth error while reading for characteristic, domain: "
              << base::SysNSStringToUTF8(error.domain)
              << ", error code: " << error.code;
      BluetoothGattService::GattErrorCode error_code =
          BluetoothDeviceMac::GetGattErrorCodeFromNSError(error);
      callbacks.second.Run(error_code);
      return;
    }
    UpdateValueAndNotify();
    callbacks.first.Run(value_);
  } else if (IsNotifying()) {
    UpdateValueAndNotify();
  } else {
    // In case of buggy device, nothing should be done if receiving extra
    // read confirmation.
    VLOG(1) << "Characteristic value updated while having no pending read nor "
               "notification.";
  }
}

void BluetoothRemoteGattCharacteristicMac::UpdateValueAndNotify() {
  NSData* nsdata_value = cb_characteristic_.get().value;
  const uint8_t* buffer = static_cast<const uint8_t*>(nsdata_value.bytes);
  value_.assign(buffer, buffer + nsdata_value.length);
  gatt_service_->GetMacAdapter()->NotifyGattCharacteristicValueChanged(this,
                                                                       value_);
}

void BluetoothRemoteGattCharacteristicMac::DidWriteValue(NSError* error) {
  CHECK_EQ(GetCBPeripheral().state, CBPeripheralStateConnected);
  if (!characteristic_value_read_or_write_in_progress_) {
    // In case of buggy device, nothing should be done if receiving extra
    // write confirmation.
    VLOG(1) << "Write notification while no write operation pending.";
    return;
  }
  std::pair<base::Closure, ErrorCallback> callbacks;
  callbacks.swap(write_characteristic_value_callbacks_);
  characteristic_value_read_or_write_in_progress_ = false;
  if (error) {
    VLOG(1) << "Bluetooth error while writing for characteristic, domain: "
            << base::SysNSStringToUTF8(error.domain)
            << ", error code: " << error.code;
    BluetoothGattService::GattErrorCode error_code =
        BluetoothDeviceMac::GetGattErrorCodeFromNSError(error);
    callbacks.second.Run(error_code);
    return;
  }
  callbacks.first.Run();
}

void BluetoothRemoteGattCharacteristicMac::DidUpdateNotificationState(
    NSError* error) {
  PendingNotifyCallbacks reentrant_safe_callbacks;
  if (!subscribe_to_notification_callbacks_.first.is_null()) {
    DCHECK([GetCBCharacteristic() isNotifying] || error);
    reentrant_safe_callbacks.swap(subscribe_to_notification_callbacks_);
  } else if (!unsubscribe_from_notification_callbacks_.first.is_null()) {
    DCHECK(![GetCBCharacteristic() isNotifying] || error);
    reentrant_safe_callbacks.swap(unsubscribe_from_notification_callbacks_);
  } else {
    VLOG(1) << "No pending notification update for characteristic "
            << GetUUID().value();
    return;
  }
  if (error) {
    VLOG(1) << "Bluetooth error while modifying notification state for "
               "characteristic, domain: "
            << base::SysNSStringToUTF8(error.domain)
            << ", error code: " << error.code << ", localized description: "
            << base::SysNSStringToUTF8(error.localizedDescription);
    BluetoothGattService::GattErrorCode error_code =
        BluetoothDeviceMac::GetGattErrorCodeFromNSError(error);
    reentrant_safe_callbacks.second.Run(error_code);
    return;
  }
  reentrant_safe_callbacks.first.Run();
}

void BluetoothRemoteGattCharacteristicMac::DidDiscoverDescriptors() {
  DCHECK(!is_discovery_complete_);
  std::unordered_set<std::string> descriptor_identifier_to_remove;
  for (const auto& iter : gatt_descriptor_macs_) {
    descriptor_identifier_to_remove.insert(iter.first);
  }

  for (CBDescriptor* cb_descriptor in cb_characteristic_.get().descriptors) {
    BluetoothRemoteGattDescriptorMac* gatt_descriptor_mac =
        GetBluetoothRemoteGattDescriptorMac(cb_descriptor);
    if (gatt_descriptor_mac) {
      const std::string& identifier = gatt_descriptor_mac->GetIdentifier();
      descriptor_identifier_to_remove.erase(identifier);
      continue;
    }
    gatt_descriptor_mac =
        new BluetoothRemoteGattDescriptorMac(this, cb_descriptor);
    const std::string& identifier = gatt_descriptor_mac->GetIdentifier();
    auto result_iter = gatt_descriptor_macs_.insert(
        {identifier, base::WrapUnique(gatt_descriptor_mac)});
    DCHECK(result_iter.second);
    GetMacAdapter()->NotifyGattDescriptorAdded(gatt_descriptor_mac);
  }

  for (const std::string& identifier : descriptor_identifier_to_remove) {
    auto pair_to_remove = gatt_descriptor_macs_.find(identifier);
    std::unique_ptr<BluetoothRemoteGattDescriptorMac> descriptor_to_remove;
    pair_to_remove->second.swap(descriptor_to_remove);
    gatt_descriptor_macs_.erase(pair_to_remove);
    GetMacAdapter()->NotifyGattDescriptorRemoved(descriptor_to_remove.get());
  }
  is_discovery_complete_ = true;
}

bool BluetoothRemoteGattCharacteristicMac::IsReadable() const {
  return GetProperties() & BluetoothGattCharacteristic::PROPERTY_READ;
}

bool BluetoothRemoteGattCharacteristicMac::IsWritable() const {
  BluetoothGattCharacteristic::Properties properties = GetProperties();
  return (properties & BluetoothGattCharacteristic::PROPERTY_WRITE) ||
         (properties & PROPERTY_WRITE_WITHOUT_RESPONSE);
}

bool BluetoothRemoteGattCharacteristicMac::SupportsNotificationsOrIndications()
    const {
  BluetoothGattCharacteristic::Properties properties = GetProperties();
  return (properties & PROPERTY_NOTIFY) || (properties & PROPERTY_INDICATE);
}

CBCharacteristicWriteType BluetoothRemoteGattCharacteristicMac::GetCBWriteType()
    const {
  return (GetProperties() & BluetoothGattCharacteristic::PROPERTY_WRITE)
             ? CBCharacteristicWriteWithResponse
             : CBCharacteristicWriteWithoutResponse;
}

CBCharacteristic* BluetoothRemoteGattCharacteristicMac::GetCBCharacteristic()
    const {
  return cb_characteristic_.get();
}

BluetoothAdapterMac* BluetoothRemoteGattCharacteristicMac::GetMacAdapter()
    const {
  return gatt_service_->GetMacAdapter();
}

CBPeripheral* BluetoothRemoteGattCharacteristicMac::GetCBPeripheral() const {
  return gatt_service_->GetCBPeripheral();
}

bool BluetoothRemoteGattCharacteristicMac::IsDiscoveryComplete() const {
  return is_discovery_complete_;
}

BluetoothRemoteGattDescriptorMac*
BluetoothRemoteGattCharacteristicMac::GetBluetoothRemoteGattDescriptorMac(
    CBDescriptor* cb_descriptor) const {
  auto found = std::find_if(
      gatt_descriptor_macs_.begin(), gatt_descriptor_macs_.end(),
      [cb_descriptor](
          const std::pair<const std::string,
                          std::unique_ptr<BluetoothRemoteGattDescriptorMac>>&
              pair) {
        return pair.second->GetCBDescriptor() == cb_descriptor;
      });
  if (found == gatt_descriptor_macs_.end()) {
    return nullptr;
  } else {
    return found->second.get();
  }
}
}  // namespace device.
