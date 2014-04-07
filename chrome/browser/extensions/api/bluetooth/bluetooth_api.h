// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_extension_function.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_profile.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
}

namespace device {

class BluetoothAdapter;
class BluetoothDevice;
class BluetoothSocket;
struct BluetoothOutOfBandPairingData;

}  // namespace device

namespace extensions {

class BluetoothEventRouter;

// The profile-keyed service that manages the bluetooth extension API.
class BluetoothAPI : public BrowserContextKeyedAPI,
                     public EventRouter::Observer {
 public:
  // Convenience method to get the BluetoothAPI for a profile.
  static BluetoothAPI* Get(content::BrowserContext* context);

  static BrowserContextKeyedAPIFactory<BluetoothAPI>* GetFactoryInstance();

  explicit BluetoothAPI(content::BrowserContext* context);
  virtual ~BluetoothAPI();

  BluetoothEventRouter* bluetooth_event_router();

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;
  virtual void OnListenerRemoved(const EventListenerInfo& details) OVERRIDE;

 private:
  friend class BrowserContextKeyedAPIFactory<BluetoothAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BluetoothAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  content::BrowserContext* browser_context_;

  // Created lazily on first access.
  scoped_ptr<BluetoothEventRouter> bluetooth_event_router_;
};

namespace api {

class BluetoothAddProfileFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.addProfile", BLUETOOTH_ADDPROFILE)

  BluetoothAddProfileFunction();

 protected:
  virtual ~BluetoothAddProfileFunction() {}
  virtual bool RunImpl() OVERRIDE;

  virtual void RegisterProfile(
      const device::BluetoothProfile::Options& options,
      const device::BluetoothProfile::ProfileCallback& callback);

 private:
  void OnProfileRegistered(device::BluetoothProfile* bluetooth_profile);

  device::BluetoothUUID uuid_;
};

class BluetoothRemoveProfileFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.removeProfile",
                             BLUETOOTH_REMOVEPROFILE)

 protected:
  virtual ~BluetoothRemoveProfileFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetAdapterStateFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.getAdapterState",
                             BLUETOOTH_GETADAPTERSTATE)

 protected:
  virtual ~BluetoothGetAdapterStateFunction() {}

  // BluetoothExtensionFunction:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;
};

class BluetoothGetDevicesFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.getDevices", BLUETOOTH_GETDEVICES)

 protected:
  virtual ~BluetoothGetDevicesFunction() {}

  // BluetoothExtensionFunction:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;
};

class BluetoothGetDeviceFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.getDevice", BLUETOOTH_GETDEVICE)

 protected:
  virtual ~BluetoothGetDeviceFunction() {}

  // BluetoothExtensionFunction:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;
};

class BluetoothConnectFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.connect", BLUETOOTH_CONNECT)

 protected:
  virtual ~BluetoothConnectFunction() {}

  // BluetoothExtensionFunction:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;

 private:
  void OnSuccessCallback();
  void OnErrorCallback();
};

class BluetoothDisconnectFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.disconnect", BLUETOOTH_DISCONNECT)

 protected:
  virtual ~BluetoothDisconnectFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothReadFunction : public AsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.read", BLUETOOTH_READ)
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
  DECLARE_EXTENSION_FUNCTION("bluetooth.write", BLUETOOTH_WRITE)
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
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.setOutOfBandPairingData",
                             BLUETOOTH_SETOUTOFBANDPAIRINGDATA)

 protected:
  virtual ~BluetoothSetOutOfBandPairingDataFunction() {}

  void OnSuccessCallback();
  void OnErrorCallback();

  // BluetoothExtensionFunction:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;
};

class BluetoothGetLocalOutOfBandPairingDataFunction
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.getLocalOutOfBandPairingData",
                             BLUETOOTH_GETLOCALOUTOFBANDPAIRINGDATA)

 protected:
  virtual ~BluetoothGetLocalOutOfBandPairingDataFunction() {}

  void ReadCallback(
      const device::BluetoothOutOfBandPairingData& data);
  void ErrorCallback();

  // BluetoothExtensionFunction:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;
};

class BluetoothStartDiscoveryFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.startDiscovery",
                             BLUETOOTH_STARTDISCOVERY)

 protected:
  virtual ~BluetoothStartDiscoveryFunction() {}

  // BluetoothExtensionFunction:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;

 private:
  void OnSuccessCallback();
  void OnErrorCallback();
};

class BluetoothStopDiscoveryFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.stopDiscovery", BLUETOOTH_STOPDISCOVERY)

 protected:
  virtual ~BluetoothStopDiscoveryFunction() {}

  // BluetoothExtensionFunction:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;

 private:
  void OnSuccessCallback();
  void OnErrorCallback();
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
