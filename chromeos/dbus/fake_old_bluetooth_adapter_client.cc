// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_old_bluetooth_adapter_client.h"

namespace chromeos {

FakeOldBluetoothAdapterClient::Properties::Properties(
    const PropertyChangedCallback& callback) :
  BluetoothAdapterClient::Properties(NULL, callback) {
}

FakeOldBluetoothAdapterClient::Properties::~Properties() {
}

void FakeOldBluetoothAdapterClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1)<< "Get " << property->name();
  callback.Run(false);
}

void FakeOldBluetoothAdapterClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeOldBluetoothAdapterClient::Properties::Set(
    dbus::PropertyBase *property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  if (property->name() == "Powered") {
    property->ReplaceValueWithSetValue();
    callback.Run(true);
  } else {
    callback.Run(false);
  }
}

FakeOldBluetoothAdapterClient::FakeOldBluetoothAdapterClient() {
  properties_.reset(new Properties(base::Bind(
      &FakeOldBluetoothAdapterClient::OnPropertyChanged,
      base::Unretained(this))));

  properties_->address.ReplaceValue("hci0");
  properties_->name.ReplaceValue("Fake Adapter");
  properties_->pairable.ReplaceValue(true);

  std::vector<dbus::ObjectPath> devices;
  devices.push_back(dbus::ObjectPath("/fake/hci0/dev0"));
  properties_->devices.ReplaceValue(devices);
}

FakeOldBluetoothAdapterClient::~FakeOldBluetoothAdapterClient() {
}

void FakeOldBluetoothAdapterClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeOldBluetoothAdapterClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

FakeOldBluetoothAdapterClient::Properties*
FakeOldBluetoothAdapterClient::GetProperties(
    const dbus::ObjectPath& object_path) {
  VLOG(1) << "GetProperties: " << object_path.value();
  if (object_path.value() == "/fake/hci0")
    return properties_.get();
  else
    return NULL;
}

void FakeOldBluetoothAdapterClient::RequestSession(
    const dbus::ObjectPath& object_path,
    const AdapterCallback& callback) {
  VLOG(1) << "RequestSession: " << object_path.value();
  callback.Run(object_path, false);
}

void FakeOldBluetoothAdapterClient::ReleaseSession(
    const dbus::ObjectPath& object_path,
    const AdapterCallback& callback) {
  VLOG(1) << "ReleaseSession: " << object_path.value();
  callback.Run(object_path, false);
}

void FakeOldBluetoothAdapterClient::StartDiscovery(
    const dbus::ObjectPath& object_path,
    const AdapterCallback& callback) {
  VLOG(1) << "StartDiscovery: " << object_path.value();
  callback.Run(object_path, false);
}

void FakeOldBluetoothAdapterClient::StopDiscovery(
    const dbus::ObjectPath & object_path,
    const AdapterCallback& callback) {
  VLOG(1) << "StopDiscovery: " << object_path.value();
  callback.Run(object_path, false);
}

void FakeOldBluetoothAdapterClient::FindDevice(
    const dbus::ObjectPath& object_path,
    const std::string& address,
    const DeviceCallback& callback) {
  VLOG(1) << "FindDevice: " << object_path.value() << " " << address;
  callback.Run(dbus::ObjectPath(), false);
}

void FakeOldBluetoothAdapterClient::CreateDevice(
    const dbus::ObjectPath& object_path,
    const std::string& address,
    const CreateDeviceCallback& callback,
    const CreateDeviceErrorCallback& error_callback) {
  VLOG(1) << "CreateDevice: " << object_path.value() << " " << address;
  error_callback.Run("", "");
}

void FakeOldBluetoothAdapterClient::CreatePairedDevice(
    const dbus::ObjectPath& object_path,
    const std::string& address,
    const dbus::ObjectPath& agent_path,
    const std::string& capability,
    const CreateDeviceCallback& callback,
    const CreateDeviceErrorCallback& error_callback) {
  VLOG(1) << "CreatePairedDevice: " << object_path.value() << " " << address
          << " " << agent_path.value() << " " << capability;
  error_callback.Run("", "");
}

void FakeOldBluetoothAdapterClient::CancelDeviceCreation(
    const dbus::ObjectPath& object_path,
    const std::string& address,
    const AdapterCallback& callback) {
  VLOG(1) << "CancelDeviceCreation: " << object_path.value()
          << " " << address;
  callback.Run(object_path, false);
}

void FakeOldBluetoothAdapterClient::RemoveDevice(
    const dbus::ObjectPath& object_path,
    const dbus::ObjectPath& device_path,
    const AdapterCallback& callback) {
  VLOG(1) << "RemoveDevice: " << object_path.value()
          << " " << device_path.value();
  callback.Run(object_path, false);
}

void FakeOldBluetoothAdapterClient::RegisterAgent(
    const dbus::ObjectPath& object_path,
    const dbus::ObjectPath& agent_path,
    const std::string& capability,
    const AdapterCallback& callback) {
  VLOG(1) << "RegisterAgent: " << object_path.value()
          << " " << agent_path.value();
  callback.Run(object_path, false);
}

void FakeOldBluetoothAdapterClient::UnregisterAgent(
    const dbus::ObjectPath& object_path,
    const dbus::ObjectPath& agent_path,
    const AdapterCallback& callback) {
  VLOG(1) << "UnregisterAgent: " << object_path.value()
          << " " << agent_path.value();
  callback.Run(object_path, false);
}

void FakeOldBluetoothAdapterClient::OnPropertyChanged(
    const std::string&property_name) {
  FOR_EACH_OBSERVER(BluetoothAdapterClient::Observer, observers_,
                    AdapterPropertyChanged(dbus::ObjectPath("/fake/hci0"),
                                           property_name));
}

} // namespace chromeos
