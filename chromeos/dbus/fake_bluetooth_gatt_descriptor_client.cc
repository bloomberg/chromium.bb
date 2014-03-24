// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_gatt_descriptor_client.h"

#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

FakeBluetoothGattDescriptorClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothGattDescriptorClient::Properties(
          NULL,
          bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface,
          callback) {
}

FakeBluetoothGattDescriptorClient::Properties::~Properties() {
}

void FakeBluetoothGattDescriptorClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeBluetoothGattDescriptorClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeBluetoothGattDescriptorClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);

  // TODO(armansito): Setting the "Value" property should be allowed based
  // on permissions.
}

FakeBluetoothGattDescriptorClient::FakeBluetoothGattDescriptorClient() {
}

FakeBluetoothGattDescriptorClient::~FakeBluetoothGattDescriptorClient() {
}

void FakeBluetoothGattDescriptorClient::Init(dbus::Bus* bus) {
}

void FakeBluetoothGattDescriptorClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothGattDescriptorClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath>
FakeBluetoothGattDescriptorClient::GetDescriptors() {
  return std::vector<dbus::ObjectPath>();
}

FakeBluetoothGattDescriptorClient::Properties*
FakeBluetoothGattDescriptorClient::GetProperties(
    const dbus::ObjectPath& object_path) {
  return NULL;
}

}  // namespace chromeos
