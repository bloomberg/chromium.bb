// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_MAC_H_

#include <memory>

#include "base/macros.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

class BluetoothLowEnergyAdvertisementManagerMac;

// Simple implementation of BluetoothAdvertisement for Mac. The primary logic is
// currently handled in BluetoothLowEnergyAdvertisementManagerMac.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdvertisementMac
    : public BluetoothAdvertisement {
 public:
  enum Status {
    WAITING_FOR_PERIPHERAL_MANAGER,
    ADVERTISEMENT_PENDING,
    ADVERTISING,
    ERROR_ADVERTISING,
    UNREGISTERED,
  };

  BluetoothAdvertisementMac(
      std::unique_ptr<BluetoothAdvertisement::UUIDList> service_uuids,
      const BluetoothAdapter::CreateAdvertisementCallback& callback,
      const BluetoothAdapter::AdvertisementErrorCallback& error_callback,
      BluetoothLowEnergyAdvertisementManagerMac* advertisement_manager);

  // BluetoothAdvertisement overrides:
  void Unregister(const SuccessCallback& success_callback,
                  const ErrorCallback& error_callback) override;

  Status status() const { return status_; }

  BluetoothAdvertisement::UUIDList service_uuids() { return *service_uuids_; }

 private:
  friend class BluetoothLowEnergyAdvertisementManagerMac;

  ~BluetoothAdvertisementMac() override;

  // Called by BluetoothLowEnergyAdvertisementManagerMac.
  void OnAdvertisementPending();
  void OnAdvertisementError(BluetoothAdvertisement::ErrorCode error_code);
  void OnAdvertisementSuccess();

  BluetoothAdapter::CreateAdvertisementCallback success_callback() {
    return success_callback_;
  }

  std::unique_ptr<BluetoothAdvertisement::UUIDList> service_uuids_;
  BluetoothAdapter::CreateAdvertisementCallback success_callback_;
  BluetoothAdapter::AdvertisementErrorCallback error_callback_;
  BluetoothLowEnergyAdvertisementManagerMac* advertisement_manager_;
  Status status_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdvertisementMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_MAC_H_
