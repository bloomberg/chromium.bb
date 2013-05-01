// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_old_bluetooth_manager_client.h"

namespace chromeos {

FakeOldBluetoothManagerClient::Properties::Properties(
    const PropertyChangedCallback& callback) :
  BluetoothManagerClient::Properties(NULL, callback) {
}

FakeOldBluetoothManagerClient::Properties::~Properties() {
}

void FakeOldBluetoothManagerClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1)<< "Get " << property->name();
  callback.Run(false);
}

void FakeOldBluetoothManagerClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeOldBluetoothManagerClient::Properties::Set(
    dbus::PropertyBase*property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  callback.Run(false);
}

FakeOldBluetoothManagerClient::FakeOldBluetoothManagerClient() {
  properties_.reset(new Properties(base::Bind(
      &FakeOldBluetoothManagerClient::OnPropertyChanged,
      base::Unretained(this))));

  std::vector<dbus::ObjectPath> adapters;
  adapters.push_back(dbus::ObjectPath("/fake/hci0"));
  properties_->adapters.ReplaceValue(adapters);
}

FakeOldBluetoothManagerClient::~FakeOldBluetoothManagerClient() {
}

void FakeOldBluetoothManagerClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeOldBluetoothManagerClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

FakeOldBluetoothManagerClient::Properties*
FakeOldBluetoothManagerClient::GetProperties() {
  VLOG(1) << "GetProperties";
  return properties_.get();
}

void FakeOldBluetoothManagerClient::DefaultAdapter(
    const AdapterCallback& callback) {
  VLOG(1) << "DefaultAdapter.";
  callback.Run(dbus::ObjectPath("/fake/hci0"), true);
}

void FakeOldBluetoothManagerClient::FindAdapter(
    const std::string& address,
    const AdapterCallback& callback) {
  VLOG(1) << "FindAdapter: " << address;
  if (address == "hci0")
    callback.Run(dbus::ObjectPath("/fake/hci0"), true);
  else
    callback.Run(dbus::ObjectPath(), false);
}

void FakeOldBluetoothManagerClient::OnPropertyChanged(
    const std::string& property_name) {
  FOR_EACH_OBSERVER(BluetoothManagerClient::Observer, observers_,
                    ManagerPropertyChanged(property_name));
}

} // namespace chromeos
