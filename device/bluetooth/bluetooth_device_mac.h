// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_MAC_H_

#include <string>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "device/bluetooth/bluetooth_device.h"

#ifdef __OBJC__
@class IOBluetoothDevice;
#else
class IOBluetoothDevice;
#endif

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace device {

class BluetoothDeviceMac : public BluetoothDevice {
 public:
  explicit BluetoothDeviceMac(
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
      IOBluetoothDevice* device);
  virtual ~BluetoothDeviceMac();

  // BluetoothDevice override
  virtual void AddObserver(
      device::BluetoothDevice::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      device::BluetoothDevice::Observer* observer) OVERRIDE;
  virtual uint32 GetBluetoothClass() const OVERRIDE;
  virtual std::string GetAddress() const OVERRIDE;
  virtual VendorIDSource GetVendorIDSource() const OVERRIDE;
  virtual uint16 GetVendorID() const OVERRIDE;
  virtual uint16 GetProductID() const OVERRIDE;
  virtual uint16 GetDeviceID() const OVERRIDE;
  virtual bool IsPaired() const OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectable() const OVERRIDE;
  virtual bool IsConnecting() const OVERRIDE;
  virtual UUIDList GetUUIDs() const OVERRIDE;
  virtual bool ExpectingPinCode() const OVERRIDE;
  virtual bool ExpectingPasskey() const OVERRIDE;
  virtual bool ExpectingConfirmation() const OVERRIDE;
  virtual void Connect(
      PairingDelegate* pairing_delegate,
      const base::Closure& callback,
      const ConnectErrorCallback& error_callback) OVERRIDE;
  virtual void SetPinCode(const std::string& pincode) OVERRIDE;
  virtual void SetPasskey(uint32 passkey) OVERRIDE;
  virtual void ConfirmPairing() OVERRIDE;
  virtual void RejectPairing() OVERRIDE;
  virtual void CancelPairing() OVERRIDE;
  virtual void Disconnect(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void Forget(const ErrorCallback& error_callback) OVERRIDE;
  virtual void ConnectToProfile(
      BluetoothProfile* profile,
      const base::Closure& callback,
      const ConnectToProfileErrorCallback& error_callback) OVERRIDE;
  virtual void SetOutOfBandPairingData(
      const BluetoothOutOfBandPairingData& data,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ClearOutOfBandPairingData(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;

 protected:
  // BluetoothDevice override
  virtual std::string GetDeviceName() const OVERRIDE;

 private:
  friend class BluetoothAdapterMac;

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  // (retained)
  IOBluetoothDevice* device_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_MAC_H_
