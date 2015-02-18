// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_CLIENT_H_

#include <map>

#include "base/callback.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_media_client.h"
#include "dbus/object_path.h"

namespace chromeos {

class CHROMEOS_EXPORT FakeBluetoothMediaClient : public BluetoothMediaClient {
 public:
  // The default codec is SBC(0x00).
  static const uint8_t kDefaultCodec;

  FakeBluetoothMediaClient();
  ~FakeBluetoothMediaClient() override;

  // DBusClient override.
  void Init(dbus::Bus* bus) override;

  // BluetoothMediaClient overrides.
  void AddObserver(BluetoothMediaClient::Observer* observer) override;
  void RemoveObserver(BluetoothMediaClient::Observer* observer) override;
  void RegisterEndpoint(const dbus::ObjectPath& object_path,
                        const dbus::ObjectPath& endpoint_path,
                        const EndpointProperties& properties,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) override;
  void UnregisterEndpoint(const dbus::ObjectPath& object_path,
                          const dbus::ObjectPath& endpoint_path,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) override;

  // Makes the media object visible/invisible to emulate the addition/removal
  // events.
  void SetVisible(bool visible);

  // Sets the registration state for a given media endpoint path.
  void SetEndpointRegistered(const dbus::ObjectPath& endpoint_path,
                             bool registered);

 private:
  // Indicates whether the media object is visible or not.
  bool visible_;

  // The path of the media object.
  dbus::ObjectPath object_path_;

  // Pairs of endpoint paths and bool values indicating whether or not endpoints
  // are registered.
  std::map<dbus::ObjectPath, bool> endpoints_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothMediaClient::Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothMediaClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_CLIENT_H_
