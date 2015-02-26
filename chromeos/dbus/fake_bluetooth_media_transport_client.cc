// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_media_transport_client.h"

#include <sstream>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chromeos/dbus/bluetooth_media_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_media_client.h"
#include "chromeos/dbus/fake_bluetooth_media_endpoint_service_provider.h"

using dbus::ObjectPath;

namespace {

// TODO(mcchou): Remove this constants once it is in cros_system_api.
const char kBluetoothMediaTransportInterface[] = "org.bluez.MediaTransport1";
const char kNotImplemented[] = "org.bluez.NotImplemented";

ObjectPath GenerateTransportPath() {
  static unsigned int sequence_number = 0;
  ++sequence_number;
  std::stringstream path;
  path << chromeos::FakeBluetoothAdapterClient::kAdapterPath
       << chromeos::FakeBluetoothMediaTransportClient::kTransportDevicePath
       << "/fd" << sequence_number;
  return ObjectPath(path.str());
}

}  // namespace

namespace chromeos {

// static
const char FakeBluetoothMediaTransportClient::kTransportDevicePath[] =
    "/fake_audio_source";
const uint8_t FakeBluetoothMediaTransportClient::kTransportCodec = 0x00;
const std::vector<uint8_t>
    FakeBluetoothMediaTransportClient::kTransportConfiguration = {
        0x21, 0x15, 0x33, 0x2C};
const uint16_t FakeBluetoothMediaTransportClient::kTransportDelay = 5;
const uint16_t FakeBluetoothMediaTransportClient::kTransportVolume = 50;

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

FakeBluetoothMediaTransportClient::Transport::Transport(
    const ObjectPath& transport_path,
    Properties* transport_properties)
    : path(transport_path) {
  properties.reset(transport_properties);
}

FakeBluetoothMediaTransportClient::Transport::~Transport() {
}

FakeBluetoothMediaTransportClient::FakeBluetoothMediaTransportClient() {
}

FakeBluetoothMediaTransportClient::~FakeBluetoothMediaTransportClient() {
  for (auto& it : endpoint_to_transport_map_)
    delete it.second;
  endpoint_to_transport_map_.clear();
}

// DBusClient override.
void FakeBluetoothMediaTransportClient::Init(dbus::Bus* bus) {
}

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
  ObjectPath endpoint_path = GetEndpointPath(object_path);
  if (!endpoint_path.IsValid() ||
      !ContainsKey(endpoint_to_transport_map_, endpoint_path))
    return nullptr;
  return endpoint_to_transport_map_[endpoint_path]->properties.get();
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
    FakeBluetoothMediaEndpointServiceProvider* endpoint,
    bool valid) {
  FakeBluetoothMediaClient* media = static_cast<FakeBluetoothMediaClient*>(
      DBusThreadManager::Get()->GetBluetoothMediaClient());
  DCHECK(media);

  ObjectPath endpoint_path(endpoint->object_path());
  if (!media->IsRegistered(endpoint_path))
    return;

  if (valid) {
    ObjectPath transport_path = GenerateTransportPath();
    VLOG(1) << "New transport, " << transport_path.value()
            << " is created for endpoint " << endpoint_path.value();

    // Sets the fake property set with default values.
    scoped_ptr<Properties> properties(new Properties(
        base::Bind(&FakeBluetoothMediaTransportClient::OnPropertyChanged,
                   base::Unretained(this))));
    properties->device.ReplaceValue(ObjectPath(kTransportDevicePath));
    properties->uuid.ReplaceValue(
        BluetoothMediaClient::kBluetoothAudioSinkUUID);
    properties->codec.ReplaceValue(kTransportCodec);
    properties->configuration.ReplaceValue(kTransportConfiguration);
    properties->state.ReplaceValue(BluetoothMediaTransportClient::kStateIdle);
    properties->delay.ReplaceValue(kTransportDelay);
    properties->volume.ReplaceValue(kTransportVolume);

    endpoint_to_transport_map_[endpoint_path] =
        new Transport(transport_path, properties.release());
    transport_to_endpoint_map_[transport_path] = endpoint_path;
    return;
  }

  if (!ContainsKey(endpoint_to_transport_map_, endpoint_path))
    return;

  // Notifies observers about the state change of the transport.
  FOR_EACH_OBSERVER(BluetoothMediaTransportClient::Observer, observers_,
                    MediaTransportRemoved(GetTransportPath(endpoint_path)));

  endpoint->ClearConfiguration(GetTransportPath(endpoint_path));
  transport_to_endpoint_map_.erase(GetTransportPath(endpoint_path));
  delete endpoint_to_transport_map_[endpoint_path];
  endpoint_to_transport_map_.erase(endpoint_path);
}

void FakeBluetoothMediaTransportClient::SetState(
    const dbus::ObjectPath& endpoint_path,
    const std::string& state) {
  if (!ContainsKey(endpoint_to_transport_map_, endpoint_path))
    return;

  endpoint_to_transport_map_[endpoint_path]
      ->properties->state.ReplaceValue(state);
  FOR_EACH_OBSERVER(BluetoothMediaTransportClient::Observer, observers_,
                    MediaTransportPropertyChanged(
                        GetTransportPath(endpoint_path),
                        BluetoothMediaTransportClient::kStateProperty));
}

void FakeBluetoothMediaTransportClient::SetVolume(
    const dbus::ObjectPath& endpoint_path,
    const uint16_t& volume) {
  if (!ContainsKey(endpoint_to_transport_map_, endpoint_path))
    return;

  endpoint_to_transport_map_[endpoint_path]->properties->volume.ReplaceValue(
      volume);
  FOR_EACH_OBSERVER(BluetoothMediaTransportClient::Observer, observers_,
                    MediaTransportPropertyChanged(
                        GetTransportPath(endpoint_path),
                        BluetoothMediaTransportClient::kVolumeProperty));
}

ObjectPath FakeBluetoothMediaTransportClient::GetTransportPath(
    const ObjectPath& endpoint_path) {
  if (ContainsKey(endpoint_to_transport_map_, endpoint_path))
    return endpoint_to_transport_map_[endpoint_path]->path;
  return ObjectPath("");
}

ObjectPath FakeBluetoothMediaTransportClient::GetEndpointPath(
    const ObjectPath& transport_path) {
  if (ContainsKey(transport_to_endpoint_map_, transport_path))
    return transport_to_endpoint_map_[transport_path];
  return ObjectPath("");
}

void FakeBluetoothMediaTransportClient::OnPropertyChanged(
    const std::string& property_name) {
  VLOG(1) << "Property " << property_name << " changed";
}

}  // namespace chromeos
