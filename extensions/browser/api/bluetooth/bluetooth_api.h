// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "device/bluetooth/bluetooth_device.h"
#include "extensions/browser/api/bluetooth/bluetooth_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/bluetooth.h"

namespace content {
class BrowserContext;
}

namespace device {
class BluetoothAdapter;
}

namespace extensions {

class BluetoothEventRouter;

// The profile-keyed service that manages the bluetooth extension API.
// All methods of this class must be called on the UI thread.
// TODO(rpaquay): Rename this and move to separate file.
class BluetoothAPI : public BrowserContextKeyedAPI,
                     public EventRouter::Observer {
 public:
  // Convenience method to get the BluetoothAPI for a browser context.
  static BluetoothAPI* Get(content::BrowserContext* context);

  static BrowserContextKeyedAPIFactory<BluetoothAPI>* GetFactoryInstance();

  explicit BluetoothAPI(content::BrowserContext* context);
  virtual ~BluetoothAPI();

  BluetoothEventRouter* event_router();

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;
  virtual void OnListenerRemoved(const EventListenerInfo& details) OVERRIDE;

 private:
  // BrowserContextKeyedAPI implementation.
  friend class BrowserContextKeyedAPIFactory<BluetoothAPI>;
  static const char* service_name() { return "BluetoothAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  content::BrowserContext* browser_context_;

  // Created lazily on first access.
  scoped_ptr<BluetoothEventRouter> event_router_;
};

namespace core_api {

class BluetoothGetAdapterStateFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.getAdapterState",
                             BLUETOOTH_GETADAPTERSTATE)

 protected:
  virtual ~BluetoothGetAdapterStateFunction();

  // BluetoothExtensionFunction:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;
};

class BluetoothGetDevicesFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.getDevices", BLUETOOTH_GETDEVICES)

 protected:
  virtual ~BluetoothGetDevicesFunction();

  // BluetoothExtensionFunction:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;
};

class BluetoothGetDeviceFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.getDevice", BLUETOOTH_GETDEVICE)

  // BluetoothExtensionFunction:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;

 protected:
  virtual ~BluetoothGetDeviceFunction();
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

}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_H_
