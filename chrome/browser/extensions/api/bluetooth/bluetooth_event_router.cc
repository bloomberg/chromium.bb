// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_event_router.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_utils.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/common/extensions/api/bluetooth.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_profile.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

namespace bluetooth = api::bluetooth;

// A struct storing a Bluetooth socket and the extension that added it.
struct ExtensionBluetoothSocketRecord {
  std::string extension_id;
  scoped_refptr<device::BluetoothSocket> socket;
};

// A struct storing a Bluetooth profile and the extension that added it.
struct ExtensionBluetoothProfileRecord {
  std::string extension_id;
  device::BluetoothProfile* profile;
};

ExtensionBluetoothEventRouter::ExtensionBluetoothEventRouter(
    content::BrowserContext* context)
    : browser_context_(context),
      adapter_(NULL),
      num_event_listeners_(0),
      next_socket_id_(1),
      weak_ptr_factory_(this) {
  DCHECK(browser_context_);
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<content::BrowserContext>(browser_context_));
}

ExtensionBluetoothEventRouter::~ExtensionBluetoothEventRouter() {
  if (adapter_.get()) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
  DLOG_IF(WARNING, socket_map_.size() != 0)
      << "Bluetooth sockets are still open.";
  socket_map_.clear();

  for (BluetoothProfileMap::iterator iter = bluetooth_profile_map_.begin();
       iter != bluetooth_profile_map_.end();
       ++iter) {
    iter->second.profile->Unregister();
  }
  for (DiscoverySessionMap::iterator iter = discovery_session_map_.begin();
       iter != discovery_session_map_.end();
       ++iter) {
    delete iter->second;
  }
}

bool ExtensionBluetoothEventRouter::IsBluetoothSupported() const {
  return adapter_.get() ||
         device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable();
}

void ExtensionBluetoothEventRouter::GetAdapter(
    const device::BluetoothAdapterFactory::AdapterCallback& callback) {
  if (adapter_.get()) {
    callback.Run(scoped_refptr<device::BluetoothAdapter>(adapter_));
    return;
  }

  device::BluetoothAdapterFactory::GetAdapter(callback);
}

void ExtensionBluetoothEventRouter::OnListenerAdded() {
  num_event_listeners_++;
  InitializeAdapterIfNeeded();
}

void ExtensionBluetoothEventRouter::OnListenerRemoved() {
  if (num_event_listeners_ > 0)
    num_event_listeners_--;
  MaybeReleaseAdapter();
}

int ExtensionBluetoothEventRouter::RegisterSocket(
    const std::string& extension_id,
    scoped_refptr<device::BluetoothSocket> socket) {
  // If there is a socket registered with the same fd, just return it's id
  for (SocketMap::const_iterator i = socket_map_.begin();
      i != socket_map_.end(); ++i) {
    if (i->second.socket.get() == socket.get())
      return i->first;
  }
  int return_id = next_socket_id_++;
  ExtensionBluetoothSocketRecord record = { extension_id, socket };
  socket_map_[return_id] = record;
  return return_id;
}

bool ExtensionBluetoothEventRouter::ReleaseSocket(int id) {
  SocketMap::iterator socket_entry = socket_map_.find(id);
  if (socket_entry == socket_map_.end())
    return false;
  socket_map_.erase(socket_entry);
  return true;
}

void ExtensionBluetoothEventRouter::AddProfile(
    const std::string& uuid,
    const std::string& extension_id,
    device::BluetoothProfile* bluetooth_profile) {
  DCHECK(!HasProfile(uuid));
  ExtensionBluetoothProfileRecord record = { extension_id, bluetooth_profile };
  bluetooth_profile_map_[uuid] = record;
}

void ExtensionBluetoothEventRouter::RemoveProfile(const std::string& uuid) {
  BluetoothProfileMap::iterator iter = bluetooth_profile_map_.find(uuid);
  if (iter != bluetooth_profile_map_.end()) {
    device::BluetoothProfile* bluetooth_profile = iter->second.profile;
    bluetooth_profile_map_.erase(iter);
    bluetooth_profile->Unregister();
  }
}

