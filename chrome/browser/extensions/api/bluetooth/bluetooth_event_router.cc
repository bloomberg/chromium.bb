// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_event_router.h"

#include <map>
#include <string>

#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_utils.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/extensions/api/bluetooth.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"

namespace extensions {

ExtensionBluetoothEventRouter::ExtensionBluetoothEventRouter(Profile* profile)
    : send_discovery_events_(false),
      responsible_for_discovery_(false),
      profile_(profile),
      adapter_(NULL),
      num_event_listeners_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(profile_);
}

ExtensionBluetoothEventRouter::~ExtensionBluetoothEventRouter() {
  if (adapter_) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
}

bool ExtensionBluetoothEventRouter::IsBluetoothSupported() const {
  return adapter_ ||
         device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable();
}

void ExtensionBluetoothEventRouter::GetAdapter(
    const device::BluetoothAdapterFactory::AdapterCallback& callback) {
  if (adapter_) {
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
    const char* event_name, const extensions::api::bluetooth::Device& device) {
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(device.ToValue().release());
  scoped_ptr<Event> event(new Event(event_name, args.Pass()));
  ExtensionSystem::Get(profile_)->event_router()->BroadcastEvent(event.Pass());
}

void ExtensionBluetoothEventRouter::AdapterPresentChanged(
    device::BluetoothAdapter* adapter, bool present) {
  if (adapter != adapter_) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->address();
    return;
  }
  DispatchAdapterStateEvent();
}

void ExtensionBluetoothEventRouter::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter, bool has_power) {
  if (adapter != adapter_) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->address();
    return;
  }
  DispatchAdapterStateEvent();
}

void ExtensionBluetoothEventRouter::AdapterDiscoveringChanged(
    device::BluetoothAdapter* adapter, bool discovering) {
  if (adapter != adapter_) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->address();
    return;
  }

  if (!discovering) {
    send_discovery_events_ = false;
    responsible_for_discovery_ = false;
    discovered_devices_.clear();
  }

  DispatchAdapterStateEvent();
}

void ExtensionBluetoothEventRouter::DeviceAdded(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device) {
  if (adapter != adapter_) {
    DVLOG(1) << "Ignoring event for adapter " << adapter->address();
    return;
  }

  extensions::api::bluetooth::Device* extension_device =
      new extensions::api::bluetooth::Device();
  extensions::api::bluetooth::BluetoothDeviceToApiDevice(
      *device, extension_device);
  discovered_devices_.push_back(extension_device);

  if (!send_discovery_events_)
    return;

  DispatchDeviceEvent(extensions::event_names::kBluetoothOnDeviceDiscovered,
                      *extension_device);
}

void ExtensionBluetoothEventRouter::InitializeAdapterIfNeeded() {
  if (!adapter_) {
    GetAdapter(base::Bind(&ExtensionBluetoothEventRouter::InitializeAdapter,
                          weak_ptr_factory_.GetWeakPtr()));
  }
}

void ExtensionBluetoothEventRouter::InitializeAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (!adapter_) {
    adapter_ = adapter;
    adapter_->AddObserver(this);
  }
}

void ExtensionBluetoothEventRouter::MaybeReleaseAdapter() {
  if (adapter_ && num_event_listeners_ == 0) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
}

void ExtensionBluetoothEventRouter::DispatchAdapterStateEvent() {
  api::bluetooth::AdapterState state;
  PopulateAdapterState(*adapter_, &state);

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(state.ToValue().release());
  scoped_ptr<Event> event(new Event(
      extensions::event_names::kBluetoothOnAdapterStateChanged,
      args.Pass()));
  ExtensionSystem::Get(profile_)->event_router()->BroadcastEvent(event.Pass());
}

}  // namespace extensions
