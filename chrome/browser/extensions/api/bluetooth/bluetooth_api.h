// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_

#include <string>

#include "chrome/browser/extensions/api/api_function.h"
#include "chrome/browser/extensions/extension_function.h"

#if defined(OS_CHROMEOS)
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_socket.h"

namespace chromeos {

class BluetoothSocket;
struct BluetoothOutOfBandPairingData;

}  // namespace chromeos
#endif

namespace extensions {
namespace api {

class BluetoothIsAvailableFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.isAvailable")

 protected:
  virtual ~BluetoothIsAvailableFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothIsPoweredFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.isPowered")

 protected:
  virtual ~BluetoothIsPoweredFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetAddressFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.getAddress")

 protected:
  virtual ~BluetoothGetAddressFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetNameFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.getName")

 protected:
  virtual ~BluetoothGetNameFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetDevicesFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.getDevices")

#if defined(OS_CHROMEOS)
  BluetoothGetDevicesFunction();
#endif

 protected:
  virtual ~BluetoothGetDevicesFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
#if defined(OS_CHROMEOS)
  void DispatchDeviceSearchResult(const chromeos::BluetoothDevice& device);
  void ProvidesServiceCallback(const chromeos::BluetoothDevice* device,
                               bool providesService);

  int callbacks_pending_;
#endif
};

class BluetoothGetServicesFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.getServices")

 protected:
  virtual ~BluetoothGetServicesFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

#if defined(OS_CHROMEOS)
 private:
  void GetServiceRecordsCallback(
      base::ListValue* services,
      const chromeos::BluetoothDevice::ServiceRecordList& records);
  void OnErrorCallback();
#endif
};

class BluetoothConnectFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.connect")

 protected:
  virtual ~BluetoothConnectFunction() {}

  virtual bool RunImpl() OVERRIDE;

 private:
#if defined(OS_CHROMEOS)
  void ConnectToServiceCallback(
      const chromeos::BluetoothDevice* device,
      const std::string& service_uuid,
      scoped_refptr<chromeos::BluetoothSocket> socket);
#endif
};

class BluetoothDisconnectFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.disconnect")

 protected:
  virtual ~BluetoothDisconnectFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothReadFunction : public AsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.read")
  BluetoothReadFunction();

 protected:
  virtual ~BluetoothReadFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual bool Respond() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
#if defined(OS_CHROMEOS)
  bool success_;
  scoped_refptr<chromeos::BluetoothSocket> socket_;
#endif
};

class BluetoothWriteFunction : public AsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.write")
  BluetoothWriteFunction();

 protected:
  virtual ~BluetoothWriteFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual bool Respond() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
#if defined(OS_CHROMEOS)
  bool success_;
  const base::BinaryValue* data_to_write_;  // memory is owned by args_
  scoped_refptr<chromeos::BluetoothSocket> socket_;
#endif
};

class BluetoothSetOutOfBandPairingDataFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.setOutOfBandPairingData")

 protected:
  virtual ~BluetoothSetOutOfBandPairingDataFunction() {}

#if defined(OS_CHROMEOS)
  void OnSuccessCallback();
  void OnErrorCallback();
#endif

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetLocalOutOfBandPairingDataFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.getLocalOutOfBandPairingData")

 protected:
  virtual ~BluetoothGetLocalOutOfBandPairingDataFunction() {}

#if defined(OS_CHROMEOS)
  void ReadCallback(const chromeos::BluetoothOutOfBandPairingData& data);
  void ErrorCallback();
#endif

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothStartDiscoveryFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.startDiscovery")

 protected:
  virtual ~BluetoothStartDiscoveryFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

#if defined(OS_CHROMEOS)
 private:
  void OnSuccessCallback();
  void OnErrorCallback();
#endif
};

class BluetoothStopDiscoveryFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.stopDiscovery")

 protected:
  virtual ~BluetoothStopDiscoveryFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

#if defined(OS_CHROMEOS)
 private:
  void OnSuccessCallback();
  void OnErrorCallback();
#endif
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
