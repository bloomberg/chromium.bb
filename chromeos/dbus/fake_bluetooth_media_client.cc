// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_media_client.h"

namespace chromeos {

FakeBluetoothMediaClient::FakeBluetoothMediaClient() {
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

// TODO(mcchou): Add method definition for |RegisterEndpoint|,
// |UnregisterEndpoint|, |RegisterPlayer| and |UnregisterPlayer|.
void FakeBluetoothMediaClient::RegisterEndpoint(
    const dbus::ObjectPath& object_path,
    const dbus::ObjectPath& endpoint_path,
    const EndpointProperties& properties,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run("org.bluez.NotImplemented", "");
}

void FakeBluetoothMediaClient::UnregisterEndpoint(
    const dbus::ObjectPath& object_path,
    const dbus::ObjectPath& endpoint_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run("org.bluez.NotImplemented", "");
}

}  // namespace chromeos
