// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_discovery_manager_mac.h"

#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_low_energy_device_mac.h"

using device::BluetoothLowEnergyDeviceMac;
using device::BluetoothLowEnergyDiscoveryManagerMac;
using device::BluetoothLowEnergyDiscoveryManagerMacDelegate;

namespace device {

// This class is a helper to call some protected methods in
// BluetoothLowEnergyDiscoveryManagerMac.
class BluetoothLowEnergyDiscoveryManagerMacDelegate {
 public:
  BluetoothLowEnergyDiscoveryManagerMacDelegate(
      BluetoothLowEnergyDiscoveryManagerMac* manager)
      : manager_(manager) {}

  virtual ~BluetoothLowEnergyDiscoveryManagerMacDelegate() {}

  virtual void DiscoveredPeripheral(CBPeripheral* peripheral,
                                    NSDictionary* advertisementData,
                                    int rssi) {
    manager_->DiscoveredPeripheral(peripheral, advertisementData, rssi);
  }

  virtual void TryStartDiscovery() { manager_->TryStartDiscovery(); }

 private:
  BluetoothLowEnergyDiscoveryManagerMac* manager_;
};

}  // namespace device

// This class will serve as the Objective-C delegate of CBCentralManager.
@interface BluetoothLowEnergyDiscoveryManagerMacBridge
    : NSObject<CBCentralManagerDelegate> {
  BluetoothLowEnergyDiscoveryManagerMac* manager_;
  scoped_ptr<BluetoothLowEnergyDiscoveryManagerMacDelegate> delegate_;
}

- (id)initWithManager:(BluetoothLowEnergyDiscoveryManagerMac*)manager;

@end

@implementation BluetoothLowEnergyDiscoveryManagerMacBridge

- (id)initWithManager:(BluetoothLowEnergyDiscoveryManagerMac*)manager {
  if ((self = [super init])) {
    manager_ = manager;
    delegate_.reset(
        new BluetoothLowEnergyDiscoveryManagerMacDelegate(manager_));
  }
  return self;
}

- (void)centralManager:(CBCentralManager*)central
    didDiscoverPeripheral:(CBPeripheral*)peripheral
        advertisementData:(NSDictionary*)advertisementData
                     RSSI:(NSNumber*)RSSI {
  // Notifies the discovery of a device.
  delegate_->DiscoveredPeripheral(peripheral, advertisementData,
                                  [RSSI intValue]);
}

- (void)centralManagerDidUpdateState:(CBCentralManager*)central {
  // Notifies when the powered state of the central manager changed.
  delegate_->TryStartDiscovery();
}

@end

BluetoothLowEnergyDiscoveryManagerMac::
    ~BluetoothLowEnergyDiscoveryManagerMac() {
}

bool BluetoothLowEnergyDiscoveryManagerMac::IsDiscovering() const {
  return discovering_;
}

void BluetoothLowEnergyDiscoveryManagerMac::StartDiscovery(
    BluetoothDevice::UUIDList services_uuids) {
  discovering_ = true;
  pending_ = true;
  services_uuids_ = services_uuids;
  TryStartDiscovery();
}

void BluetoothLowEnergyDiscoveryManagerMac::TryStartDiscovery() {
  if (!discovering_) {
    return;
  }

  if (!pending_) {
    return;
  }

  // Can only start if the bluetooth power is turned on.
  if ([manager_ state] != CBCentralManagerStatePoweredOn) {
    return;
  }

  // Converts the services UUIDs to a CoreBluetooth data structure.
  NSMutableArray* services = nil;
  if (!services_uuids_.empty()) {
    services = [NSMutableArray array];
    for (auto& service_uuid : services_uuids_) {
      NSString* uuidString =
          base::SysUTF8ToNSString(service_uuid.canonical_value().c_str());
      Class aClass = NSClassFromString(@"CBUUID");
      CBUUID* uuid = [aClass UUIDWithString:uuidString];
      [services addObject:uuid];
    }
  };

  [manager_ scanForPeripheralsWithServices:services options:nil];
  pending_ = false;
}

void BluetoothLowEnergyDiscoveryManagerMac::StopDiscovery() {
  if (discovering_ && !pending_) {
    [manager_ stopScan];
  }
  discovering_ = false;
}

void BluetoothLowEnergyDiscoveryManagerMac::DiscoveredPeripheral(
    CBPeripheral* peripheral,
    NSDictionary* advertisementData,
    int rssi) {
  observer_->LowEnergyDeviceUpdated(peripheral, advertisementData, rssi);
}

BluetoothLowEnergyDiscoveryManagerMac*
BluetoothLowEnergyDiscoveryManagerMac::Create(Observer* observer) {
  return new BluetoothLowEnergyDiscoveryManagerMac(observer);
}

BluetoothLowEnergyDiscoveryManagerMac::BluetoothLowEnergyDiscoveryManagerMac(
    Observer* observer)
    : observer_(observer) {
  DCHECK(BluetoothAdapterMac::IsLowEnergyAvailable());
  bridge_.reset([[BluetoothLowEnergyDiscoveryManagerMacBridge alloc]
      initWithManager:this]);
  Class aClass = NSClassFromString(@"CBCentralManager");
  manager_.reset([[aClass alloc] initWithDelegate:bridge_
                                            queue:dispatch_get_main_queue()]);
  discovering_ = false;
}

void BluetoothLowEnergyDiscoveryManagerMac::SetManagerForTesting(
    CBCentralManager* manager) {
  DCHECK(BluetoothAdapterMac::IsLowEnergyAvailable());
  [manager performSelector:@selector(setDelegate:) withObject:bridge_];
  manager_.reset(manager);
}
