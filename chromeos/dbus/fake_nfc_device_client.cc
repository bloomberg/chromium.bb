// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_nfc_device_client.h"

#include "base/logging.h"
#include "dbus/object_path.h"

// TODO(armansito): For now, this class doesn't do anything. Implement fake
// behavior in conjunction with unit tests while implementing the src/device
// layer.

namespace chromeos {

FakeNfcDeviceClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : NfcDeviceClient::Properties(NULL, callback) {
}

FakeNfcDeviceClient::Properties::~Properties() {
}

void FakeNfcDeviceClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeNfcDeviceClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeNfcDeviceClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  callback.Run(false);
}

FakeNfcDeviceClient::FakeNfcDeviceClient() {
  VLOG(1) << "Creating FakeNfcDeviceClient";
}

FakeNfcDeviceClient::~FakeNfcDeviceClient() {
}

void FakeNfcDeviceClient::Init(dbus::Bus* bus) {
}

void FakeNfcDeviceClient::AddObserver(Observer* observer) {
}

void FakeNfcDeviceClient::RemoveObserver(Observer* observer) {
}

FakeNfcDeviceClient::Properties*
FakeNfcDeviceClient::GetProperties(const dbus::ObjectPath& object_path) {
  return NULL;
}

void FakeNfcDeviceClient::Push(
    const dbus::ObjectPath& object_path,
    const base::DictionaryValue& attributes,
    const base::Closure& callback,
    const nfc_client_helpers::ErrorCallback& error_callback) {
  VLOG(1) << "FakeNfcDeviceClient::Write called. Nothing happened.";
}

}  // namespace chromeos
