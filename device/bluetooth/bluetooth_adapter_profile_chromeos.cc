// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_profile_chromeos.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chromeos/dbus/bluetooth_profile_service_provider.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace chromeos {

// static
void BluetoothAdapterProfileChromeOS::Register(
    const device::BluetoothUUID& uuid,
    const BluetoothProfileManagerClient::Options& options,
    const ProfileRegisteredCallback& success_callback,
    const BluetoothProfileManagerClient::ErrorCallback& error_callback) {
  scoped_ptr<BluetoothAdapterProfileChromeOS> profile(
      new BluetoothAdapterProfileChromeOS(uuid));

  VLOG(1) << "Registering profile: " << profile->object_path().value();
  const dbus::ObjectPath& object_path = profile->object_path();
  DBusThreadManager::Get()->GetBluetoothProfileManagerClient()->RegisterProfile(
      object_path,
      uuid.canonical_value(),
      options,
      base::Bind(success_callback, base::Passed(&profile)),
      error_callback);
}

BluetoothAdapterProfileChromeOS::BluetoothAdapterProfileChromeOS(
    const device::BluetoothUUID& uuid)
    : uuid_(uuid), weak_ptr_factory_(this) {
  std::string uuid_path;
  base::ReplaceChars(uuid.canonical_value(), ":-", "_", &uuid_path);
  object_path_ =
      dbus::ObjectPath("/org/chromium/bluetooth_profile/" + uuid_path);

  dbus::Bus* system_bus = DBusThreadManager::Get()->GetSystemBus();
  profile_.reset(
      BluetoothProfileServiceProvider::Create(system_bus, object_path_, this));
  DCHECK(profile_.get());
}

BluetoothAdapterProfileChromeOS::~BluetoothAdapterProfileChromeOS() {
}

bool BluetoothAdapterProfileChromeOS::SetDelegate(
    const dbus::ObjectPath& device_path,
    BluetoothProfileServiceProvider::Delegate* delegate) {
  DCHECK(delegate);
  VLOG(1) << "SetDelegate: " << object_path_.value() << " dev "
          << device_path.value();

  if (delegates_.find(device_path.value()) != delegates_.end()) {
    return false;
  }

  delegates_[device_path.value()] = delegate;
  return true;
}

void BluetoothAdapterProfileChromeOS::RemoveDelegate(
    const dbus::ObjectPath& device_path,
    const base::Closure& unregistered_callback) {
  VLOG(1) << object_path_.value() << " dev " << device_path.value()
          << ": RemoveDelegate";

  if (delegates_.find(device_path.value()) == delegates_.end())
    return;

  delegates_.erase(device_path.value());

  if (delegates_.size() != 0)
    return;

  VLOG(1) << device_path.value() << " No delegates left, unregistering.";

  // No users left, release the profile.
  DBusThreadManager::Get()
      ->GetBluetoothProfileManagerClient()
      ->UnregisterProfile(
          object_path_, unregistered_callback,
          base::Bind(&BluetoothAdapterProfileChromeOS::OnUnregisterProfileError,
                     weak_ptr_factory_.GetWeakPtr(), unregistered_callback));
}

void BluetoothAdapterProfileChromeOS::OnUnregisterProfileError(
    const base::Closure& unregistered_callback,
    const std::string& error_name,
    const std::string& error_message) {
  LOG(WARNING) << this->object_path().value()
               << ": Failed to unregister profile: " << error_name << ": "
               << error_message;

  unregistered_callback.Run();
}

// BluetoothProfileServiceProvider::Delegate:
void BluetoothAdapterProfileChromeOS::Released() {
  VLOG(1) << object_path_.value() << ": Release";
}

void BluetoothAdapterProfileChromeOS::NewConnection(
    const dbus::ObjectPath& device_path,
    scoped_ptr<dbus::FileDescriptor> fd,
    const BluetoothProfileServiceProvider::Delegate::Options& options,
    const ConfirmationCallback& callback) {
  dbus::ObjectPath delegate_path = device_path;

  if (delegates_.find(device_path.value()) == delegates_.end())
    delegate_path = dbus::ObjectPath("");

  if (delegates_.find(delegate_path.value()) == delegates_.end()) {
    VLOG(1) << object_path_.value() << ": New connection for device "
            << device_path.value() << " which has no delegates!";
    callback.Run(REJECTED);
    return;
  }

  delegates_[delegate_path.value()]->NewConnection(device_path, fd.Pass(),
                                                   options, callback);
}

void BluetoothAdapterProfileChromeOS::RequestDisconnection(
    const dbus::ObjectPath& device_path,
    const ConfirmationCallback& callback) {
  dbus::ObjectPath delegate_path = device_path;

  if (delegates_.find(device_path.value()) == delegates_.end())
    delegate_path = dbus::ObjectPath("");

  if (delegates_.find(delegate_path.value()) == delegates_.end()) {
    VLOG(1) << object_path_.value() << ": RequestDisconnection for device "
            << device_path.value() << " which has no delegates!";
    return;
  }

  delegates_[delegate_path.value()]->RequestDisconnection(device_path,
                                                          callback);
}

void BluetoothAdapterProfileChromeOS::Cancel() {
  // Cancel() should only go to a delegate accepting connections.
  if (delegates_.find("") == delegates_.end()) {
    VLOG(1) << object_path_.value() << ": Cancel with no delegate!";
    return;
  }

  delegates_[""]->Cancel();
}

}  // namespace chromeos
