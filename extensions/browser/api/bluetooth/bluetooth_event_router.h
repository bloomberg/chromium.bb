// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_

#include <map>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/api/bluetooth.h"
#include "extensions/common/api/bluetooth_private.h"

namespace content {
class BrowserContext;
}

namespace device {

class BluetoothDevice;
class BluetoothDiscoverySession;

}  // namespace device

namespace extensions {
class BluetoothApiPairingDelegate;
class ExtensionRegistry;

class BluetoothEventRouter : public device::BluetoothAdapter::Observer,
                             public content::NotificationObserver,
                             public ExtensionRegistryObserver {
 public:
  explicit BluetoothEventRouter(content::BrowserContext* context);
  virtual ~BluetoothEventRouter();

  // Returns true if adapter_ has been initialized for testing or bluetooth
  // adapter is available for the current platform.
  bool IsBluetoothSupported() const;

  void GetAdapter(
      const device::BluetoothAdapterFactory::AdapterCallback& callback);

  // Requests that a new device discovery session be initiated for extension
  // with id |extension_id|. |callback| is called, if a session has been
  // initiated. |error_callback| is called, if the adapter failed to initiate
  // the session or if an active session already exists for the extension.
  void StartDiscoverySession(device::BluetoothAdapter* adapter,
                             const std::string& extension_id,
                             const base::Closure& callback,
                             const base::Closure& error_callback);

  // Requests that the active discovery session that belongs to the extension
  // with id |extension_id| be terminated. |callback| is called, if the session
  // successfully ended. |error_callback| is called, if the adapter failed to
  // terminate the session or if no active discovery session exists for the
  // extension.
  void StopDiscoverySession(device::BluetoothAdapter* adapter,
                            const std::string& extension_id,
                            const base::Closure& callback,
                            const base::Closure& error_callback);

  // Called when a bluetooth event listener is added.
  void OnListenerAdded();

  // Called when a bluetooth event listener is removed.
  void OnListenerRemoved();

  // Adds a pairing delegate for an extension.
  void AddPairingDelegate(const std::string& extension_id);

  // Removes the pairing delegate for an extension.
  void RemovePairingDelegate(const std::string& extension_id);

  // Returns the pairing delegate for an extension or NULL if it doesn't have a
  // pairing delegate.
  BluetoothApiPairingDelegate* GetPairingDelegate(
      const std::string& extension_id);

  // Exposed for testing.
  void SetAdapterForTest(device::BluetoothAdapter* adapter) {
    adapter_ = adapter;
  }

  // Override from device::BluetoothAdapter::Observer.
  virtual void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                     bool present) OVERRIDE;
  virtual void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                     bool has_power) OVERRIDE;
  virtual void AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                         bool discovering) OVERRIDE;
  virtual void DeviceAdded(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device) OVERRIDE;
  virtual void DeviceChanged(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* device) OVERRIDE;
  virtual void DeviceRemoved(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* device) OVERRIDE;

  // Overridden from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from ExtensionRegistryObserver.
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BluetoothEventRouter"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

 private:
  void OnAdapterInitialized(const base::Closure& callback,
                            scoped_refptr<device::BluetoothAdapter> adapter);
  void MaybeReleaseAdapter();
  void DispatchAdapterStateEvent();
  void DispatchDeviceEvent(const std::string& event_name,
                           device::BluetoothDevice* device);
  void CleanUpForExtension(const std::string& extension_id);
  void CleanUpAllExtensions();
  void OnStartDiscoverySession(
      const std::string& extension_id,
      const base::Closure& callback,
      scoped_ptr<device::BluetoothDiscoverySession> discovery_session);

  content::BrowserContext* browser_context_;
  scoped_refptr<device::BluetoothAdapter> adapter_;

  int num_event_listeners_;

  // A map that maps extension ids to BluetoothDiscoverySession pointers.
  typedef std::map<std::string, device::BluetoothDiscoverySession*>
      DiscoverySessionMap;
  DiscoverySessionMap discovery_session_map_;

  // Maps an extension id to its pairing delegate.
  typedef std::map<std::string, BluetoothApiPairingDelegate*>
      PairingDelegateMap;
  PairingDelegateMap pairing_delegate_map_;

  content::NotificationRegistrar registrar_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  base::WeakPtrFactory<BluetoothEventRouter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothEventRouter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_
