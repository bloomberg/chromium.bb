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
  virtual ~BluetoothDeviceMac();

  // BluetoothDevice override
  virtual uint32 GetBluetoothClass() const override;
  virtual std::string GetAddress() const override;
  virtual VendorIDSource GetVendorIDSource() const override;
  virtual uint16 GetVendorID() const override;
  virtual uint16 GetProductID() const override;
  virtual uint16 GetDeviceID() const override;
  virtual int GetRSSI() const override;
  virtual int GetCurrentHostTransmitPower() const override;
  virtual int GetMaximumHostTransmitPower() const override;
  virtual bool IsPaired() const override;
  virtual bool IsConnected() const override;
  virtual bool IsConnectable() const override;
  virtual bool IsConnecting() const override;
  virtual UUIDList GetUUIDs() const override;
  virtual bool ExpectingPinCode() const override;
  virtual bool ExpectingPasskey() const override;
  virtual bool ExpectingConfirmation() const override;
  virtual void Connect(
      PairingDelegate* pairing_delegate,
      const base::Closure& callback,
      const ConnectErrorCallback& error_callback) override;
  virtual void SetPinCode(const std::string& pincode) override;
  virtual void SetPasskey(uint32 passkey) override;
  virtual void ConfirmPairing() override;
  virtual void RejectPairing() override;
  virtual void CancelPairing() override;
  virtual void Disconnect(
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;
  virtual void Forget(const ErrorCallback& error_callback) override;
  virtual void ConnectToService(
      const BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;
  virtual void CreateGattConnection(
      const GattConnectionCallback& callback,
      const ConnectErrorCallback& error_callback) override;
  virtual void StartConnectionMonitor(
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;

  // Returns the timestamp when the device was last seen during an inquiry.
  // Returns nil if the device has never been seen during an inquiry.
  NSDate* GetLastInquiryUpdate();

  // Returns the Bluetooth address for the |device|. The returned address has a
  // normalized format (see below).
  static std::string GetDeviceAddress(IOBluetoothDevice* device);

 protected:
  // BluetoothDevice override
  virtual std::string GetDeviceName() const override;

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
