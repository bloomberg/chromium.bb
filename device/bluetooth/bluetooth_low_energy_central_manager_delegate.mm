// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_central_manager_delegate.h"

#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_low_energy_discovery_manager_mac.h"

namespace device {

// This class exists to bridge between the Objective-C CBCentralManagerDelegate
// class and our BluetoothLowEnergyDiscoveryManagerMac and BluetoothAdapterMac
// classes.
class BluetoothLowEnergyCentralManagerBridge {
 public:
  BluetoothLowEnergyCentralManagerBridge(
      BluetoothLowEnergyDiscoveryManagerMac* discovery_manager,
      BluetoothAdapterMac* adapter)
      : discovery_manager_(discovery_manager), adapter_(adapter) {}

  virtual ~BluetoothLowEnergyCentralManagerBridge() {}

  virtual void DiscoveredPeripheral(CBPeripheral* peripheral,
                                    NSDictionary* advertisementData,
                                    int rssi) {
    discovery_manager_->DiscoveredPeripheral(peripheral, advertisementData,
                                             rssi);
  }

  virtual void UpdatedState() {
    discovery_manager_->TryStartDiscovery();
    adapter_->LowEnergyCentralManagerUpdatedState();
  }

 private:
  BluetoothLowEnergyDiscoveryManagerMac* discovery_manager_;
  BluetoothAdapterMac* adapter_;
};

}  // namespace device

@implementation BluetoothLowEnergyCentralManagerDelegate

- (id)initWithDiscoveryManager:
          (device::BluetoothLowEnergyDiscoveryManagerMac*)discovery_manager
                    andAdapter:(device::BluetoothAdapterMac*)adapter {
  if ((self = [super init])) {
    bridge_.reset(new device::BluetoothLowEnergyCentralManagerBridge(
        discovery_manager, adapter));
  }
  return self;
}

- (void)centralManager:(CBCentralManager*)central
 didDiscoverPeripheral:(CBPeripheral*)peripheral
     advertisementData:(NSDictionary*)advertisementData
                  RSSI:(NSNumber*)RSSI {
  // Notifies the discovery of a device.
  bridge_->DiscoveredPeripheral(peripheral, advertisementData, [RSSI intValue]);
}

- (void)centralManagerDidUpdateState:(CBCentralManager*)central {
  // Notifies when the powered state of the central manager changed.
  bridge_->UpdatedState();
}

@end
