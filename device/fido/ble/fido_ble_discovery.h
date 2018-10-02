// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_BLE_FIDO_BLE_DISCOVERY_H_
#define DEVICE_FIDO_BLE_FIDO_BLE_DISCOVERY_H_

#include <memory>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/ble/fido_ble_discovery_base.h"

namespace device {

class BluetoothDevice;
class BluetoothUUID;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoBleDiscovery
    : public FidoBleDiscoveryBase {
 public:
  FidoBleDiscovery();
  ~FidoBleDiscovery() override;

 private:
  static const BluetoothUUID& FidoServiceUUID();

  // FidoBleDiscoveryBase:
  void OnSetPowered() override;

  // BluetoothAdapter::Observer:
  void DeviceAdded(BluetoothAdapter* adapter, BluetoothDevice* device) override;
  void DeviceChanged(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void DeviceRemoved(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void AdapterPoweredChanged(BluetoothAdapter* adapter, bool powered) override;
  void DeviceAddressChanged(BluetoothAdapter* adapter,
                            BluetoothDevice* device,
                            const std::string& old_address) override;

  // Returns true if |device| is a Cable device. If so, add address of |device|
  // to |blacklisted_cable_device_addresses_|.
  bool CheckForExcludedDeviceAndCacheAddress(const BluetoothDevice* device);

  std::set<std::string> excluded_cable_device_addresses_;
  base::WeakPtrFactory<FidoBleDiscovery> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FidoBleDiscovery);
};

}  // namespace device

#endif  // DEVICE_FIDO_BLE_FIDO_BLE_DISCOVERY_H_
