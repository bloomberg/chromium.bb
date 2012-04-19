// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

#if defined(OS_CHROMEOS)
namespace chromeos {

class BluetoothAdapter;
class BluetoothDevice;

}  // namespace chromeos
#endif

namespace extensions {
namespace api {

class BluetoothExtensionFunction : public SyncExtensionFunction {
 protected:
#if defined(OS_CHROMEOS)
  const chromeos::BluetoothAdapter* adapter() const;
  chromeos::BluetoothAdapter* GetMutableAdapter();
#endif
};

class AsyncBluetoothExtensionFunction : public AsyncExtensionFunction {
 protected:
#if defined(OS_CHROMEOS)
  const chromeos::BluetoothAdapter* adapter() const;
  chromeos::BluetoothAdapter* GetMutableAdapter();
#endif
};

class BluetoothIsAvailableFunction : public BluetoothExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.isAvailable")
};

class BluetoothIsPoweredFunction : public BluetoothExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.isPowered")
};

class BluetoothGetAddressFunction : public BluetoothExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.getAddress")
};

class BluetoothGetDevicesWithServiceUUIDFunction
    : public BluetoothExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.getDevicesWithServiceUUID")
};

class BluetoothGetDevicesWithServiceNameFunction
    : public AsyncBluetoothExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.getDevicesWithServiceName")

#if defined(OS_CHROMEOS)
  BluetoothGetDevicesWithServiceNameFunction();

 private:
  void AddDeviceIfTrue(
      ListValue* list, const chromeos::BluetoothDevice* device, bool result);

  int callbacks_pending_;
#endif
};

class BluetoothDisconnectFunction : public BluetoothExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.disconnect")
};

class BluetoothReadFunction : public BluetoothExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.read")
};

class BluetoothSetOutOfBandPairingDataFunction
    : public BluetoothExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.setOutOfBandPairingData")
};

class BluetoothGetOutOfBandPairingDataFunction
    : public BluetoothExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.getOutOfBandPairingData")
};

class BluetoothWriteFunction : public BluetoothExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.write")
};

class BluetoothConnectFunction : public BluetoothExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.connect")
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
