// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WIN_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"

namespace device {

class BluetoothAdapterWin;
class BluetoothServiceRecord;

class BluetoothDeviceWin : public BluetoothDevice {
 public:
  explicit BluetoothDeviceWin(
      const BluetoothTaskManagerWin::DeviceState& state);
  virtual ~BluetoothDeviceWin();

  // BluetoothDevice override
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
  virtual void ConnectToService(
      const std::string& service_uuid,
      const SocketCallback& callback) OVERRIDE;
  virtual void ConnectToProfile(
      device::BluetoothProfile* profile,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void SetOutOfBandPairingData(
      const BluetoothOutOfBandPairingData& data,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ClearOutOfBandPairingData(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;

  // Used by BluetoothProfileWin to retrieve the service record for the given
  // |uuid|.
  const BluetoothServiceRecord* GetServiceRecord(
      const std::string& uuid) const;

 protected:
  // BluetoothDevice override
  virtual std::string GetDeviceName() const OVERRIDE;

 private:
  friend class BluetoothAdapterWin;

  // Used by BluetoothAdapterWin to update the visible state during
  // discovery.
  void SetVisible(bool visible);

  // The Bluetooth class of the device, a bitmask that may be decoded using
  // https://www.bluetooth.org/Technical/AssignedNumbers/baseband.htm
  uint32 bluetooth_class_;

  // The name of the device, as supplied by the remote device.
  std::string name_;

  // The Bluetooth address of the device.
  std::string address_;

  // Tracked device state, updated by the adapter managing the lifecycle of
  // the device.
  bool paired_;
  bool connected_;

  // Used to send change notifications when a device disappears during
  // discovery.
  bool visible_;

  // The services (identified by UUIDs) that this device provides.
  UUIDList uuids_;

  // The service records retrieved from SDP.
  typedef ScopedVector<BluetoothServiceRecord> ServiceRecordList;
  ServiceRecordList service_record_list_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_WIN_H_