bool ExtensionBluetoothEventRouter::HasProfile(const std::string& uuid) const {
  return bluetooth_profile_map_.find(uuid) != bluetooth_profile_map_.end();
}

void ExtensionBluetoothEventRouter::StartDiscoverySession(
    device::BluetoothAdapter* adapter,
    const std::string& extension_id,
    const base::Closure& callback,
    const base::Closure& error_callback) {
  if (adapter != adapter_.get()) {
    error_callback.Run();
    return;
  }
  DiscoverySessionMap::iterator iter =
      discovery_session_map_.find(extension_id);
  if (iter != discovery_session_map_.end() && iter->second->IsActive()) {
    DVLOG(1) << "An active discovery session exists for extension.";
    error_callback.Run();
    return;
  }
  adapter->StartDiscoverySession(
      base::Bind(&ExtensionBluetoothEventRouter::OnStartDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr(),
                 extension_id,
                 callback),
      error_callback);
}

void ExtensionBluetoothEventRouter::StopDiscoverySession(
    device::BluetoothAdapter* adapter,
    const std::string& extension_id,
    const base::Closure& callback,
    const base::Closure& error_callback) {
  if (adapter != adapter_.get()) {
    error_callback.Run();
    return;
  }
  DiscoverySessionMap::iterator iter =
      discovery_session_map_.find(extension_id);
  if (iter == discovery_session_map_.end() || !iter->second->IsActive()) {
    DVLOG(1) << "No active discovery session exists for extension.";
    error_callback.Run();
    return;
  }
  device::BluetoothDiscoverySession* session = iter->second;
  session->Stop(callback, error_callback);
}

device::BluetoothProfile* ExtensionBluetoothEventRouter::GetProfile(
    const std::string& uuid) const {
  BluetoothProfileMap::const_iterator iter = bluetooth_profile_map_.find(uuid);
  if (iter != bluetooth_profile_map_.end())
    return iter->second.profile;

  return NULL;
}

scoped_refptr<device::BluetoothSocket>
ExtensionBluetoothEventRouter::GetSocket(int id) {
  SocketMap::iterator socket_entry = socket_map_.find(id);
  if (socket_entry == socket_map_.end())
    return NULL;
  return socket_entry->second.socket;
}

void ExtensionBluetoothEventRouter::DispatchDeviceEvent(
    const std::string& event_name, const bluetooth::Device& device) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(device.ToValue().release());
  scoped_ptr<Event> event(new Event(event_name, args.Pass()));
  ExtensionSystem::Get(browser_context_)->event_router()->BroadcastEvent(
      event.Pass());
}

void ExtensionBluetoothEventRouter::DispatchConnectionEvent(
    const std::string& extension_id,
    const std::string& uuid,
    const device::BluetoothDevice* device,
    scoped_refptr<device::BluetoothSocket> socket) {
  if (!HasProfile(uuid))
    return;

  int socket_id = RegisterSocket(extension_id, socket);
  api::bluetooth::Socket result_socket;
  api::bluetooth::BluetoothDeviceToApiDevice(*device, &result_socket.device);
  result_socket.profile.uuid = uuid;
  result_socket.id = socket_id;

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(result_socket.ToValue().release());
  scoped_ptr<Event> event(new Event(
      bluetooth::OnConnection::kEventName, args.Pass()));
  ExtensionSystem::Get(browser_context_)
      ->event_router()
      ->DispatchEventToExtension(extension_id, event.Pass());
}

void ExtensionBluetoothEventRouter::AdapterPresentChanged(
    device::BluetoothAdapter* adapter, bool present) {
  if (adapter != adapter_.get()) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->GetAddress();
    return;
  }
  DispatchAdapterStateEvent();
}

void ExtensionBluetoothEventRouter::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter, bool has_power) {
  if (adapter != adapter_.get()) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->GetAddress();
    return;
  }
  DispatchAdapterStateEvent();
}

