// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "chromeos/dbus/fake_old_bluetooth_device_client.h"

namespace chromeos {

FakeOldBluetoothDeviceClient::Properties::Properties(
    const PropertyChangedCallback& callback) :
  BluetoothDeviceClient::Properties(NULL, callback) {
}

FakeOldBluetoothDeviceClient::Properties::~Properties() {
}

void FakeOldBluetoothDeviceClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1)<< "Get " << property->name();
  callback.Run(false);
}

void FakeOldBluetoothDeviceClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeOldBluetoothDeviceClient::Properties::Set(
    dbus::PropertyBase *property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  callback.Run(false);
}

FakeOldBluetoothDeviceClient::FakeOldBluetoothDeviceClient() {
  dbus::ObjectPath dev0("/fake/hci0/dev0");

  Properties* properties = new Properties(base::Bind(
      &FakeOldBluetoothDeviceClient::OnPropertyChanged,
      base::Unretained(this),
      dev0));
  properties->address.ReplaceValue("00:11:22:33:44:55");
  properties->name.ReplaceValue("Fake Device");
  properties->paired.ReplaceValue(true);
  properties->trusted.ReplaceValue(true);

  properties_map_[dev0] = properties;
}

FakeOldBluetoothDeviceClient::~FakeOldBluetoothDeviceClient() {
  // Clean up Properties structures
  STLDeleteValues(&properties_map_);
}

void FakeOldBluetoothDeviceClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeOldBluetoothDeviceClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

FakeOldBluetoothDeviceClient::Properties*
FakeOldBluetoothDeviceClient::GetProperties(
    const dbus::ObjectPath& object_path) {
  VLOG(1)<< "GetProperties: " << object_path.value();
  PropertiesMap::iterator iter = properties_map_.find(object_path);
  if (iter != properties_map_.end())
  return iter->second;
  return NULL;
}

void FakeOldBluetoothDeviceClient::DiscoverServices(
    const dbus::ObjectPath& object_path,
    const std::string& pattern,
    const ServicesCallback& callback) {
  VLOG(1) << "DiscoverServices: " << object_path.value() << " " << pattern;

  ServiceMap services;
  callback.Run(object_path, services, false);
}

void FakeOldBluetoothDeviceClient::CancelDiscovery(
    const dbus::ObjectPath& object_path,
    const DeviceCallback& callback) {
  VLOG(1) << "CancelDiscovery: " << object_path.value();
  callback.Run(object_path, false);
}

void FakeOldBluetoothDeviceClient::Disconnect(
    const dbus::ObjectPath& object_path,
    const DeviceCallback& callback) {
  VLOG(1) << "Disconnect: " << object_path.value();
  callback.Run(object_path, false);
}

void FakeOldBluetoothDeviceClient::CreateNode(
    const dbus::ObjectPath& object_path,
    const std::string& uuid,
    const NodeCallback& callback) {
  VLOG(1) << "CreateNode: " << object_path.value() << " " << uuid;
  callback.Run(dbus::ObjectPath(), false);
}

void FakeOldBluetoothDeviceClient::RemoveNode(
    const dbus::ObjectPath& object_path,
    const dbus::ObjectPath& node_path,
    const DeviceCallback& callback) {
  VLOG(1) << "RemoveNode: " << object_path.value()
          << " " << node_path.value();
  callback.Run(object_path, false);
}

void FakeOldBluetoothDeviceClient::OnPropertyChanged(
    dbus::ObjectPath object_path,
    const std::string& property_name) {
  FOR_EACH_OBSERVER(BluetoothDeviceClient::Observer, observers_,
                    DevicePropertyChanged(object_path, property_name));
}

} // namespace chromeos
