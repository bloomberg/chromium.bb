// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_nfc_adapter_client.h"

#include "base/logging.h"
#include "dbus/object_path.h"

// TODO(armansito): For now, this class doesn't do anything. Implement fake
// behavior in conjunction with unit tests while implementing the src/device
// layer.

namespace chromeos {

FakeNfcAdapterClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : NfcAdapterClient::Properties(NULL, callback) {
}

FakeNfcAdapterClient::Properties::~Properties() {
}

void FakeNfcAdapterClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeNfcAdapterClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeNfcAdapterClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  callback.Run(false);
}

FakeNfcAdapterClient::FakeNfcAdapterClient() {
  VLOG(1) << "Creating FakeNfcAdapterClient";
}

FakeNfcAdapterClient::~FakeNfcAdapterClient() {
}

void FakeNfcAdapterClient::Init(dbus::Bus* bus) {
}

void FakeNfcAdapterClient::AddObserver(Observer* observer) {
}

void FakeNfcAdapterClient::RemoveObserver(Observer* observer) {
}

FakeNfcAdapterClient::Properties*
FakeNfcAdapterClient::GetProperties(const dbus::ObjectPath& object_path) {
  return NULL;
}

void FakeNfcAdapterClient::StartPollLoop(
    const dbus::ObjectPath& object_path,
    const std::string& mode,
    const base::Closure& callback,
    const nfc_client_helpers::ErrorCallback& error_callback) {
  VLOG(1) << "FakeNfcAdapterClient::StartPollLoop called. Nothing happened.";
}

void FakeNfcAdapterClient::StopPollLoop(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const nfc_client_helpers::ErrorCallback& error_callback) {
  VLOG(1) << "FakeNfcAdapterClient::StopPollLoop called. Nothing happened.";
}

}  // namespace chromeos
