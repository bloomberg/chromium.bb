// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_nfc_record_client.h"

#include "base/logging.h"

// TODO(armansito): For now, this class doesn't do anything. Implement fake
// behavior in conjunction with unit tests while implementing the src/device
// layer.

namespace chromeos {

FakeNfcRecordClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : NfcRecordClient::Properties(NULL, callback) {
}

FakeNfcRecordClient::Properties::~Properties() {
}

void FakeNfcRecordClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeNfcRecordClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeNfcRecordClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  callback.Run(false);
}

FakeNfcRecordClient::FakeNfcRecordClient() {
  VLOG(1) << "Creating FakeNfcRecordClient";
}

FakeNfcRecordClient::~FakeNfcRecordClient() {
}

void FakeNfcRecordClient::Init(dbus::Bus* bus) {
}

void FakeNfcRecordClient::AddObserver(Observer* observer) {
}

void FakeNfcRecordClient::RemoveObserver(Observer* observer) {
}

FakeNfcRecordClient::Properties*
FakeNfcRecordClient::GetProperties(const dbus::ObjectPath& object_path) {
  return NULL;
}

}  // namespace chromeos
