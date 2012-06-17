// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/bluetooth_event_router.h"

#include <map>

#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_socket.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"

namespace chromeos {

ExtensionBluetoothEventRouter::ExtensionBluetoothEventRouter(Profile* profile)
    : profile_(profile),
      adapter_(chromeos::BluetoothAdapter::CreateDefaultAdapter()),
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

void ExtensionBluetoothEventRouter::AdapterPresentChanged(
    chromeos::BluetoothAdapter* adapter, bool present) {
  DCHECK(adapter == adapter_.get());
  DispatchEvent(extension_event_names::kBluetoothOnAvailabilityChanged,
      present);
}

void ExtensionBluetoothEventRouter::AdapterPoweredChanged(
    chromeos::BluetoothAdapter* adapter, bool has_power) {
  DCHECK(adapter == adapter_.get());
  DispatchEvent(extension_event_names::kBluetoothOnPowerChanged, has_power);
}

void ExtensionBluetoothEventRouter::DispatchEvent(
    const char* event_name, bool value) {
  ListValue args;
  args.Append(Value::CreateBooleanValue(value));
  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      event_name, json_args, NULL, GURL());
}

}  // namespace chromeos
