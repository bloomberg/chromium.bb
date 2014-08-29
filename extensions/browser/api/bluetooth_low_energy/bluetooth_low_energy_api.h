// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_API_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_API_H_

#include "base/memory/scoped_ptr.h"
#include "extensions/browser/api/bluetooth_low_energy/bluetooth_low_energy_event_router.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class BluetoothLowEnergyEventRouter;

// The profile-keyed service that manages the bluetoothLowEnergy extension API.
class BluetoothLowEnergyAPI : public BrowserContextKeyedAPI {
 public:
  static BrowserContextKeyedAPIFactory<BluetoothLowEnergyAPI>*
      GetFactoryInstance();

  // Convenience method to get the BluetoothLowEnergy API for a browser context.
  static BluetoothLowEnergyAPI* Get(content::BrowserContext* context);

  explicit BluetoothLowEnergyAPI(content::BrowserContext* context);
  virtual ~BluetoothLowEnergyAPI();

  // KeyedService implementation..
  virtual void Shutdown() OVERRIDE;

  BluetoothLowEnergyEventRouter* event_router() const {
    return event_router_.get();
  }

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BluetoothLowEnergyAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

 private:
  friend class BrowserContextKeyedAPIFactory<BluetoothLowEnergyAPI>;

  scoped_ptr<BluetoothLowEnergyEventRouter> event_router_;

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyAPI);
};

namespace core_api {

// Base class for bluetoothLowEnergy API functions. This class handles some of
// the common logic involved in all API functions, such as checking for
// platform support and returning the correct error.
class BluetoothLowEnergyExtensionFunction : public AsyncExtensionFunction {
 public:
  BluetoothLowEnergyExtensionFunction();

 protected:
  virtual ~BluetoothLowEnergyExtensionFunction();

  // ExtensionFunction override.
  virtual bool RunAsync() OVERRIDE;

  // Implemented by individual bluetoothLowEnergy extension functions to perform
  // the body of the function. This invoked asynchonously after RunAsync after
  // the BluetoothLowEnergyEventRouter has obtained a handle on the
  // BluetoothAdapter.
  virtual bool DoWork() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyExtensionFunction);
};

class BluetoothLowEnergyConnectFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.connect",
                             BLUETOOTHLOWENERGY_CONNECT);

 protected:
  virtual ~BluetoothLowEnergyConnectFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::Connect.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyDisconnectFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.disconnect",
                             BLUETOOTHLOWENERGY_DISCONNECT);

 protected:
  virtual ~BluetoothLowEnergyDisconnectFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::Disconnect.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyGetServiceFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getService",
                             BLUETOOTHLOWENERGY_GETSERVICE);

 protected:
  virtual ~BluetoothLowEnergyGetServiceFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;
};

class BluetoothLowEnergyGetServicesFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getServices",
                             BLUETOOTHLOWENERGY_GETSERVICES);

 protected:
  virtual ~BluetoothLowEnergyGetServicesFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;
};

class BluetoothLowEnergyGetCharacteristicFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getCharacteristic",
                             BLUETOOTHLOWENERGY_GETCHARACTERISTIC);

 protected:
  virtual ~BluetoothLowEnergyGetCharacteristicFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;
};

class BluetoothLowEnergyGetCharacteristicsFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getCharacteristics",
                             BLUETOOTHLOWENERGY_GETCHARACTERISTICS);

 protected:
  virtual ~BluetoothLowEnergyGetCharacteristicsFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;
};

class BluetoothLowEnergyGetIncludedServicesFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getIncludedServices",
                             BLUETOOTHLOWENERGY_GETINCLUDEDSERVICES);

 protected:
  virtual ~BluetoothLowEnergyGetIncludedServicesFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;
};

class BluetoothLowEnergyGetDescriptorFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getDescriptor",
                             BLUETOOTHLOWENERGY_GETDESCRIPTOR);

 protected:
  virtual ~BluetoothLowEnergyGetDescriptorFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;
};

class BluetoothLowEnergyGetDescriptorsFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getDescriptors",
                             BLUETOOTHLOWENERGY_GETDESCRIPTORS);

 protected:
  virtual ~BluetoothLowEnergyGetDescriptorsFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;
};

class BluetoothLowEnergyReadCharacteristicValueFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.readCharacteristicValue",
                             BLUETOOTHLOWENERGY_READCHARACTERISTICVALUE);

 protected:
  virtual ~BluetoothLowEnergyReadCharacteristicValueFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::ReadCharacteristicValue.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);

  // The instance ID of the requested characteristic.
  std::string instance_id_;
};

class BluetoothLowEnergyWriteCharacteristicValueFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.writeCharacteristicValue",
                             BLUETOOTHLOWENERGY_WRITECHARACTERISTICVALUE);

 protected:
  virtual ~BluetoothLowEnergyWriteCharacteristicValueFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::WriteCharacteristicValue.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);

  // The instance ID of the requested characteristic.
  std::string instance_id_;
};

class BluetoothLowEnergyStartCharacteristicNotificationsFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "bluetoothLowEnergy.startCharacteristicNotifications",
      BLUETOOTHLOWENERGY_STARTCHARACTERISTICNOTIFICATIONS);

 protected:
  virtual ~BluetoothLowEnergyStartCharacteristicNotificationsFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::StartCharacteristicNotifications.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyStopCharacteristicNotificationsFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "bluetoothLowEnergy.stopCharacteristicNotifications",
      BLUETOOTHLOWENERGY_STOPCHARACTERISTICNOTIFICATIONS);

 protected:
  virtual ~BluetoothLowEnergyStopCharacteristicNotificationsFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::StopCharacteristicNotifications.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyReadDescriptorValueFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.readDescriptorValue",
                             BLUETOOTHLOWENERGY_READDESCRIPTORVALUE);

 protected:
  virtual ~BluetoothLowEnergyReadDescriptorValueFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::ReadDescriptorValue.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);

  // The instance ID of the requested descriptor.
  std::string instance_id_;
};

class BluetoothLowEnergyWriteDescriptorValueFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.writeDescriptorValue",
                             BLUETOOTHLOWENERGY_WRITEDESCRIPTORVALUE);

 protected:
  virtual ~BluetoothLowEnergyWriteDescriptorValueFunction() {}

  // BluetoothLowEnergyExtensionFunction override.
  virtual bool DoWork() OVERRIDE;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::WriteDescriptorValue.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);

  // The instance ID of the requested descriptor.
  std::string instance_id_;
};

}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_API_H_
