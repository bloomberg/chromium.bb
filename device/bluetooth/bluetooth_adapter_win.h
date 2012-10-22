// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_

#include <string>

#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

class BluetoothDevice;

class BluetoothAdapterWin : public BluetoothAdapter {
  // BluetoothAdapter override
  virtual void AddObserver(BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual bool IsPresent() const OVERRIDE;
  virtual bool IsPowered() const OVERRIDE;
  virtual void SetPowered(
      bool powered,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsDiscovering() const OVERRIDE;
  virtual void SetDiscovering(
      bool discovering,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual ConstDeviceList GetDevices() const OVERRIDE;
  virtual BluetoothDevice* GetDevice(const std::string& address) OVERRIDE;
  virtual const BluetoothDevice* GetDevice(
      const std::string& address) const OVERRIDE;
  virtual void ReadLocalOutOfBandPairingData(
      const BluetoothOutOfBandPairingDataCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;

 private:
  BluetoothAdapterWin();
  virtual ~BluetoothAdapterWin();
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_
