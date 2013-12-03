// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_PROFILE_SERVICE_PROVIDER_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_PROFILE_SERVICE_PROVIDER_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_profile_service_provider.h"
#include "dbus/file_descriptor.h"
#include "dbus/object_path.h"

namespace chromeos {

// FakeBluetoothProfileServiceProvider simulates the behavior of a local
// Bluetooth agent object and is used both in test cases in place of a
// mock and on the Linux desktop.
class CHROMEOS_EXPORT FakeBluetoothProfileServiceProvider
    : public BluetoothProfileServiceProvider {
 public:
  FakeBluetoothProfileServiceProvider(const dbus::ObjectPath& object_path,
                                      Delegate *delegate);
  virtual ~FakeBluetoothProfileServiceProvider();

  // Each of these calls the equivalent
  // BluetoothProfileServiceProvider::Delegate method on the object passed on
  // construction.
  virtual void Release();
  virtual void NewConnection(
      const dbus::ObjectPath& device_path,
      scoped_ptr<dbus::FileDescriptor> fd,
      const Delegate::Options& options,
      const Delegate::ConfirmationCallback& callback);
  virtual void RequestDisconnection(
      const dbus::ObjectPath& device_path,
      const Delegate::ConfirmationCallback& callback);
  virtual void Cancel();

 private:
  friend class FakeBluetoothProfileManagerClient;

  // D-Bus object path we are faking.
  dbus::ObjectPath object_path_;

  // All incoming method calls are passed on to the Delegate and a callback
  // passed to generate the reply. |delegate_| is generally the object that
  // owns this one, and must outlive it.
  Delegate* delegate_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_PROFILE_SERVICE_PROVIDER_H_
