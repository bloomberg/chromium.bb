// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "dbus/object_path.h"

namespace chromeos {

// BluetoothProfileManagerClient is used to communicate with the profile
// manager object of the Bluetooth daemon.
class CHROMEOS_EXPORT BluetoothProfileManagerClient : public DBusClient {
 public:
  // Species the role of the object within the profile. SYMMETRIC should be
  // usually used unless the profile requires you specify as a CLIENT or as a
  // SERVER.
  enum ProfileRole {
    SYMMETRIC,
    CLIENT,
    SERVER
  };

  // Options used to register a Profile object.
  struct CHROMEOS_EXPORT Options {
    Options();
    ~Options();

    // Human readable name for the profile.
    scoped_ptr<std::string> name;

    // Primary service class UUID (if different from the actual UUID)
    scoped_ptr<std::string> service;

    // Role.
    enum ProfileRole role;

    // RFCOMM channel number.
    scoped_ptr<uint16> channel;

    // PSM number.
    scoped_ptr<uint16> psm;

    // Pairing is required before connections will be established.
    scoped_ptr<bool> require_authentication;

    // Request authorization before connections will be established.
    scoped_ptr<bool> require_authorization;

    // Force connections when a remote device is connected.
    scoped_ptr<bool> auto_connect;

    // Manual SDP record.
    scoped_ptr<std::string> service_record;

    // Profile version.
    scoped_ptr<uint16> version;

    // Profile features.
    scoped_ptr<uint16> features;
  };

  virtual ~BluetoothProfileManagerClient();

  // The ErrorCallback is used by adapter methods to indicate failure.
  // It receives two arguments: the name of the error in |error_name| and
  // an optional message in |error_message|.
  typedef base::Callback<void(const std::string& error_name,
                              const std::string& error_message)> ErrorCallback;

  // Registers a profile implementation within the local process at the
  // D-bus object path |profile_path| with the remote profile manager.
  // |uuid| specifies the identifier of the profile and |options| the way in
  // which the profile is implemented.
  virtual void RegisterProfile(const dbus::ObjectPath& profile_path,
                               const std::string& uuid,
                               const Options& options,
                               const base::Closure& callback,
                               const ErrorCallback& error_callback) = 0;

  // Unregisters the profile with the D-Bus object path |agent_path| from the
  // remote profile manager.
  virtual void UnregisterProfile(const dbus::ObjectPath& profile_path,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) = 0;


  // Creates the instance.
  static BluetoothProfileManagerClient* Create();

  // Constants used to indicate exceptional error conditions.
  static const char kNoResponseError[];

 protected:
  BluetoothProfileManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothProfileManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_
