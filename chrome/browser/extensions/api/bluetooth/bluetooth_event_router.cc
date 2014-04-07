// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_event_router.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_pairing_delegate.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_utils.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_private_api.h"
#include "chrome/common/extensions/api/bluetooth.h"
#include "chrome/common/extensions/api/bluetooth_private.h"
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
namespace bt_private = api::bluetooth_private;

// A struct storing a Bluetooth socket and the extension that added it.
struct BluetoothEventRouter::ExtensionBluetoothSocketRecord {
  std::string extension_id;
  scoped_refptr<device::BluetoothSocket> socket;
};

// A struct storing a Bluetooth profile and the extension that added it.
struct BluetoothEventRouter::ExtensionBluetoothProfileRecord {
  std::string extension_id;
  device::BluetoothProfile* profile;
};

BluetoothEventRouter::BluetoothEventRouter(content::BrowserContext* context)
    : browser_context_(context),
      adapter_(NULL),
      num_event_listeners_(0),
      next_socket_id_(1),
      weak_ptr_factory_(this) {
  DCHECK(browser_context_);
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 content::Source<content::BrowserContext>(browser_context_));
}

BluetoothEventRouter::~BluetoothEventRouter() {
  if (adapter_.get()) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
  DLOG_IF(WARNING, socket_map_.size() != 0)
      << "Bluetooth sockets are still open.";
  CleanUpAllExtensions();
}

bool BluetoothEventRouter::IsBluetoothSupported() const {
  return adapter_.get() ||
         device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable();
}

void BluetoothEventRouter::GetAdapter(
    const device::BluetoothAdapterFactory::AdapterCallback& callback) {
  if (adapter_.get()) {
    callback.Run(scoped_refptr<device::BluetoothAdapter>(adapter_));
    return;
  }

  device::BluetoothAdapterFactory::GetAdapter(callback);
}

int BluetoothEventRouter::RegisterSocket(
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

bool BluetoothEventRouter::ReleaseSocket(int id) {
  SocketMap::iterator socket_entry = socket_map_.find(id);
  if (socket_entry == socket_map_.end())
    return false;
  socket_map_.erase(socket_entry);
  return true;
}

void BluetoothEventRouter::AddProfile(
    const device::BluetoothUUID& uuid,
    const std::string& extension_id,
    device::BluetoothProfile* bluetooth_profile) {
  DCHECK(!HasProfile(uuid));
  ExtensionBluetoothProfileRecord record = { extension_id, bluetooth_profile };
  bluetooth_profile_map_[uuid] = record;
}

void BluetoothEventRouter::RemoveProfile(const device::BluetoothUUID& uuid) {
  BluetoothProfileMap::iterator iter = bluetooth_profile_map_.find(uuid);
  if (iter != bluetooth_profile_map_.end()) {
    device::BluetoothProfile* bluetooth_profile = iter->second.profile;
    bluetooth_profile_map_.erase(iter);
    bluetooth_profile->Unregister();
  }
}

bool BluetoothEventRouter::HasProfile(const device::BluetoothUUID& uuid) const {
  return bluetooth_profile_map_.find(uuid) != bluetooth_profile_map_.end();
}

void BluetoothEventRouter::StartDiscoverySession(
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
      base::Bind(&BluetoothEventRouter::OnStartDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr(),
                 extension_id,
                 callback),
      error_callback);
}

void BluetoothEventRouter::StopDiscoverySession(
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

device::BluetoothProfile* BluetoothEventRouter::GetProfile(
    const device::BluetoothUUID& uuid) const {
  BluetoothProfileMap::const_iterator iter = bluetooth_profile_map_.find(uuid);
  if (iter != bluetooth_profile_map_.end())
    return iter->second.profile;

  return NULL;
}

scoped_refptr<device::BluetoothSocket> BluetoothEventRouter::GetSocket(int id) {
  SocketMap::iterator socket_entry = socket_map_.find(id);
  if (socket_entry == socket_map_.end())
    return NULL;
  return socket_entry->second.socket;
}

void BluetoothEventRouter::DispatchConnectionEvent(
    const std::string& extension_id,
    const device::BluetoothUUID& uuid,
    const device::BluetoothDevice* device,
    scoped_refptr<device::BluetoothSocket> socket) {
  if (!HasProfile(uuid))
    return;

  int socket_id = RegisterSocket(extension_id, socket);
  bluetooth::Socket result_socket;
  bluetooth::BluetoothDeviceToApiDevice(*device, &result_socket.device);
  result_socket.profile.uuid = uuid.canonical_value();
  result_socket.id = socket_id;

  scoped_ptr<base::ListValue> args =
      bluetooth::OnConnection::Create(result_socket);
  scoped_ptr<Event> event(new Event(
      bluetooth::OnConnection::kEventName, args.Pass()));
  ExtensionSystem::Get(browser_context_)
      ->event_router()
      ->DispatchEventToExtension(extension_id, event.Pass());
}

BluetoothApiPairingDelegate* BluetoothEventRouter::GetPairingDelegate(
    const std::string& extension_id) {
  return ContainsKey(pairing_delegate_map_, extension_id)
             ? pairing_delegate_map_[extension_id]
             : NULL;
}

void BluetoothEventRouter::OnAdapterInitialized(
    const base::Closure& callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (!adapter_.get()) {
    adapter_ = adapter;
    adapter_->AddObserver(this);
  }

  callback.Run();
}

void BluetoothEventRouter::MaybeReleaseAdapter() {
  if (adapter_.get() && num_event_listeners_ == 0 &&
      pairing_delegate_map_.empty()) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
}

void BluetoothEventRouter::AddPairingDelegate(const std::string& extension_id) {
  if (!adapter_.get()) {
    base::Closure self_callback =
        base::Bind(&BluetoothEventRouter::AddPairingDelegate,
                   weak_ptr_factory_.GetWeakPtr(),
                   extension_id);
    GetAdapter(base::Bind(&BluetoothEventRouter::OnAdapterInitialized,
                          weak_ptr_factory_.GetWeakPtr(),
                          self_callback));
    return;
  }

  if (!ContainsKey(pairing_delegate_map_, extension_id)) {
    BluetoothApiPairingDelegate* delegate =
        new BluetoothApiPairingDelegate(extension_id, browser_context_);
    DCHECK(adapter_.get());
    adapter_->AddPairingDelegate(
        delegate, device::BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);
    pairing_delegate_map_[extension_id] = delegate;
  } else {
    LOG(ERROR) << "Pairing delegate already exists for extension. "
               << "There should be at most one onPairing listener.";
    NOTREACHED();
  }
}

void BluetoothEventRouter::RemovePairingDelegate(
    const std::string& extension_id) {
  if (ContainsKey(pairing_delegate_map_, extension_id)) {
    BluetoothApiPairingDelegate* delegate = pairing_delegate_map_[extension_id];
    if (adapter_.get())
      adapter_->RemovePairingDelegate(delegate);
    pairing_delegate_map_.erase(extension_id);
    delete delegate;
    MaybeReleaseAdapter();
  }
}

void BluetoothEventRouter::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  if (adapter != adapter_.get()) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->GetAddress();
    return;
  }
  DispatchAdapterStateEvent();
}

