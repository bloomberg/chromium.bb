// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_socket.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_extension_function.h"
#include "chrome/common/extensions/api/bluetooth.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_profile.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
}

namespace device {
class BluetoothAdapter;
struct BluetoothOutOfBandPairingData;
}

namespace net {
class IOBuffer;
}

namespace extensions {

class BluetoothEventRouter;

// The profile-keyed service that manages the bluetooth extension API.
// All methods of this class must be called on the UI thread.
// TODO(rpaquay): Rename this and move to separate file.
class BluetoothAPI : public BrowserContextKeyedAPI,
                     public EventRouter::Observer {
 public:
  typedef ApiResourceManager<BluetoothApiSocket>::ApiResourceData SocketData;

  struct ConnectionParams {
    ConnectionParams();
    ~ConnectionParams();

    content::BrowserThread::ID thread_id;
    void* browser_context_id;
    std::string extension_id;
    std::string device_address;
    device::BluetoothUUID uuid;
    scoped_refptr<device::BluetoothSocket> socket;
    scoped_refptr<SocketData> socket_data;
  };

  // Convenience method to get the BluetoothAPI for a browser context.
  static BluetoothAPI* Get(content::BrowserContext* context);

  static BrowserContextKeyedAPIFactory<BluetoothAPI>* GetFactoryInstance();

  explicit BluetoothAPI(content::BrowserContext* context);
  virtual ~BluetoothAPI();

  BluetoothEventRouter* event_router();
  scoped_refptr<SocketData> socket_data();

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;
  virtual void OnListenerRemoved(const EventListenerInfo& details) OVERRIDE;

  // Dispatch an event that takes a connection socket as a parameter to the
  // extension that registered the profile that the socket has connected to.
  void DispatchConnectionEvent(const std::string& extension_id,
                               const device::BluetoothUUID& profile_uuid,
                               const device::BluetoothDevice* device,
                               scoped_refptr<device::BluetoothSocket> socket);

 private:
  static void RegisterSocket(const ConnectionParams& params);
  static void RegisterSocketUI(const ConnectionParams& params, int socket_id);
  static void RegisterSocketWithAdapterUI(
      const ConnectionParams& params,
      int socket_id,
      scoped_refptr<device::BluetoothAdapter> adapter);

  // BrowserContextKeyedAPI implementation.
  friend class BrowserContextKeyedAPIFactory<BluetoothAPI>;
  static const char* service_name() { return "BluetoothAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  content::BrowserContext* browser_context_;

  // Created lazily on first access.
  scoped_ptr<BluetoothEventRouter> event_router_;
  scoped_refptr<SocketData> socket_data_;
};

namespace api {

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

class BluetoothAddProfileFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.addProfile", BLUETOOTH_ADDPROFILE)

  BluetoothAddProfileFunction();

 protected:
  virtual ~BluetoothAddProfileFunction();
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
  virtual ~BluetoothRemoveProfileFunction();
  virtual bool RunSync() OVERRIDE;
};

class BluetoothConnectFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.connect", BLUETOOTH_CONNECT)

 protected:
  virtual ~BluetoothConnectFunction();

  // BluetoothExtensionFunction:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;

 private:
  void OnSuccessCallback();
  void OnErrorCallback(const std::string& error);
};

class BluetoothDisconnectFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.disconnect", BLUETOOTH_DISCONNECT)

 protected:
  virtual ~BluetoothDisconnectFunction() {}

  // UIThreadExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothSendFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.send", BLUETOOTH_WRITE)

 protected:
  virtual ~BluetoothSendFunction() {}

  // UIThreadExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothUpdateSocketFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.updateSocket", BLUETOOTH_UPDATE_SOCKET)

 protected:
  virtual ~BluetoothUpdateSocketFunction() {}

  // UIThreadExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothSetSocketPausedFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.setSocketPaused",
                             BLUETOOTH_SET_SOCKET_PAUSED)

 protected:
  virtual ~BluetoothSetSocketPausedFunction() {}

  // UIThreadExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetSocketFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.getSocket", BLUETOOTH_GET_SOCKET)

 protected:
  virtual ~BluetoothGetSocketFunction() {}

  // UIThreadExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetSocketsFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.getSockets", BLUETOOTH_GET_SOCKETS)

 protected:
  virtual ~BluetoothGetSocketsFunction() {}

  // UIThreadExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetLocalOutOfBandPairingDataFunction
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.getLocalOutOfBandPairingData",
                             BLUETOOTH_GETLOCALOUTOFBANDPAIRINGDATA)

 protected:
  virtual ~BluetoothGetLocalOutOfBandPairingDataFunction() {}

  void ReadCallback(const device::BluetoothOutOfBandPairingData& data);
  void ErrorCallback();

  // BluetoothExtensionFunction:
  virtual bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) OVERRIDE;
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
