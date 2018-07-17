// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_peripheral_manager_delegate.h"

#include "device/bluetooth/bluetooth_adapter_mac.h"

namespace device {

// This class exists to bridge between the Objective-C
// CBPeripheralManagerDelegate class and our BluetoothAdapterMac classes.
class BluetoothLowEnergyPeripheralManagerBridge {
 public:
  BluetoothLowEnergyPeripheralManagerBridge(BluetoothAdapterMac* adapter)
      : adapter_(adapter) {}

  ~BluetoothLowEnergyPeripheralManagerBridge() {}

  void UpdatedState() {
    // TODO(tengs): Hook this up to BluetoothAdapterMac.
  }

  void DidStartAdvertising(NSError* error) {
    // TODO(tengs): Hook this up to BluetoothAdapterMac.
  }

  CBPeripheralManager* GetPeripheralManager() {
    return adapter_->GetPeripheralManager();
  }

 private:
  BluetoothAdapterMac* adapter_;
};

}  // namespace device

@implementation BluetoothLowEnergyPeripheralManagerDelegate

- (id)initWithAdapter:(device::BluetoothAdapterMac*)adapter {
  if ((self = [super init])) {
    bridge_.reset(
        new device::BluetoothLowEnergyPeripheralManagerBridge(adapter));
  }
  return self;
}

- (void)dealloc {
  [bridge_->GetPeripheralManager() setDelegate:nil];
  [super dealloc];
}

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager*)peripheral {
  bridge_->UpdatedState();
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager*)peripheral
                                       error:(NSError*)error {
  bridge_->DidStartAdvertising(error);
}

@end
