// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/api_function.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

class BluetoothSocket;
struct BluetoothOutOfBandPairingData;

}  // namespace device

namespace extensions {

class ExtensionBluetoothEventRouter;

// The profile-keyed service that manages the bluetooth extension API.
class BluetoothAPI : public ProfileKeyedService,
                     public EventRouter::Observer {
 public:
  // Convenience method to get the BluetoothAPI for a profile.
  static BluetoothAPI* Get(Profile* profile);

  explicit BluetoothAPI(Profile* profile);
  virtual ~BluetoothAPI();

  ExtensionBluetoothEventRouter* bluetooth_event_router();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;
  virtual void OnListenerRemoved(const EventListenerInfo& details) OVERRIDE;

 private:
  Profile* profile_;

  // Created lazily on first access.
  scoped_ptr<ExtensionBluetoothEventRouter> bluetooth_event_router_;
};

namespace api {

class BluetoothGetAdapterStateFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bluetooth.getAdapterState")

 protected:
  virtual ~BluetoothGetAdapterStateFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetDevicesFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bluetooth.getDevices")

  BluetoothGetDevicesFunction();

 protected:
  virtual ~BluetoothGetDevicesFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  void DispatchDeviceSearchResult(const device::BluetoothDevice& device);
  void ProvidesServiceCallback(const device::BluetoothDevice* device,
                               bool providesService);
  void FinishDeviceSearch();

  int callbacks_pending_;
  int device_events_sent_;
};

class BluetoothGetServicesFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bluetooth.getServices")

 protected:
  virtual ~BluetoothGetServicesFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  void GetServiceRecordsCallback(
      base::ListValue* services,
      const device::BluetoothDevice::ServiceRecordList& records);
  void OnErrorCallback();
};

class BluetoothConnectFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bluetooth.connect")

 protected:
  virtual ~BluetoothConnectFunction() {}

  virtual bool RunImpl() OVERRIDE;

 private:
  void ConnectToServiceCallback(
      const device::BluetoothDevice* device,
      const std::string& service_uuid,
      scoped_refptr<device::BluetoothSocket> socket);
};

class BluetoothDisconnectFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bluetooth.disconnect")

 protected:
  virtual ~BluetoothDisconnectFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothReadFunction : public AsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bluetooth.read")
  BluetoothReadFunction();

 protected:
  virtual ~BluetoothReadFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual bool Respond() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  bool success_;
  scoped_refptr<device::BluetoothSocket> socket_;
};

class BluetoothWriteFunction : public AsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bluetooth.write")
  BluetoothWriteFunction();

 protected:
  virtual ~BluetoothWriteFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual bool Respond() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  bool success_;
  const base::BinaryValue* data_to_write_;  // memory is owned by args_
  scoped_refptr<device::BluetoothSocket> socket_;
};

class BluetoothSetOutOfBandPairingDataFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bluetooth.setOutOfBandPairingData")

 protected:
  virtual ~BluetoothSetOutOfBandPairingDataFunction() {}

  void OnSuccessCallback();
  void OnErrorCallback();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetLocalOutOfBandPairingDataFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bluetooth.getLocalOutOfBandPairingData")

 protected:
  virtual ~BluetoothGetLocalOutOfBandPairingDataFunction() {}

  void ReadCallback(
      const device::BluetoothOutOfBandPairingData& data);
  void ErrorCallback();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothStartDiscoveryFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bluetooth.startDiscovery")

 protected:
  virtual ~BluetoothStartDiscoveryFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnSuccessCallback();
  void OnErrorCallback();
};

class BluetoothStopDiscoveryFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("bluetooth.stopDiscovery")

 protected:
  virtual ~BluetoothStopDiscoveryFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnSuccessCallback();
  void OnErrorCallback();
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
