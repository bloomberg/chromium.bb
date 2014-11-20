// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_ENDPOINT_SERVICE_PROVIDER_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_ENDPOINT_SERVICE_PROVIDER_H_

#include <vector>

#include "base/logging.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_media_endpoint_service_provider.h"
#include "dbus/object_path.h"

namespace chromeos {

// FakeBluetoothMediaEndpointServiceProvider simulates the behavior of a local
// Bluetooth Media Endpoint object.
// TODO(mcchou): Add the logic of the behavior.
class CHROMEOS_EXPORT FakeBluetoothMediaEndpointServiceProvider
    : public BluetoothMediaEndpointServiceProvider {
 public:
  FakeBluetoothMediaEndpointServiceProvider(const dbus::ObjectPath object_path,
                                            Delegate* delegate);
  ~FakeBluetoothMediaEndpointServiceProvider() override;

  // Each of these calls the equivalent BluetoothMediaEnpointServiceProvider::
  // Delegate method on the object passed on construction.
  void SetConfiguration(const dbus::ObjectPath& transport_path,
                        const dbus::MessageReader& properties);
  void SelectConfiguration(
      const std::vector<uint8_t>& capabilities,
      const Delegate::SelectConfigurationCallback& callback);
  void ClearConfiguration(const dbus::ObjectPath& transport_path);
  void Release();

 private:
  // The fake D-Bus object path.
  dbus::ObjectPath object_path_;

  // All incoming method calls are passed to |delegate_|. |callback| passed to
  // |delegate_| will generate the response for those methods which have
  // non-void return.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothMediaEndpointServiceProvider);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_MEDIA_ENDPOINT_SERVICE_PROVIDER_H_
