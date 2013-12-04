// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_nfc_tag_client.h"

#include "base/logging.h"
#include "dbus/object_path.h"

// TODO(armansito): For now, this class doesn't do anything. Implement fake
// behavior in conjunction with unit tests while implementing the src/device
// layer.

namespace chromeos {

FakeNfcTagClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : NfcTagClient::Properties(NULL, callback) {
}

FakeNfcTagClient::Properties::~Properties() {
}

void FakeNfcTagClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeNfcTagClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeNfcTagClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  callback.Run(false);
}

FakeNfcTagClient::FakeNfcTagClient() {
  VLOG(1) << "Creating FakeNfcTagClient";
}

FakeNfcTagClient::~FakeNfcTagClient() {
}

void FakeNfcTagClient::Init(dbus::Bus* bus) {
}

void FakeNfcTagClient::AddObserver(Observer* observer) {
}

void FakeNfcTagClient::RemoveObserver(Observer* observer) {
}

FakeNfcTagClient::Properties*
FakeNfcTagClient::GetProperties(const dbus::ObjectPath& object_path) {
  return NULL;
}

void FakeNfcTagClient::Write(
    const dbus::ObjectPath& object_path,
    const base::DictionaryValue& attributes,
    const base::Closure& callback,
    const nfc_client_helpers::ErrorCallback& error_callback) {
  VLOG(1) << "FakeNfcTagClient::Write called. Nothing happened.";
}

}  // namespace chromeos
