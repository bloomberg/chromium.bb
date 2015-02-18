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

  // Makes the transport valid/invalid for a given media endpoint path. The
  // transport object is assigned to the given endpoint if valid is true, false
  // otherwise.
  void SetValid(const dbus::ObjectPath& endpoint_path, bool valid);

 private:
  // Property callback passed while a Properties structure is created.
  void OnPropertyChanged(const std::string& property_name);

  // The path of the media transport object.
  dbus::ObjectPath object_path_;

  // Indicates whether endpoints are assigned with the transport object.
  std::map<dbus::ObjectPath, bool> endpoints_;

  // The fake property set of the media transport object.
  scoped_ptr<Properties> properties_;

  ObserverList<BluetoothMediaTransportClient::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothMediaTransportClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_TRANSPORT_CLIENT_H_
