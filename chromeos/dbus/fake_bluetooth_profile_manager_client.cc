// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_profile_manager_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

FakeBluetoothProfileManagerClient::FakeBluetoothProfileManagerClient() {
}

FakeBluetoothProfileManagerClient::~FakeBluetoothProfileManagerClient() {
}

void FakeBluetoothProfileManagerClient::RegisterProfile(
    const dbus::ObjectPath& profile_path,
    const std::string& uuid,
    const Options& options,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "RegisterProfile: " << profile_path.value() << ": " << uuid;
  callback.Run();
}

void FakeBluetoothProfileManagerClient::UnregisterProfile(
    const dbus::ObjectPath& profile_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "UnregisterProfile: " << profile_path.value();
  callback.Run();
}

}  // namespace chromeos
