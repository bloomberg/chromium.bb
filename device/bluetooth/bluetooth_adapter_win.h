// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"

namespace device {

class BluetoothAdapterFactory;
class BluetoothAdapterWinTest;
class BluetoothDevice;

class BluetoothAdapterWin : public BluetoothAdapter,
                            public BluetoothTaskManagerWin::Observer {
 public:
  // BluetoothAdapter override
  virtual void AddObserver(BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual bool IsInitialized() const OVERRIDE;
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

  // BluetoothTaskManagerWin::Observer override
  virtual void AdapterStateChanged(
      const BluetoothTaskManagerWin::AdapterState& state) OVERRIDE;

 protected:
  friend class BluetoothAdapterWinTest;

  BluetoothAdapterWin();
  virtual ~BluetoothAdapterWin();

 private:
  friend class BluetoothAdapterFactory;

  void TrackDefaultAdapter();

  bool powered_;

  scoped_refptr<BluetoothTaskManagerWin> task_manager_;

  base::ThreadChecker thread_checker_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothAdapter::Observer> observers_;

  // NOTE: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterWin> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_
