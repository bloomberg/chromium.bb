// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_peripheral_delegate.h"

#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_low_energy_discovery_manager_mac.h"

namespace device {

// This class exists to bridge between the Objective-C CBPeripheralDelegate
// class and our BluetoothLowEnergyDiscoveryManagerMac and BluetoothAdapterMac
// classes.
class BluetoothLowEnergyPeripheralBridge {
 public:
  BluetoothLowEnergyPeripheralBridge(BluetoothLowEnergyDeviceMac* device_mac)
      : device_mac_(device_mac) {}

  ~BluetoothLowEnergyPeripheralBridge() {}

  void DidModifyServices(NSArray* invalidatedServices) {
    device_mac_->DidModifyServices(invalidatedServices);
  }

  void DidDiscoverPrimaryServices(NSError* error) {
    device_mac_->DidDiscoverPrimaryServices(error);
  };

  CBPeripheral* GetPeripheral() { return device_mac_->GetPeripheral(); }

 private:
  BluetoothLowEnergyDeviceMac* device_mac_;
};

}  // namespace device

@implementation BluetoothLowEnergyPeripheralDelegate

- (id)initWithBluetoothLowEnergyDeviceMac:
    (device::BluetoothLowEnergyDeviceMac*)device_mac {
  if ((self = [super init])) {
    bridge_.reset(new device::BluetoothLowEnergyPeripheralBridge(device_mac));
  }
  return self;
}

- (void)dealloc {
  [bridge_->GetPeripheral() setDelegate:nil];
  [super dealloc];
}

- (void)peripheral:(CBPeripheral*)peripheral
    didModifyServices:(NSArray*)invalidatedServices {
  bridge_->DidModifyServices(invalidatedServices);
}

- (void)peripheral:(CBPeripheral*)peripheral
    didDiscoverServices:(NSError*)error {
  bridge_->DidDiscoverPrimaryServices(error);
}

@end
