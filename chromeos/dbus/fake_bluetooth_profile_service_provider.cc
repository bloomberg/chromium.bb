// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_profile_service_provider.h"

#include "dbus/object_path.h"

namespace chromeos {

FakeBluetoothProfileServiceProvider::FakeBluetoothProfileServiceProvider(
    const dbus::ObjectPath& object_path,
    Delegate* delegate)
    : object_path_(object_path),
      delegate_(delegate) {
  VLOG(1) << "Creating Bluetooth Profile: " << object_path_.value();
}

FakeBluetoothProfileServiceProvider::~FakeBluetoothProfileServiceProvider() {
  VLOG(1) << "Cleaning up Bluetooth Profile: " << object_path_.value();
}

void FakeBluetoothProfileServiceProvider::Release() {
  VLOG(1) << object_path_.value() << ": Release";
  delegate_->Release();
}

void FakeBluetoothProfileServiceProvider::NewConnection(
    const dbus::ObjectPath& device_path,
    dbus::FileDescriptor* fd,
    const Delegate::Options& options,
    const Delegate::ConfirmationCallback& callback) {
  VLOG(1) << object_path_.value() << ": NewConnection for "
          << device_path.value();
  delegate_->NewConnection(device_path, fd, options, callback);
}

void FakeBluetoothProfileServiceProvider::RequestDisconnection(
    const dbus::ObjectPath& device_path,
    const Delegate::ConfirmationCallback& callback) {
  VLOG(1) << object_path_.value() << ": RequestDisconnection for "
          << device_path.value();
  delegate_->RequestDisconnection(device_path, callback);
}

void FakeBluetoothProfileServiceProvider::Cancel() {
  VLOG(1) << object_path_.value() << ": Cancel";
  delegate_->Cancel();
}

}  // namespace chromeos
