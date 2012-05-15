// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

#if defined(OS_CHROMEOS)
#include "base/memory/ref_counted.h"

namespace chromeos {

class BluetoothAdapter;
class BluetoothDevice;
class BluetoothSocket;
class ExtensionBluetoothEventRouter;

}  // namespace chromeos
#endif

namespace extensions {
namespace api {

class BluetoothExtensionFunction : public SyncExtensionFunction {
 protected:
  virtual ~BluetoothExtensionFunction() {}

#if defined(OS_CHROMEOS)
  chromeos::ExtensionBluetoothEventRouter* event_router();
  const chromeos::BluetoothAdapter* adapter() const;
  chromeos::BluetoothAdapter* GetMutableAdapter();
#endif
};

class AsyncBluetoothExtensionFunction : public AsyncExtensionFunction {
 protected:
  virtual ~AsyncBluetoothExtensionFunction() {}

#if defined(OS_CHROMEOS)
  chromeos::ExtensionBluetoothEventRouter* event_router();
  const chromeos::BluetoothAdapter* adapter() const;
  chromeos::BluetoothAdapter* GetMutableAdapter();
#endif
};

class BluetoothIsAvailableFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.isAvailable")

 protected:
  virtual ~BluetoothIsAvailableFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothIsPoweredFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.isPowered")

 protected:
  virtual ~BluetoothIsPoweredFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetAddressFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.getAddress")

 protected:
  virtual ~BluetoothGetAddressFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetDevicesWithServiceUUIDFunction
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.getDevicesWithServiceUUID")

 protected:
  virtual ~BluetoothGetDevicesWithServiceUUIDFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetDevicesWithServiceNameFunction
    : public AsyncBluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.getDevicesWithServiceName")

#if defined(OS_CHROMEOS)
  BluetoothGetDevicesWithServiceNameFunction();
#endif

 protected:
  virtual ~BluetoothGetDevicesWithServiceNameFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

#if defined(OS_CHROMEOS)
 private:
  void AddDeviceIfTrue(
      ListValue* list, const chromeos::BluetoothDevice* device, bool result);

  int callbacks_pending_;
#endif
};

class BluetoothConnectFunction : public AsyncBluetoothExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.connect")

 private:
#if defined(OS_CHROMEOS)
  void ConnectToServiceCallback(
      const chromeos::BluetoothDevice* device,
      const std::string& service_uuid,
      scoped_refptr<chromeos::BluetoothSocket> socket);
#endif
};

class BluetoothDisconnectFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.disconnect")

 protected:
  virtual ~BluetoothDisconnectFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothReadFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.read")

 protected:
  virtual ~BluetoothReadFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothSetOutOfBandPairingDataFunction
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.setOutOfBandPairingData")

 protected:
  virtual ~BluetoothSetOutOfBandPairingDataFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetOutOfBandPairingDataFunction
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.getOutOfBandPairingData")

 protected:
  virtual ~BluetoothGetOutOfBandPairingDataFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothWriteFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.write")

 private:
  virtual ~BluetoothWriteFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
