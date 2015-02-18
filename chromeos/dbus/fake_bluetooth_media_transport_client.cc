// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_media_transport_client.h"

#include "base/bind.h"
#include "chromeos/dbus/bluetooth_media_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_media_client.h"

using dbus::ObjectPath;

namespace {

// TODO(mcchou): Remove this constants once it is in cros_system_api.
const char kBluetoothMediaTransportInterface[] = "org.bluez.MediaTransport1";
const char kNotImplemented[] = "org.bluez.NotImplemented";

}  // namespace

namespace chromeos {

// static
const char FakeBluetoothMediaTransportClient::kTransportPath[] =
    "/fake/hci0/dev_00_00_00_00_00_00/fd0";
const char FakeBluetoothMediaTransportClient::kTransportDevicePath[] =
    "/fake/hci0/dev_00_00_00_00_00_00";
const uint8_t FakeBluetoothMediaTransportClient::kTransportCodec = 0x00;
const std::vector<uint8_t>
    FakeBluetoothMediaTransportClient::kTransportConfiguration = {
        0x21, 0x15, 0x33, 0x2C};
const uint16_t FakeBluetoothMediaTransportClient::kTransportDelay = 5;
const uint16_t FakeBluetoothMediaTransportClient::kTransportVolume = 10;

FakeBluetoothMediaTransportClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothMediaTransportClient::Properties(
        nullptr,
        kBluetoothMediaTransportInterface,
        callback) {
}

FakeBluetoothMediaTransportClient::Properties::~Properties() {
}

// dbus::PropertySet overrides.

void FakeBluetoothMediaTransportClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeBluetoothMediaTransportClient::Properties::GetAll() {
  VLOG(1) << "GetAll called.";
}

void FakeBluetoothMediaTransportClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  callback.Run(false);
}

FakeBluetoothMediaTransportClient::FakeBluetoothMediaTransportClient()
    : object_path_(ObjectPath(kTransportPath)) {
  // TODO(mcchou): Multiple endpoints are sharing one property set for now.
  // Add property sets accordingly to separate the
  // MediaTransportPropertiesChanged events for different endpoints.

  // Sets fake property set with default values.
  properties_.reset(new Properties(
      base::Bind(&FakeBluetoothMediaTransportClient::OnPropertyChanged,
                 base::Unretained(this))));
  properties_->device.ReplaceValue(ObjectPath(kTransportDevicePath));
  properties_->uuid.ReplaceValue(BluetoothMediaClient::kBluetoothAudioSinkUUID);
  properties_->codec.ReplaceValue(kTransportCodec);
  properties_->configuration.ReplaceValue(kTransportConfiguration);
  properties_->state.ReplaceValue(BluetoothMediaTransportClient::kStateIdle);
  properties_->delay.ReplaceValue(kTransportDelay);
  properties_->volume.ReplaceValue(kTransportVolume);
}

FakeBluetoothMediaTransportClient::~FakeBluetoothMediaTransportClient() {
}

// DBusClient override.
void FakeBluetoothMediaTransportClient::Init(dbus::Bus* bus) {
}

// BluetoothMediaTransportClient overrides.

void FakeBluetoothMediaTransportClient::AddObserver(
    BluetoothMediaTransportClient::Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothMediaTransportClient::RemoveObserver(
    BluetoothMediaTransportClient::Observer* observer) {
  observers_.RemoveObserver(observer);
}

FakeBluetoothMediaTransportClient::Properties*
    FakeBluetoothMediaTransportClient::GetProperties(
        const ObjectPath& object_path) {
  return nullptr;
}

void FakeBluetoothMediaTransportClient::Acquire(
    const ObjectPath& object_path,
    const AcquireCallback& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run(kNotImplemented, "");
}

void FakeBluetoothMediaTransportClient::TryAcquire(
    const ObjectPath& object_path,
    const AcquireCallback& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run(kNotImplemented, "");
}

void FakeBluetoothMediaTransportClient::Release(
    const ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run(kNotImplemented, "");
}

void FakeBluetoothMediaTransportClient::SetValid(
    const ObjectPath& endpoint_path,
    bool valid) {
  if (valid) {
    endpoints_[endpoint_path] = valid;
    return;
  }
  endpoints_.erase(endpoint_path);

  // TODO(mcchou): Since there is only one transport path, all observers will
  // be notified. Shades irrelevant observers.

  // Notifies observers about the state change of the transport.
  FOR_EACH_OBSERVER(BluetoothMediaTransportClient::Observer, observers_,
                    MediaTransportRemoved(object_path_));
}

void FakeBluetoothMediaTransportClient::OnPropertyChanged(
    const std::string& property_name) {
  VLOG(1) << "Property " << property_name << " changed";
}

}  // namespace chromeos