void ExtensionBluetoothEventRouter::AdapterDiscoveringChanged(
    device::BluetoothAdapter* adapter, bool discovering) {
  if (adapter != adapter_.get()) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->GetAddress();
    return;
  }

  if (!discovering) {
    // If any discovery sessions are inactive, clean them up.
    DiscoverySessionMap active_session_map;
    for (DiscoverySessionMap::iterator iter = discovery_session_map_.begin();
         iter != discovery_session_map_.end();
         ++iter) {
      device::BluetoothDiscoverySession* session = iter->second;
      if (session->IsActive()) {
        active_session_map[iter->first] = session;
        continue;
      }
      delete session;
    }
    discovery_session_map_.swap(active_session_map);
  }

  DispatchAdapterStateEvent();
}

void ExtensionBluetoothEventRouter::DeviceAdded(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device) {
  if (adapter != adapter_.get()) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->GetAddress();
    return;
  }

  bluetooth::Device* extension_device =
      new bluetooth::Device();
  bluetooth::BluetoothDeviceToApiDevice(
      *device, extension_device);

  DispatchDeviceEvent(extensions::event_names::kBluetoothOnDeviceDiscovered,
                      *extension_device);
}

void ExtensionBluetoothEventRouter::InitializeAdapterIfNeeded() {
  if (!adapter_.get()) {
    GetAdapter(base::Bind(&ExtensionBluetoothEventRouter::InitializeAdapter,
                          weak_ptr_factory_.GetWeakPtr()));
  }
}

void ExtensionBluetoothEventRouter::InitializeAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (!adapter_.get()) {
    adapter_ = adapter;
    adapter_->AddObserver(this);
  }
}

void ExtensionBluetoothEventRouter::MaybeReleaseAdapter() {
  if (adapter_.get() && num_event_listeners_ == 0) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
}

void ExtensionBluetoothEventRouter::DispatchAdapterStateEvent() {
  api::bluetooth::AdapterState state;
  PopulateAdapterState(*adapter_.get(), &state);

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(state.ToValue().release());
  scoped_ptr<Event> event(new Event(
      bluetooth::OnAdapterStateChanged::kEventName,
      args.Pass()));
  ExtensionSystem::Get(browser_context_)->event_router()->BroadcastEvent(
      event.Pass());
}

void ExtensionBluetoothEventRouter::CleanUpForExtension(
    const std::string& extension_id) {
  // Remove all profiles added by the extension.
  BluetoothProfileMap::iterator profile_iter = bluetooth_profile_map_.begin();
  while (profile_iter != bluetooth_profile_map_.end()) {
    ExtensionBluetoothProfileRecord record = profile_iter->second;
    if (record.extension_id == extension_id) {
      bluetooth_profile_map_.erase(profile_iter++);
      record.profile->Unregister();
    } else {
      profile_iter++;
    }
  }

  // Remove all sockets opened by the extension.
  SocketMap::iterator socket_iter = socket_map_.begin();
  while (socket_iter != socket_map_.end()) {
    int socket_id = socket_iter->first;
    ExtensionBluetoothSocketRecord record = socket_iter->second;
    socket_iter++;
    if (record.extension_id == extension_id) {
      ReleaseSocket(socket_id);
    }
  }

  // Remove any discovery session initiated by the extension.
  DiscoverySessionMap::iterator session_iter =
      discovery_session_map_.find(extension_id);
  if (session_iter == discovery_session_map_.end())
    return;
  delete session_iter->second;
  discovery_session_map_.erase(session_iter);
}

void ExtensionBluetoothEventRouter::OnStartDiscoverySession(
    const std::string& extension_id,
    const base::Closure& callback,
    scoped_ptr<device::BluetoothDiscoverySession> discovery_session) {
  // Clean up any existing session instance for the extension.
  DiscoverySessionMap::iterator iter =
      discovery_session_map_.find(extension_id);
  if (iter != discovery_session_map_.end())
    delete iter->second;
  discovery_session_map_[extension_id] = discovery_session.release();
  callback.Run();
}

void ExtensionBluetoothEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      extensions::UnloadedExtensionInfo* info =
          content::Details<extensions::UnloadedExtensionInfo>(details).ptr();
      CleanUpForExtension(info->extension->id());
      break;
    }
  }
}

}  // namespace extensions
