// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/experimental_bluetooth_profile_manager_client.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace chromeos {

// FakeBluetoothProfileManagerClient simulates the behavior of the Bluetooth
// Daemon's profile manager object and is used both in test cases in place of a
// mock and on the Linux desktop.
class CHROMEOS_EXPORT FakeBluetoothProfileManagerClient
    : public ExperimentalBluetoothProfileManagerClient {
 public:
  FakeBluetoothProfileManagerClient();
  virtual ~FakeBluetoothProfileManagerClient();

  // ExperimentalBluetoothProfileManagerClient override
  virtual void RegisterProfile(const dbus::ObjectPath& profile_path,
                               const std::string& uuid,
                               const Options& options,
                               const base::Closure& callback,
                               const ErrorCallback& error_callback) OVERRIDE;
  virtual void UnregisterProfile(const dbus::ObjectPath& profile_path,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) OVERRIDE;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_
