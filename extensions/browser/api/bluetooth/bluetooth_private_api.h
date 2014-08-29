// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_PRIVATE_API_H_

#include "base/callback_forward.h"
#include "extensions/browser/api/bluetooth/bluetooth_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

namespace device {
class BluetoothAdapter;
}

namespace extensions {

class BluetoothApiPairingDelegate;

// The profile-keyed service that manages the bluetoothPrivate extension API.
class BluetoothPrivateAPI : public BrowserContextKeyedAPI,
                            public EventRouter::Observer {
 public:
  static BrowserContextKeyedAPIFactory<BluetoothPrivateAPI>*
      GetFactoryInstance();

  explicit BluetoothPrivateAPI(content::BrowserContext* context);
  virtual ~BluetoothPrivateAPI();

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;
  virtual void OnListenerRemoved(const EventListenerInfo& details) OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BluetoothPrivateAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

 private:
  friend class BrowserContextKeyedAPIFactory<BluetoothPrivateAPI>;

  content::BrowserContext* browser_context_;
};

namespace core_api {

class BluetoothPrivateSetAdapterStateFunction
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothPrivate.setAdapterState",
                             BLUETOOTHPRIVATE_SETADAPTERSTATE)
  BluetoothPrivateSetAdapterStateFunction();

 private:
  virtual ~BluetoothPrivateSetAdapterStateFunction();

  base::Closure CreatePropertySetCallback(const std::string& property_name);
  base::Closure CreatePropertyErrorCallback(const std::string& property_name);
  void OnAdapterPropertySet(const std::string& property);
  void OnAdapterPropertyError(const std::string& property);
  void SendError();

  // BluetoothExtensionFunction overrides:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;

  // Set of expected adapter properties to be changed.
  std::set<std::string> pending_properties_;

  // Set of adapter properties that were not set successfully.
  std::set<std::string> failed_properties_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothPrivateSetAdapterStateFunction);
};

class BluetoothPrivateSetPairingResponseFunction
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothPrivate.setPairingResponse",
                             BLUETOOTHPRIVATE_SETPAIRINGRESPONSE)
  BluetoothPrivateSetPairingResponseFunction();
  // BluetoothExtensionFunction overrides:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;

 private:
  virtual ~BluetoothPrivateSetPairingResponseFunction();
  DISALLOW_COPY_AND_ASSIGN(BluetoothPrivateSetPairingResponseFunction);
};

}  // namespace core_api

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_PRIVATE_API_H_
