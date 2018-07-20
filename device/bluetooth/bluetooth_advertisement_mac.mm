// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_advertisement_mac.h"

#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothHostController.h>

#include "device/bluetooth/bluetooth_adapter_mac.h"

namespace device {

BluetoothAdvertisementMac::BluetoothAdvertisementMac(
    std::unique_ptr<BluetoothAdvertisement::UUIDList> service_uuids,
    const BluetoothAdapter::CreateAdvertisementCallback& success_callback,
    const BluetoothAdapter::AdvertisementErrorCallback& error_callback,
    BluetoothLowEnergyAdvertisementManagerMac* advertisement_manager)
    : service_uuids_(std::move(service_uuids)),
      success_callback_(success_callback),
      error_callback_(error_callback),
      advertisement_manager_(advertisement_manager),
      status_(BluetoothAdvertisementMac::WAITING_FOR_PERIPHERAL_MANAGER) {}

void BluetoothAdvertisementMac::Unregister(
    const SuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  if (status_ == Status::UNREGISTERED) {
    success_callback.Run();
    return;
  }

  status_ = Status::UNREGISTERED;
  advertisement_manager_->UnregisterAdvertisement(this, success_callback,
                                                  error_callback);
}

BluetoothAdvertisementMac::~BluetoothAdvertisementMac() {
  // This object should be owned by BluetoothLowEnergyAdvertisementManagerMac,
  // and will be cleaned up there.
}

void BluetoothAdvertisementMac::OnAdvertisementPending() {
  status_ = Status::ADVERTISEMENT_PENDING;
}

void BluetoothAdvertisementMac::OnAdvertisementError(
    BluetoothAdvertisement::ErrorCode error_code) {
  status_ = Status::ERROR_ADVERTISING;
  error_callback_.Run(error_code);
}

void BluetoothAdvertisementMac::OnAdvertisementSuccess() {
  status_ = Status::ADVERTISING;
  success_callback_.Run(this);
}

}  // namespace device
