// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_ADVERTISEMENT_MANAGER_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_ADVERTISEMENT_MANAGER_MAC_H_

#include "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_advertisement_mac.h"

namespace device {

// Class used by BluetoothAdapterMac to manage LE advertisements.
// Currently, only a single concurrent BLE advertisement is supported. Future
// work will add support for multiple concurrent and rotating advertisements.
class DEVICE_BLUETOOTH_EXPORT BluetoothLowEnergyAdvertisementManagerMac {
 public:
  BluetoothLowEnergyAdvertisementManagerMac();
  ~BluetoothLowEnergyAdvertisementManagerMac();

  // Registers a new BLE advertisement.
  void RegisterAdvertisement(
      std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
      const BluetoothAdapter::CreateAdvertisementCallback& callback,
      const BluetoothAdapter::AdvertisementErrorCallback& error_callback);

  // Unregisters an existing BLE advertisement.
  void UnregisterAdvertisement(
      BluetoothAdvertisementMac* advertisement,
      const BluetoothAdvertisementMac::SuccessCallback& success_callback,
      const BluetoothAdvertisementMac::ErrorCallback& error_callback);

  // Tries to start the active advertisement if the adapter is ready.
  void TryStartAdvertisement();

  // Called when advertisement starts with success or failure.
  void DidStartAdvertising(NSError* error);

  // Sets the CBPeripheralManager instance.
  void SetPeripheralManager(CBPeripheralManager* peripheral_manager);

 private:
  CBPeripheralManager* peripheral_manager_;

  scoped_refptr<BluetoothAdvertisementMac> active_advertisement_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyAdvertisementManagerMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_ADVERTISEMENT_MANAGER_MAC_H_
