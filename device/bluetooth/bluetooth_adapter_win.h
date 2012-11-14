// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

class BluetoothAdapterFactory;
class BluetoothAdapterWinTest;
class BluetoothDevice;

class BluetoothAdapterWin : public BluetoothAdapter {
 public:
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

 protected:
  BluetoothAdapterWin();
  virtual ~BluetoothAdapterWin();

  virtual void UpdateAdapterState();

 private:
  friend class BluetoothAdapterFactory;
  friend class BluetoothAdapterWinTest;

  // Obtains the default adapter info (the first bluetooth radio info found on
  // the system) and tracks future changes to it.
  void TrackDefaultAdapter();

  void PollAdapterState();

  static const int kPollIntervalMs;

  // NOTE: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterWin> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_
