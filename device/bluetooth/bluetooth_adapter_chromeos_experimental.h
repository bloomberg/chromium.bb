// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_EXPERIMENTAL_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_EXPERIMENTAL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

class BluetoothAdapterFactory;

}  // namespace device

namespace chromeos {

// The BluetoothAdapterChromeOSExperimental class is an alternate implementation
// of BluetoothAdapter for the Chrome OS platform using the Bluetooth Smart
// capable backend. It will become the sole implementation for Chrome OS, and
// be renamed to BluetoothAdapterChromeOS, once the backend is switched,
class BluetoothAdapterChromeOSExperimental
    : public device::BluetoothAdapter {
 public:
  // BluetoothAdapter override
  virtual void AddObserver(
      device::BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      device::BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual std::string address() const OVERRIDE;
  virtual std::string name() const OVERRIDE;
  virtual bool IsInitialized() const OVERRIDE;
  virtual bool IsPresent() const OVERRIDE;
  virtual bool IsPowered() const OVERRIDE;
  virtual void SetPowered(
      bool powered,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsDiscovering() const OVERRIDE;
  virtual void StartDiscovering(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void StopDiscovering(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ReadLocalOutOfBandPairingData(
      const device::BluetoothAdapter::BluetoothOutOfBandPairingDataCallback&
          callback,
      const ErrorCallback& error_callback) OVERRIDE;

 private:
  friend class device::BluetoothAdapterFactory;

  BluetoothAdapterChromeOSExperimental();
  virtual ~BluetoothAdapterChromeOSExperimental();

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterChromeOSExperimental> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterChromeOSExperimental);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_EXPERIMENTAL_H_
