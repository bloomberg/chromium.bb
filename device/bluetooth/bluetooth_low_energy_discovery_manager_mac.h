// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DISCOVERY_MANAGER_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DISCOVERY_MANAGER_MAC_H_

#if defined(OS_IOS)
#import <CoreBluetooth/CoreBluetooth.h>
#else
#import <IOBluetooth/IOBluetooth.h>
#endif

#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "device/bluetooth/bluetooth_device.h"

@class BluetoothLowEnergyDiscoveryManagerMacBridge;

namespace device {

class BluetoothLowEnergyDeviceMac;
class BluetoothLowEnergyDiscoveryManagerMacDelegate;

// This class will scan for Bluetooth LE device on Mac.
class BluetoothLowEnergyDiscoveryManagerMac {
 public:
  // Interface for being notified of events during a device discovery session.
  class Observer {
   public:
    // Called when |this| manager has found a device.
    virtual void DeviceFound(BluetoothLowEnergyDeviceMac* device) = 0;

    // Called when |this| manager has updated on a device.
    virtual void DeviceUpdated(BluetoothLowEnergyDeviceMac* device) = 0;

   protected:
    virtual ~Observer() {}
  };

  virtual ~BluetoothLowEnergyDiscoveryManagerMac();

  // Returns true, if discovery is currently being performed.
  virtual bool IsDiscovering() const;

  // Initiates a discovery session.
  // BluetoothLowEnergyDeviceMac objects discovered within a previous
  // discovery session will be invalid.
  virtual void StartDiscovery(BluetoothDevice::UUIDList services_uuids);

  // Stops a discovery session.
  virtual void StopDiscovery();

  // Returns a new BluetoothLowEnergyDiscoveryManagerMac.
  static BluetoothLowEnergyDiscoveryManagerMac* Create(Observer* observer);

 protected:
  // Called when a discovery or an update of a BLE device occurred.
  virtual void DiscoveredPeripheral(CBPeripheral* peripheral,
                                    NSDictionary* advertisementData,
                                    int rssi);

  // The device discovery can really be started when Bluetooth is powered on.
  // The method TryStartDiscovery() is called when it's a good time to try to
  // start the BLE device discovery. It will check if the discovery session has
  // been started and if the Bluetooth is powered and then really start the
  // CoreBluetooth BLE device discovery.
  virtual void TryStartDiscovery();

 private:
  explicit BluetoothLowEnergyDiscoveryManagerMac(Observer* observer);
  void ClearDevices();

  friend class BluetoothLowEnergyDiscoveryManagerMacDelegate;

  // Observer interested in notifications from us.
  Observer* observer_;

  // Underlaying CoreBluetooth central manager.
  base::scoped_nsobject<CBCentralManager> manager_;

  // Discovery has been initiated by calling the API StartDiscovery().
  bool discovering_;

  // A discovery has been initiated but has not started yet because it's
  // waiting for Bluetooth to turn on.
  bool pending_;

  // Delegate of the central manager.
  base::scoped_nsobject<BluetoothLowEnergyDiscoveryManagerMacBridge> bridge_;

  // Map of the device identifiers to the discovered device.
  std::map<const std::string, BluetoothLowEnergyDeviceMac*> devices_;

  // List of service UUIDs to scan.
  BluetoothDevice::UUIDList services_uuids_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyDiscoveryManagerMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DISCOVERY_MANAGER_MAC_H_
