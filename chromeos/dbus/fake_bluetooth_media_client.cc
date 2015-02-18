// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_media_client.h"

#include <string>

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_media_transport_client.h"

using dbus::ObjectPath;

namespace {

// Except for |kFailedError|, the other error is defined in BlueZ D-Bus Media
// API.
const char kFailedError[] = "org.chromium.Error.Failed";
const char kInvalidArgumentsError[] = "org.chromium.Error.InvalidArguments";

}  // namespace

namespace chromeos {

// static
const uint8_t FakeBluetoothMediaClient::kDefaultCodec = 0x00;

FakeBluetoothMediaClient::FakeBluetoothMediaClient()
    : visible_(true),
      object_path_(ObjectPath(FakeBluetoothAdapterClient::kAdapterPath)) {
}

FakeBluetoothMediaClient::~FakeBluetoothMediaClient() {
}

void FakeBluetoothMediaClient::Init(dbus::Bus* bus) {
}

void FakeBluetoothMediaClient::AddObserver(
    BluetoothMediaClient::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void FakeBluetoothMediaClient::RemoveObserver(
    BluetoothMediaClient::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void FakeBluetoothMediaClient::RegisterEndpoint(
    const ObjectPath& object_path,
    const ObjectPath& endpoint_path,
    const EndpointProperties& properties,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!visible_)
    return;

  VLOG(1) << "RegisterEndpoint: " << endpoint_path.value();

  // The media client and adapter client should have the same object path.
  if (object_path != object_path_ ||
      properties.uuid != BluetoothMediaClient::kBluetoothAudioSinkUUID ||
      properties.codec != kDefaultCodec || properties.capabilities.empty()) {
    error_callback.Run(kInvalidArgumentsError, "");
    return;
  }

  // Sets the state of a given endpoint path to true if it is registered.
  SetEndpointRegistered(endpoint_path, true);

  callback.Run();
}

void FakeBluetoothMediaClient::UnregisterEndpoint(
    const ObjectPath& object_path,
    const ObjectPath& endpoint_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // TODO(mcchou): Come up with some corresponding actions.
  error_callback.Run(kFailedError, "");
}

void FakeBluetoothMediaClient::SetVisible(bool visible) {
  visible_ = visible;

  if (visible_)
    return;

  // If the media object becomes invisible, an update chain will
  // unregister all endpoints and set the associated transport
  // objects to be invalid.
  for (std::map<ObjectPath, bool>::iterator it = endpoints_.begin();
       it != endpoints_.end(); ++it) {
    SetEndpointRegistered(it->first, false);
  }
  endpoints_.clear();

  // Notifies observers about the change on |visible_|.
  FOR_EACH_OBSERVER(BluetoothMediaClient::Observer, observers_,
                    MediaRemoved(object_path_));
}

void FakeBluetoothMediaClient::SetEndpointRegistered(
    const ObjectPath& endpoint_path,
    bool registered) {
  if (registered) {
    endpoints_[endpoint_path] = registered;
  } else {
    // Once a media endpoint object becomes invalid, the associated transport
    // becomes invalid.
    FakeBluetoothMediaTransportClient* transport =
        static_cast<FakeBluetoothMediaTransportClient*>(
            DBusThreadManager::Get()->GetBluetoothMediaTransportClient());
    transport->SetValid(endpoint_path, true);
  }
}

}  // namespace chromeos
