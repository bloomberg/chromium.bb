// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "chrome/common/extensions/api/bluetooth.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_socket.h"

class Profile;

namespace extensions {

class ExtensionBluetoothEventRouter
    : public device::BluetoothAdapter::Observer {
 public:
  explicit ExtensionBluetoothEventRouter(Profile* profile);
  virtual ~ExtensionBluetoothEventRouter();

  // GetAdapter will return NULL if the bluetooth adapter is not
  // supported in the current platform.
  scoped_refptr<device::BluetoothAdapter> GetAdapter();

  // Called when a bluetooth event listener is added.
  void OnListenerAdded();

  // Called when a bluetooth event listener is removed.
  void OnListenerRemoved();

  // Register the BluetoothSocket |socket| for use by the extensions system.
  // This class will hold onto the socket for its lifetime, or until
  // ReleaseSocket is called for the socket.  Returns an id for the socket.
  int RegisterSocket(scoped_refptr<device::BluetoothSocket> socket);

  // Release the BluetoothSocket corresponding to |id|.  Returns true if
  // the socket was found and released, false otherwise.
  bool ReleaseSocket(int id);

  // Get the BluetoothSocket corresponding to |id|.
  scoped_refptr<device::BluetoothSocket> GetSocket(int id);

  // Sets whether this Profile is responsible for the discovering state of the
  // adapter.
  void SetResponsibleForDiscovery(bool responsible);
  bool IsResponsibleForDiscovery() const;

  // Sets whether or not DeviceAdded events will be dispatched to extensions.
  void SetSendDiscoveryEvents(bool should_send);

  // Dispatch an event that takes a device as a parameter to all renderers.
  void DispatchDeviceEvent(
      const char* event_name,
      const extensions::api::bluetooth::Device& device);

  // Override from device::BluetoothAdapter::Observer
  virtual void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                     bool present) OVERRIDE;
  virtual void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                     bool has_power) OVERRIDE;
  virtual void AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                         bool discovering) OVERRIDE;
  virtual void DeviceAdded(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device) OVERRIDE;

  // Exposed for testing.
  void SetAdapterForTest(device::BluetoothAdapter* adapter) {
    adapter_ = adapter;
  }
 private:
  void InitializeAdapterIfNeeded();
  void MaybeReleaseAdapter();
  void DispatchAdapterStateEvent();

  bool send_discovery_events_;
  bool responsible_for_discovery_;

  Profile* profile_;
  scoped_refptr<device::BluetoothAdapter> adapter_;

  int num_event_listeners_;

  // The next id to use for referring to a BluetoothSocket.  We avoid using
  // the fd of the socket because we don't want to leak that information to
  // the extension javascript.
  int next_socket_id_;

  typedef std::map<int, scoped_refptr<device::BluetoothSocket> > SocketMap;
  SocketMap socket_map_;

  typedef ScopedVector<extensions::api::bluetooth::Device>
      DeviceList;
  DeviceList discovered_devices_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBluetoothEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_
