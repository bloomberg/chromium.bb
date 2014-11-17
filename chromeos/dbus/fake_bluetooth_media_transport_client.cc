// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_media_transport_client.h"

namespace {

// TODO(mcchou): Remove this constants once it is in cros_system_api.
const char kBluetoothMediaTransportInterface[] = "org.bluez.MediaTransport1";
const char kNotImplemented[] = "org.bluez.NotImplemented";

}  // namespace

namespace chromeos {

FakeBluetoothMediaTransportClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothMediaTransportClient::Properties(
        nullptr,
        kBluetoothMediaTransportInterface,
        callback) {
}

FakeBluetoothMediaTransportClient::Properties::~Properties() {
}

void FakeBluetoothMediaTransportClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get" << property->name();
  callback.Run(false);
}

void FakeBluetoothMediaTransportClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeBluetoothMediaTransportClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set" << property->name();
  callback.Run(false);
}

FakeBluetoothMediaTransportClient::FakeBluetoothMediaTransportClient() {
}

FakeBluetoothMediaTransportClient::~FakeBluetoothMediaTransportClient() {
}

void FakeBluetoothMediaTransportClient::Init(dbus::Bus* bus) {
}

FakeBluetoothMediaTransportClient::Properties*
    FakeBluetoothMediaTransportClient::GetProperties(
    const dbus::ObjectPath& object_path) {
  return nullptr;
}

void FakeBluetoothMediaTransportClient::Acquire(
    const dbus::ObjectPath& object_path,
    const AcquireCallback& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run(kNotImplemented, "");
}

void FakeBluetoothMediaTransportClient::TryAcquire(
    const dbus::ObjectPath& object_path,
    const AcquireCallback& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run(kNotImplemented, "");
}

void FakeBluetoothMediaTransportClient::Release(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run(kNotImplemented, "");
}

}  // namespace chromeos
