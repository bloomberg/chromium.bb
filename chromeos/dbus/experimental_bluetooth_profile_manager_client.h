// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_EXPERIMENTAL_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_EXPERIMENTAL_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "dbus/object_path.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// ExperimentalBluetoothProfileManagerClient is used to communicate with
// Bluetooth profile manager objects.
class CHROMEOS_EXPORT ExperimentalBluetoothProfileManagerClient {
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
    std::string name;

    // Primary service class UUID (if different from the actual UUID)
    std::string service;

    // Role.
    enum ProfileRole role;

    // RFCOMM channel number.
    uint16 channel;

    // PSM number.
    uint16 psm;

    // Pairing is required before connections will be established.
    bool require_authentication;

    // Request authorization before connections will be established.
    bool require_authorization;

    // Force connections when a remote device is connected.
    bool auto_connect;

    // Manual SDP record.
    std::string service_record;

    // Profile version.
    uint16 version;

    // Profile features.
    uint16 features;
  };

  virtual ~ExperimentalBluetoothProfileManagerClient();

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
  static ExperimentalBluetoothProfileManagerClient* Create(
      DBusClientImplementationType type,
      dbus::Bus* bus);

  // Constants used to indicate exceptional error conditions.
  static const char kNoResponseError[];

 protected:
  ExperimentalBluetoothProfileManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ExperimentalBluetoothProfileManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_EXPERIMENTAL_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_
