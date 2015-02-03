// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_media_client.h"

#include <string>

#include "chromeos/dbus/fake_bluetooth_adapter_client.h"

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

void FakeBluetoothMediaClient::RegisterEndpoint(
    const ObjectPath& object_path,
    const ObjectPath& endpoint_path,
    const EndpointProperties& properties,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) <<  "RegisterEndpoint: " << endpoint_path.value();

  // The object paths of the media client and adapter client should be the same.
  if (object_path != ObjectPath(FakeBluetoothAdapterClient::kAdapterPath) ||
      properties.uuid != BluetoothMediaClient::kBluetoothAudioSinkUUID ||
      properties.codec != kDefaultCodec ||
      properties.capabilities.empty()) {
    error_callback.Run(kInvalidArgumentsError, "");
    return;
  }
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

}  // namespace chromeos
