// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_advertisement_manager_mac.h"

#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace device {

BluetoothLowEnergyAdvertisementManagerMac::
    BluetoothLowEnergyAdvertisementManagerMac()
    : active_advertisement_(nullptr) {}

BluetoothLowEnergyAdvertisementManagerMac::
    ~BluetoothLowEnergyAdvertisementManagerMac() {}

void BluetoothLowEnergyAdvertisementManagerMac::TryStartAdvertisement() {
  if (!active_advertisement_ ||
      active_advertisement_->status() !=
          BluetoothAdvertisementMac::WAITING_FOR_PERIPHERAL_MANAGER) {
    return;
  }

  if ([peripheral_manager_ state] < CBPeripheralManagerStatePoweredOn) {
    return;
  }

  NSMutableArray* service_uuid_array = [[NSMutableArray alloc] init];
  for (const std::string& service_uuid :
       active_advertisement_->service_uuids()) {
    NSString* uuid_string =
        [NSString stringWithCString:service_uuid.c_str()
                           encoding:[NSString defaultCStringEncoding]];
    [service_uuid_array addObject:uuid_string];
  }

  active_advertisement_->OnAdvertisementPending();
  [peripheral_manager_ startAdvertising:@{
    CBAdvertisementDataServiceUUIDsKey : service_uuid_array
  }];
}

void BluetoothLowEnergyAdvertisementManagerMac::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    const BluetoothAdapter::CreateAdvertisementCallback& callback,
    const BluetoothAdapter::AdvertisementErrorCallback& error_callback) {
  std::unique_ptr<BluetoothAdvertisement::UUIDList> service_uuids =
      advertisement_data->service_uuids();
  if (!service_uuids || advertisement_data->manufacturer_data() ||
      advertisement_data->solicit_uuids() ||
      advertisement_data->service_data()) {
    LOG(ERROR) << "macOS only supports advertising service UUIDs.";
    error_callback.Run(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM);
    return;
  }

  if (active_advertisement_ && active_advertisement_->status() !=
                                   BluetoothAdvertisementMac::UNREGISTERED) {
    LOG(ERROR) << "Only one active BLE advertisement is currently supported.";
    error_callback.Run(
        BluetoothAdvertisement::ERROR_ADVERTISEMENT_ALREADY_EXISTS);
    return;
  }

  active_advertisement_ = new BluetoothAdvertisementMac(
      std::move(service_uuids), callback, error_callback, this);
  TryStartAdvertisement();
}

void BluetoothLowEnergyAdvertisementManagerMac::UnregisterAdvertisement(
    BluetoothAdvertisementMac* advertisement,
    const BluetoothAdvertisement::SuccessCallback& success_callback,
    const BluetoothAdvertisement::ErrorCallback& error_callback) {
  if (advertisement != active_advertisement_.get()) {
    LOG(ERROR) << "Only one active advertisement is supported currently.";
    error_callback.Run(BluetoothAdvertisement::ERROR_RESET_ADVERTISING);
    return;
  }

  active_advertisement_ = nullptr;
  [peripheral_manager_ stopAdvertising];
}

void BluetoothLowEnergyAdvertisementManagerMac::DidStartAdvertising(
    NSError* error) {
  if (!active_advertisement_ ||
      active_advertisement_->status() !=
          BluetoothAdvertisementMac::ADVERTISEMENT_PENDING) {
    return;
  }

  if (error != nil) {
    LOG(ERROR) << "Error advertising: "
               << base::SysNSStringToUTF8(error.localizedDescription);
    active_advertisement_->OnAdvertisementError(
        BluetoothAdvertisement::ERROR_STARTING_ADVERTISEMENT);
    return;
  }

  active_advertisement_->OnAdvertisementSuccess();
}

void BluetoothLowEnergyAdvertisementManagerMac::SetPeripheralManager(
    CBPeripheralManager* peripheral_manager) {
  peripheral_manager_ = peripheral_manager;
}

}  // device
