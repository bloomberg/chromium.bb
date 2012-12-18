// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WIN_H_

#include <string>

#include "base/basictypes.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

class BluetoothDeviceWin : public BluetoothDevice {
 public:
  BluetoothDeviceWin();
  virtual ~BluetoothDeviceWin();

  virtual bool IsPaired() const OVERRIDE;
  virtual const ServiceList& GetServices() const OVERRIDE;
  virtual void GetServiceRecords(
      const ServiceRecordsCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool ProvidesServiceWithUUID(const std::string& uuid) const OVERRIDE;
  virtual void ProvidesServiceWithName(
      const std::string& name,
      const ProvidesServiceCallback& callback) OVERRIDE;
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
  virtual void ConnectToService(
      const std::string& service_uuid,
      const SocketCallback& callback) OVERRIDE;
  virtual void SetOutOfBandPairingData(
      const BluetoothOutOfBandPairingData& data,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ClearOutOfBandPairingData(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;

 private:
  // The services (identified by UUIDs) that this device provides.
  std::vector<std::string> service_uuids_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WIN_H_
