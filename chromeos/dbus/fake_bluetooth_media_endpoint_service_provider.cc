// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_media_endpoint_service_provider.h"

namespace chromeos {

// TODO(mcchou): Add the logic of the behavior.
FakeBluetoothMediaEndpointServiceProvider::
    FakeBluetoothMediaEndpointServiceProvider(
    const dbus::ObjectPath object_path, Delegate* delegate)
    : object_path_(object_path) , delegate_(delegate) {
  VLOG(1) << "Create Bluetooth Media Endpoint: " << object_path_.value();
  // TODO(mcchou): Use the FakeBluetoothMediaClient in DBusThreadManager
  // to register the FakeBluetoothMediaEndpoint object.
}

FakeBluetoothMediaEndpointServiceProvider::
    ~FakeBluetoothMediaEndpointServiceProvider() {
  VLOG(1) << "Cleaning up Bluetooth Media Endpoint: " << object_path_.value();
  // TODO(mcchou): Use the FakeBluetoothMediaClient in DBusThreadManager
  // to unregister the FakeBluetoothMediaEndpoint object.
}

void FakeBluetoothMediaEndpointServiceProvider::SetConfiguration(
    const dbus::ObjectPath& transport_path,
    const dbus::MessageReader& properties) {
  VLOG(1) << object_path_.value() << ": SetConfiguration for "
          << transport_path.value();
  delegate_->SetConfiguration(transport_path, properties);
}

void FakeBluetoothMediaEndpointServiceProvider::SelectConfiguration(
    const std::vector<uint8_t>& capabilities,
    const Delegate::SelectConfigurationCallback& callback) {
  VLOG(1) << object_path_.value() << ": SelectConfiguration";
  delegate_->SelectConfiguration(capabilities, callback);
}

void FakeBluetoothMediaEndpointServiceProvider::ClearConfiguration(
    const dbus::ObjectPath& transport_path) {
  VLOG(1) << object_path_.value() << ": ClearConfiguration for"
          << transport_path.value();
  delegate_->ClearConfiguration(transport_path);
}

void FakeBluetoothMediaEndpointServiceProvider::Release() {
  VLOG(1) << object_path_.value() << ": Release";
  delegate_->Release();
}

}  // namespace chromeos
