// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_MAC_H_

#import <IOBluetooth/IOBluetooth.h>

#include <string>

#include "base/basictypes.h"
#include "base/mac/scoped_nsobject.h"
#include "base/observer_list.h"
#include "device/bluetooth/bluetooth_device.h"

@class IOBluetoothDevice;

namespace device {

class BluetoothDeviceMac : public BluetoothDevice {
 public:
  explicit BluetoothDeviceMac(IOBluetoothDevice* device);
  ~BluetoothDeviceMac() override;

  // BluetoothDevice override
  uint32 GetBluetoothClass() const override;
  std::string GetAddress() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16 GetVendorID() const override;
  uint16 GetProductID() const override;
  uint16 GetDeviceID() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  UUIDList GetUUIDs() const override;
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
      const BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;
  void CreateGattConnection(
      const GattConnectionCallback& callback,
      const ConnectErrorCallback& error_callback) override;

  // Returns the timestamp when the device was last seen during an inquiry.
  // Returns nil if the device has never been seen during an inquiry.
  NSDate* GetLastInquiryUpdate();

  // Returns the Bluetooth address for the |device|. The returned address has a
  // normalized format (see below).
  static std::string GetDeviceAddress(IOBluetoothDevice* device);

 protected:
  // BluetoothDevice override
  std::string GetDeviceName() const override;

 private:
  friend class BluetoothAdapterMac;

  // Implementation to read the host's transmit power level of type
  // |power_level_type|.
  int GetHostTransmitPower(
      BluetoothHCITransmitPowerLevelType power_level_type) const;

  base::scoped_nsobject<IOBluetoothDevice> device_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_MAC_H_
