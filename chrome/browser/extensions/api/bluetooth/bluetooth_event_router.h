// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_

#include <map>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/api/bluetooth.h"
#include "chrome/common/extensions/api/bluetooth_private.h"
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

class BluetoothApiPairingDelegate;

class BluetoothEventRouter : public device::BluetoothAdapter::Observer,
                             public content::NotificationObserver {
 public:
  explicit BluetoothEventRouter(content::BrowserContext* context);
  virtual ~BluetoothEventRouter();

  // Returns true if adapter_ has been initialized for testing or bluetooth
  // adapter is available for the current platform.
  bool IsBluetoothSupported() const;

  void GetAdapter(
      const device::BluetoothAdapterFactory::AdapterCallback& callback);

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

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BluetoothEventRouter"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

 private:
  // Forward declarations of internal structs.
  struct ExtensionBluetoothSocketRecord;
  struct ExtensionBluetoothProfileRecord;

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

  // The next id to use for referring to a BluetoothSocket.  We avoid using
  // the fd of the socket because we don't want to leak that information to
  // the extension javascript.
  int next_socket_id_;

  typedef std::map<int, ExtensionBluetoothSocketRecord> SocketMap;
  SocketMap socket_map_;

  // Maps uuids to a struct containing a Bluetooth profile and its
  // associated extension id.
  typedef std::map<std::string, ExtensionBluetoothProfileRecord>
      BluetoothProfileMap;
  BluetoothProfileMap bluetooth_profile_map_;

  // A map that maps extension ids to BluetoothDiscoverySession pointers.
  typedef std::map<std::string, device::BluetoothDiscoverySession*>
      DiscoverySessionMap;
  DiscoverySessionMap discovery_session_map_;

  // Maps an extension id to its pairing delegate.
  typedef std::map<std::string, BluetoothApiPairingDelegate*>
      PairingDelegateMap;
  PairingDelegateMap pairing_delegate_map_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<BluetoothEventRouter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_
