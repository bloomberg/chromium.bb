// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_CHROMEOS_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_profile_manager_client.h"
#include "chromeos/dbus/bluetooth_profile_service_provider.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_profile.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace dbus {

class FileDescriptor;

}  // namespace dbus

namespace chromeos {

// The BluetoothProfileChromeOS class implements BluetoothProfile for the
// Chrome OS platform.
class CHROMEOS_EXPORT BluetoothProfileChromeOS
    : public device::BluetoothProfile,
      public device::BluetoothAdapter::Observer,
      public BluetoothProfileServiceProvider::Delegate {
 public:
  // BluetoothProfile override.
  virtual void Unregister() OVERRIDE;
  virtual void SetConnectionCallback(
      const ConnectionCallback& callback) OVERRIDE;

  // Return the UUID of the profile.
  const device::BluetoothUUID& uuid() const { return uuid_; }

 private:
  friend class BluetoothProfile;

  BluetoothProfileChromeOS();
  virtual ~BluetoothProfileChromeOS();

  // Called by BluetoothProfile::Register to initialize the profile object
  // asynchronously. |uuid|, |options| and |callback| are the arguments to
  // BluetoothProfile::Register.
  void Init(const device::BluetoothUUID& uuid,
            const device::BluetoothProfile::Options& options,
            const ProfileCallback& callback);

  // BluetoothProfileServiceProvider::Delegate override.
  virtual void Release() OVERRIDE;
  virtual void NewConnection(
      const dbus::ObjectPath& device_path,
      scoped_ptr<dbus::FileDescriptor> fd,
      const BluetoothProfileServiceProvider::Delegate::Options& options,
      const ConfirmationCallback& callback) OVERRIDE;
  virtual void RequestDisconnection(
      const dbus::ObjectPath& device_path,
      const ConfirmationCallback& callback) OVERRIDE;
  virtual void Cancel() OVERRIDE;

  // device::BluetoothAdapter::Observer override.
  virtual void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                     bool present) OVERRIDE;

  // Called by dbus:: on completion of the D-Bus method call to register the
  // profile object as a result of the adapter becoming present.
  void OnInternalRegisterProfile();
  void OnInternalRegisterProfileError(const std::string& error_name,
                                      const std::string& error_message);

  // Internal method run to get the adapter object during initialization.
  void OnGetAdapter(const ProfileCallback& callback,
                    scoped_refptr<device::BluetoothAdapter> adapter);

  // Called by dbus:: on completion of the D-Bus method call to register the
  // profile object during initialization.
  void OnRegisterProfile(const ProfileCallback& callback);
  void OnRegisterProfileError(const ProfileCallback& callback,
                              const std::string& error_name,
                              const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to unregister
  // the profile object.
  void OnUnregisterProfile();
  void OnUnregisterProfileError(const std::string& error_name,
                                const std::string& error_message);

  // Method run once the file descriptor has been validated in order to get
  // the device object to be passed to the connection callback.
  //
  // The |fd| argument is moved compared to the NewConnection() call since it
  // becomes the result of a PostTaskAndReplyWithResult() call.
  void OnCheckValidity(
      const dbus::ObjectPath& device_path,
      const BluetoothProfileServiceProvider::Delegate::Options& options,
      const ConfirmationCallback& callback,
      scoped_ptr<dbus::FileDescriptor> fd);

  // UUID of the profile passed during initialization.
  device::BluetoothUUID uuid_;

  // Copy of the profile options passed during initialization.
  BluetoothProfileManagerClient::Options options_;

  // Object path of the local profile D-Bus object.
  dbus::ObjectPath object_path_;

  // Local profile D-Bus object used for receiving profile delegate methods
  // from BlueZ.
  scoped_ptr<BluetoothProfileServiceProvider> profile_;

  // Reference to the adapter object, the profile is re-registered when the
  // adapter changes.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Callback used on both outgoing and incoming connections to pass the
  // connected socket to profile object owner.
  ConnectionCallback connection_callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothProfileChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothProfileChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_CHROMEOS_H_
