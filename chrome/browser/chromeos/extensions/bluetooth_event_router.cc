// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/bluetooth_event_router.h"

#include <map>

#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_socket.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_utils.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/common/extensions/api/experimental_bluetooth.h"

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

void ExtensionBluetoothEventRouter::SetSendDiscoveryEvents(bool should_send) {
  send_discovery_events_ = should_send;
}

void ExtensionBluetoothEventRouter::AdapterPresentChanged(
    chromeos::BluetoothAdapter* adapter, bool present) {
  DCHECK(adapter == adapter_.get());
  DispatchBooleanValueEvent(
      extension_event_names::kBluetoothOnAvailabilityChanged,
      present);
}

void ExtensionBluetoothEventRouter::AdapterPoweredChanged(
    chromeos::BluetoothAdapter* adapter, bool has_power) {
  DCHECK(adapter == adapter_.get());
  DispatchBooleanValueEvent(
      extension_event_names::kBluetoothOnPowerChanged,
      has_power);
}

void ExtensionBluetoothEventRouter::DeviceAdded(
    chromeos::BluetoothAdapter* adapter, chromeos::BluetoothDevice* device) {
  if (!send_discovery_events_)
    return;

  DCHECK(adapter == adapter_.get());

  extensions::api::experimental_bluetooth::Device extension_device;
  extensions::api::experimental_bluetooth::BluetoothDeviceToApiDevice(
      *device, &extension_device);

  ListValue args;
  args.Append(extension_device.ToValue().release());
  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      extension_event_names::kBluetoothOnDeviceDiscovered,
      json_args,
      NULL,
      GURL());
}

void ExtensionBluetoothEventRouter::DispatchBooleanValueEvent(
    const char* event_name, bool value) {
  ListValue args;
  args.Append(Value::CreateBooleanValue(value));
  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  // TODO(bryeung): only dispatch the event to interested renderers
  // crbug.com/133179
  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      event_name, json_args, NULL, GURL());
}

}  // namespace chromeos
