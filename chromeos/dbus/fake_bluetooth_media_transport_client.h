// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_TRANSPORT_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_TRANSPORT_CLIENT_H_

#include "chromeos/dbus/bluetooth_media_transport_client.h"

#include "chromeos/chromeos_export.h"
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

  FakeBluetoothMediaTransportClient();
  ~FakeBluetoothMediaTransportClient() override;

  void Init(dbus::Bus* bus) override;
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

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothMediaTransportClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_TRANSPORT_CLIENT_H_