void BluetoothEventRouter::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool has_power) {
  if (adapter != adapter_.get()) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->GetAddress();
    return;
  }
  DispatchAdapterStateEvent();
}

void BluetoothEventRouter::AdapterDiscoveringChanged(
    device::BluetoothAdapter* adapter,
    bool discovering) {
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
    MaybeReleaseAdapter();
  }

  DispatchAdapterStateEvent();
}

void BluetoothEventRouter::DeviceAdded(device::BluetoothAdapter* adapter,
                                       device::BluetoothDevice* device) {
  if (adapter != adapter_.get()) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->GetAddress();
    return;
  }

  DispatchDeviceEvent(bluetooth::OnDeviceAdded::kEventName, device);
}

void BluetoothEventRouter::DeviceChanged(device::BluetoothAdapter* adapter,
                                         device::BluetoothDevice* device) {
  if (adapter != adapter_.get()) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->GetAddress();
    return;
  }

  DispatchDeviceEvent(bluetooth::OnDeviceChanged::kEventName, device);
}

void BluetoothEventRouter::DeviceRemoved(device::BluetoothAdapter* adapter,
                                         device::BluetoothDevice* device) {
  if (adapter != adapter_.get()) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->GetAddress();
    return;
  }

  DispatchDeviceEvent(bluetooth::OnDeviceRemoved::kEventName, device);
}

void BluetoothEventRouter::OnListenerAdded() {
  num_event_listeners_++;
  if (!adapter_.get()) {
    GetAdapter(base::Bind(&BluetoothEventRouter::OnAdapterInitialized,
                          weak_ptr_factory_.GetWeakPtr(),
                          base::Bind(&base::DoNothing)));
  }
}

void BluetoothEventRouter::OnListenerRemoved() {
  if (num_event_listeners_ > 0)
    num_event_listeners_--;
  MaybeReleaseAdapter();
}

void BluetoothEventRouter::DispatchAdapterStateEvent() {
  bluetooth::AdapterState state;
  PopulateAdapterState(*adapter_.get(), &state);

  scoped_ptr<base::ListValue> args =
      bluetooth::OnAdapterStateChanged::Create(state);
  scoped_ptr<Event> event(new Event(
      bluetooth::OnAdapterStateChanged::kEventName,
      args.Pass()));
  ExtensionSystem::Get(browser_context_)->event_router()->BroadcastEvent(
      event.Pass());
}

void BluetoothEventRouter::DispatchDeviceEvent(
    const std::string& event_name,
    device::BluetoothDevice* device) {
  bluetooth::Device extension_device;
  bluetooth::BluetoothDeviceToApiDevice(*device, &extension_device);

  scoped_ptr<base::ListValue> args =
      bluetooth::OnDeviceAdded::Create(extension_device);
  scoped_ptr<Event> event(new Event(event_name, args.Pass()));
  ExtensionSystem::Get(browser_context_)->event_router()->BroadcastEvent(
      event.Pass());
}

void BluetoothEventRouter::CleanUpForExtension(
    const std::string& extension_id) {
  RemovePairingDelegate(extension_id);

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

void BluetoothEventRouter::CleanUpAllExtensions() {
  for (BluetoothProfileMap::iterator it = bluetooth_profile_map_.begin();
       it != bluetooth_profile_map_.end();
       ++it) {
    it->second.profile->Unregister();
  }
  bluetooth_profile_map_.clear();

  for (DiscoverySessionMap::iterator it = discovery_session_map_.begin();
       it != discovery_session_map_.end();
       ++it) {
    delete it->second;
  }
  discovery_session_map_.clear();

  SocketMap::iterator socket_iter = socket_map_.begin();
  while (socket_iter != socket_map_.end())
    ReleaseSocket(socket_iter++->first);

  PairingDelegateMap::iterator pairing_iter = pairing_delegate_map_.begin();
  while (pairing_iter != pairing_delegate_map_.end())
    RemovePairingDelegate(pairing_iter++->first);
}

void BluetoothEventRouter::OnStartDiscoverySession(
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

void BluetoothEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED: {
      extensions::UnloadedExtensionInfo* info =
          content::Details<extensions::UnloadedExtensionInfo>(details).ptr();
      CleanUpForExtension(info->extension->id());
      break;
    }
  }
}

}  // namespace extensions
