// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/api/bluetooth.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_socket.h"

namespace content {
class BrowserContext;
}

namespace device {

class BluetoothDevice;
class BluetoothDiscoverySession;
class BluetoothProfile;

}  // namespace device

namespace extensions {

// Foward declarations of internal structs.
struct ExtensionBluetoothSocketRecord;
struct ExtensionBluetoothProfileRecord;

class ExtensionBluetoothEventRouter
    : public device::BluetoothAdapter::Observer,
      public content::NotificationObserver {
 public:
  explicit ExtensionBluetoothEventRouter(content::BrowserContext* context);
  virtual ~ExtensionBluetoothEventRouter();

  // Returns true if adapter_ has been initialized for testing or bluetooth
  // adapter is available for the current platform.
  bool IsBluetoothSupported() const;

  void GetAdapter(
      const device::BluetoothAdapterFactory::AdapterCallback& callback);

  // Called when a bluetooth event listener is added.
  void OnListenerAdded();

  // Called when a bluetooth event listener is removed.
  void OnListenerRemoved();

  // Register the BluetoothSocket |socket| for use by the extensions system.
  // This class will hold onto the socket for its lifetime until
  // ReleaseSocket is called for the socket, or until the extension associated
  // with the socket is disabled/ reloaded. Returns an id for the socket.
  int RegisterSocket(const std::string& extension_id,
                     scoped_refptr<device::BluetoothSocket> socket);

  // Release the BluetoothSocket corresponding to |id|.  Returns true if
  // the socket was found and released, false otherwise.
  bool ReleaseSocket(int id);

  // Add the BluetoothProfile |bluetooth_profile| for use by the extension
  // system. This class will hold onto the profile until RemoveProfile is
  // called for the profile, or until the extension that added the profile
  // is disabled/reloaded.
  void AddProfile(const std::string& uuid,
                  const std::string& extension_id,
                  device::BluetoothProfile* bluetooth_profile);

  // Unregister the BluetoothProfile corersponding to |uuid| and release the
  // object from this class.
  void RemoveProfile(const std::string& uuid);

  // Returns true if the BluetoothProfile corresponding to |uuid| is already
  // registered.
  bool HasProfile(const std::string& uuid) const;

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

  // Returns the BluetoothProfile that corresponds to |uuid|. It returns NULL
  // if the BluetoothProfile with |uuid| does not exist.
  device::BluetoothProfile* GetProfile(const std::string& uuid) const;

  // Get the BluetoothSocket corresponding to |id|.
  scoped_refptr<device::BluetoothSocket> GetSocket(int id);

  // Dispatch an event that takes a connection socket as a parameter to the
  // extension that registered the profile that the socket has connected to.
  void DispatchConnectionEvent(const std::string& extension_id,
                               const std::string& uuid,
                               const device::BluetoothDevice* device,
                               scoped_refptr<device::BluetoothSocket> socket);

  // Override from device::BluetoothAdapter::Observer
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

  // Overridden from content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Exposed for testing.
  void SetAdapterForTest(device::BluetoothAdapter* adapter) {
    adapter_ = adapter;
  }

 private:
  void InitializeAdapterIfNeeded();
  void InitializeAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void MaybeReleaseAdapter();
  void DispatchAdapterStateEvent();
  void DispatchDeviceEvent(const std::string& event_name,
                           device::BluetoothDevice* device);
  void CleanUpForExtension(const std::string& extension_id);
  void OnStartDiscoverySession(
      const std::string& extension_id,
      const base::Closure& callback,
      scoped_ptr<device::BluetoothDiscoverySession> discovery_session);

  content::BrowserContext* browser_context_;
  scoped_refptr<device::BluetoothAdapter> adapter_;

  int num_event_listeners_;

  // The next id to use for referring to a BluetoothSocket.  We avoid using
  // the fd of the socket because we don't want to leak that information to
  // the extension javascript.
  int next_socket_id_;

  typedef std::map<int, ExtensionBluetoothSocketRecord> SocketMap;
  SocketMap socket_map_;

  // A map that maps uuids to ExtensionBluetoothProfileRecord.
  typedef std::map<std::string, ExtensionBluetoothProfileRecord>
      BluetoothProfileMap;
  BluetoothProfileMap bluetooth_profile_map_;

  // A map that maps extension ids to BluetoothDiscoverySession pointers.
  typedef std::map<std::string, device::BluetoothDiscoverySession*>
      DiscoverySessionMap;
  DiscoverySessionMap discovery_session_map_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<ExtensionBluetoothEventRouter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBluetoothEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_
