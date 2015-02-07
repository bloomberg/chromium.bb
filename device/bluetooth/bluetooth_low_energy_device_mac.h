// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEVICE_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEVICE_MAC_H_

#if defined(OS_IOS)
#import <CoreBluetooth/CoreBluetooth.h>
#else
#import <IOBluetooth/IOBluetooth.h>
#endif

#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

class BluetoothLowEnergyDiscoverManagerMac;

class BluetoothLowEnergyDeviceMac : public BluetoothDevice {
 public:
  BluetoothLowEnergyDeviceMac(CBPeripheral* peripheral,
                              NSDictionary* advertisementData,
                              int rssi);
  ~BluetoothLowEnergyDeviceMac() override;

  int GetRSSI() const;

  // BluetoothDevice overrides.
  std::string GetIdentifier() const override;
  uint32 GetBluetoothClass() const override;
  std::string GetAddress() const override;
  BluetoothDevice::VendorIDSource GetVendorIDSource() const override;
  uint16 GetVendorID() const override;
  uint16 GetProductID() const override;
  uint16 GetDeviceID() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  BluetoothDevice::UUIDList GetUUIDs() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(const ConnectionInfoCallback& callback) override;
  void Connect(PairingDelegate* pairing_delegate,
               const base::Closure& callback,
               const ConnectErrorCallback& error_callback) override;
  void SetPinCode(const std::string& pincode) override;
  void SetPasskey(uint32 passkey) override;
  void ConfirmPairing() override;
  void RejectPairing() override;
  void CancelPairing() override;
  void Disconnect(const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  void Forget(const ErrorCallback& error_callback) override;
  void ConnectToService(
      const BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;
  void ConnectToServiceInsecurely(
      const device::BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;
  void CreateGattConnection(
      const GattConnectionCallback& callback,
      const ConnectErrorCallback& error_callback) override;

 protected:
  // BluetoothDevice override.
  std::string GetDeviceName() const override;

  // Updates information about the device.
  virtual void Update(CBPeripheral* peripheral,
                      NSDictionary* advertisementData,
                      int rssi);

  static std::string GetPeripheralIdentifier(CBPeripheral* peripheral);

 private:
  friend class BluetoothLowEnergyDiscoveryManagerMac;

  // CoreBluetooth data structure.
  base::scoped_nsobject<CBPeripheral> peripheral_;

  // RSSI value.
  int rssi_;

  // Whether the device is connectable.
  bool connectable_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyDeviceMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEVICE_MAC_H_
