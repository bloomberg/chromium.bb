// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/bluetooth_event_router.h"

#include <map>

#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_socket.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_utils.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/common/extensions/api/experimental_bluetooth.h"

namespace experimental_bluetooth = extensions::api::experimental_bluetooth;

namespace chromeos {

ExtensionBluetoothEventRouter::ExtensionBluetoothEventRouter(Profile* profile)
    : profile_(profile),
      adapter_(chromeos::BluetoothAdapter::DefaultAdapter()),
      next_socket_id_(1) {
  DCHECK(profile_);
  DCHECK(adapter_.get());
  adapter_->AddObserver(this);
}

ExtensionBluetoothEventRouter::~ExtensionBluetoothEventRouter() {
  adapter_->RemoveObserver(this);
  socket_map_.clear();
}

int ExtensionBluetoothEventRouter::RegisterSocket(
    scoped_refptr<BluetoothSocket> socket) {
  // If there is a socket registered with the same fd, just return it's id
  for (SocketMap::const_iterator i = socket_map_.begin();
      i != socket_map_.end(); ++i) {
    if (i->second->fd() == socket->fd()) {
      return i->first;
    }
  }
  int return_id = next_socket_id_++;
  socket_map_[return_id] = socket;
  return return_id;
}

bool ExtensionBluetoothEventRouter::ReleaseSocket(int id) {
  SocketMap::iterator socket_entry = socket_map_.find(id);
  if (socket_entry == socket_map_.end())
    return false;
  socket_map_.erase(socket_entry);
  return true;
}

scoped_refptr<BluetoothSocket> ExtensionBluetoothEventRouter::GetSocket(
    int id) {
  SocketMap::iterator socket_entry = socket_map_.find(id);
  if (socket_entry == socket_map_.end())
    return NULL;
  return socket_entry->second;
}

void ExtensionBluetoothEventRouter::SetResponsibleForDiscovery(
    bool responsible) {
  responsible_for_discovery_ = responsible;
}

bool ExtensionBluetoothEventRouter::IsResponsibleForDiscovery() const {
  return responsible_for_discovery_;
}

void ExtensionBluetoothEventRouter::SetSendDiscoveryEvents(bool should_send) {
  // At the transition into sending devices, also send past devices that
  // were discovered as they will not be discovered again.
  if (should_send && !send_discovery_events_) {
    for (DeviceList::const_iterator i = discovered_devices_.begin();
        i != discovered_devices_.end(); ++i) {
      DispatchDeviceEvent(extensions::event_names::kBluetoothOnDeviceDiscovered,
                          **i);
    }
  }

  send_discovery_events_ = should_send;
}

void ExtensionBluetoothEventRouter::DispatchDeviceEvent(
    const char* event_name, const experimental_bluetooth::Device& device) {
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(device.ToValue().release());
  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      event_name,
      args.Pass(),
      NULL,
      GURL());
}

void ExtensionBluetoothEventRouter::AdapterPresentChanged(
    chromeos::BluetoothAdapter* adapter, bool present) {
  if (adapter != adapter_.get()) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->address();
    return;
  }

  DispatchBooleanValueEvent(
      extensions::event_names::kBluetoothOnAvailabilityChanged,
      present);
}

void ExtensionBluetoothEventRouter::AdapterPoweredChanged(
    chromeos::BluetoothAdapter* adapter, bool has_power) {
  if (adapter != adapter_.get()) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->address();
    return;
  }

  DispatchBooleanValueEvent(
      extensions::event_names::kBluetoothOnPowerChanged,
      has_power);
}

void ExtensionBluetoothEventRouter::AdapterDiscoveringChanged(
    chromeos::BluetoothAdapter* adapter, bool discovering) {
  if (adapter != adapter_.get()) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->address();
    return;
  }

  if (!discovering) {
    send_discovery_events_ = false;
    responsible_for_discovery_ = false;
    discovered_devices_.clear();
  }

  DispatchBooleanValueEvent(
      extensions::event_names::kBluetoothOnDiscoveringChanged,
      discovering);
}

void ExtensionBluetoothEventRouter::DeviceAdded(
    chromeos::BluetoothAdapter* adapter, chromeos::BluetoothDevice* device) {
  if (adapter != adapter_.get()) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->address();
    return;
  }

  experimental_bluetooth::Device* extension_device =
      new experimental_bluetooth::Device();
  experimental_bluetooth::BluetoothDeviceToApiDevice(*device, extension_device);
  discovered_devices_.push_back(extension_device);

  if (!send_discovery_events_)
    return;

  DispatchDeviceEvent(extensions::event_names::kBluetoothOnDeviceDiscovered,
                      *extension_device);
}

void ExtensionBluetoothEventRouter::DispatchBooleanValueEvent(
    const char* event_name, bool value) {
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(Value::CreateBooleanValue(value));
  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      event_name, args.Pass(), NULL, GURL());
}

}  // namespace chromeos
