// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_CLIENT_H_

#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_media_client.h"
#include "dbus/object_path.h"

namespace chromeos {

class CHROMEOS_EXPORT FakeBluetoothMediaClient : public BluetoothMediaClient {
 public:
  FakeBluetoothMediaClient();
  ~FakeBluetoothMediaClient() override;

  void Init(dbus::Bus* bus) override;
  void RegisterEndpoint(const dbus::ObjectPath& object_path,
                        const dbus::ObjectPath& endpoint_path,
                        const EndpointProperties& properties,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) override;
  void UnregisterEndpoint(const dbus::ObjectPath& object_path,
                          const dbus::ObjectPath& endpoint_path,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothMediaClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_CLIENT_H_
