// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_discovery_manager_mac.h"

#include "base/mac/mac_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
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
  ClearDevices();
}

bool BluetoothLowEnergyDiscoveryManagerMac::IsDiscovering() const {
  return discovering_;
}

void BluetoothLowEnergyDiscoveryManagerMac::StartDiscovery(
    BluetoothDevice::UUIDList services_uuids) {
  ClearDevices();
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
  // Look for existing device.
  auto iter = devices_.find(
      BluetoothLowEnergyDeviceMac::GetPeripheralIdentifier(peripheral));
  if (iter == devices_.end()) {
    // A device has been added.
    BluetoothLowEnergyDeviceMac* device =
        new BluetoothLowEnergyDeviceMac(peripheral, advertisementData, rssi);
    devices_.insert(devices_.begin(),
                    std::make_pair(device->GetIdentifier(), device));
    observer_->DeviceFound(device);
    return;
  }

  // A device has an update.
  BluetoothLowEnergyDeviceMac* old_device = iter->second;
  old_device->Update(peripheral, advertisementData, rssi);
  observer_->DeviceUpdated(old_device);
}

BluetoothLowEnergyDiscoveryManagerMac*
BluetoothLowEnergyDiscoveryManagerMac::Create(Observer* observer) {
  return new BluetoothLowEnergyDiscoveryManagerMac(observer);
}

BluetoothLowEnergyDiscoveryManagerMac::BluetoothLowEnergyDiscoveryManagerMac(
    Observer* observer)
    : observer_(observer) {
  bridge_.reset([[BluetoothLowEnergyDiscoveryManagerMacBridge alloc]
      initWithManager:this]);
  // Since CoreBluetooth is only available on OS X 10.7 or later, we
  // instantiate CBCentralManager only for OS X >= 10.7.
  if (base::mac::IsOSLionOrLater()) {
    Class aClass = NSClassFromString(@"CBCentralManager");
    manager_.reset(
        [[aClass alloc] initWithDelegate:bridge_
                                   queue:dispatch_get_main_queue()]);
  }
  discovering_ = false;
}

void BluetoothLowEnergyDiscoveryManagerMac::ClearDevices() {
  STLDeleteValues(&devices_);
}
