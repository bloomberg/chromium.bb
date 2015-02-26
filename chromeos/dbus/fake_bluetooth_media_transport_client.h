// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_TRANSPORT_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_TRANSPORT_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_media_transport_client.h"
#include "dbus/object_path.h"

namespace chromeos {

class FakeBluetoothMediaEndpointServiceProvider;

class CHROMEOS_EXPORT FakeBluetoothMediaTransportClient
    : public BluetoothMediaTransportClient {
 public:
  struct Properties : public BluetoothMediaTransportClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    ~Properties() override;

    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override;
    void GetAll() override;
    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override;
  };

  // The default path of the transport object.
  static const char kTransportPath[];

  // The default properties including device, codec, configuration, state, delay
  // and volume, owned by a fake media transport object we emulate.
  static const char kTransportDevicePath[];
  static const uint8_t kTransportCodec;
  static const std::vector<uint8_t> kTransportConfiguration;
  static const uint16_t kTransportDelay;
  static const uint16_t kTransportVolume;

  FakeBluetoothMediaTransportClient();
  ~FakeBluetoothMediaTransportClient() override;

  // DBusClient override.
  void Init(dbus::Bus* bus) override;

  // BluetoothMediaTransportClient override.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  Properties* GetProperties(const dbus::ObjectPath& object_path) override;
  void Acquire(const dbus::ObjectPath& object_path,
               const AcquireCallback& callback,
               const ErrorCallback& error_callback) override;
  void TryAcquire(const dbus::ObjectPath& object_path,
                  const AcquireCallback& callback,
                  const ErrorCallback& error_callback) override;
  void Release(const dbus::ObjectPath& object_path,
               const base::Closure& callback,
               const ErrorCallback& error_callback) override;

  // Makes the transport valid/invalid for a given media endpoint. The transport
  // object is assigned to the given endpoint if valid is true, false
  // otherwise.
  void SetValid(FakeBluetoothMediaEndpointServiceProvider* endpoint,
                bool valid);

  // Set state/volume property to a certain value.
  void SetState(const dbus::ObjectPath& endpoint_path,
                const std::string& state);
  void SetVolume(const dbus::ObjectPath& endpoint_path,
                 const uint16_t& volume);

  // Gets the transport path associated with the given endpoint path.
  dbus::ObjectPath GetTransportPath(const dbus::ObjectPath& endpoint_path);

  // Gets the endpoint path associated with the given transport_path.
  dbus::ObjectPath GetEndpointPath(const dbus::ObjectPath& transport_path);

 private:
  // This class is used for simulating the scenario where each media endpoint
  // has a corresponding transport path and properties. Once an endpoint is
  // assigned with a transport path, an object of Transport is created.
  class Transport {
   public:
    Transport(const dbus::ObjectPath& transport_path,
              Properties* transport_properties);
    ~Transport();

    // An unique transport path.
    dbus::ObjectPath path;

    // The property set bound with |path|.
    scoped_ptr<Properties> properties;
  };

  // Property callback passed while a Properties structure is created.
  void OnPropertyChanged(const std::string& property_name);

  // Map of endpoints with valid transport. Each pair is composed of an endpoint
  // path and a Transport structure containing a transport path and its
  // properties.
  std::map<dbus::ObjectPath, Transport*> endpoint_to_transport_map_;

  // Map of valid transports. Each pair is composed of a transport path as the
  // key and an endpoint path as the value. This map is used to get the
  // corresponding endpoint path when GetProperties() is called.
  std::map<dbus::ObjectPath, dbus::ObjectPath> transport_to_endpoint_map_;

  // The path of the media transport object.
  dbus::ObjectPath object_path_;

  ObserverList<BluetoothMediaTransportClient::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothMediaTransportClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_TRANSPORT_CLIENT_H_
