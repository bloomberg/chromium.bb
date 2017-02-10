// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_AUTH_POLICY_CLIENT_H_
#define CHROMEOS_DBUS_AUTH_POLICY_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/authpolicy/active_directory_account_data.pb.h"
#include "chromeos/dbus/dbus_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

class AccountId;

namespace chromeos {

// AuthPolicyClient is used to communicate with the org.chromium.AuthPolicy
// sevice. All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class CHROMEOS_EXPORT AuthPolicyClient : public DBusClient {
 public:
  // |user_id| is a unique id for the users. Using objectGUID from Active
  // Directory server.
  using AuthCallback = base::Callback<void(
      authpolicy::ErrorType error,
      const authpolicy::ActiveDirectoryAccountData& account_data)>;
  using JoinCallback = base::Callback<void(authpolicy::ErrorType error)>;
  using RefreshPolicyCallback = base::Callback<void(bool success)>;

  ~AuthPolicyClient() override;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static AuthPolicyClient* Create();

  // Calls JoinADDomain. It runs "net ads join ..." which joins machine to
  // Active directory domain.
  // |machine_name| is a name for a local machine. |user_principal_name|,
  // |password_fd| are credentials of the Active directory account which has
  // right to join the machine to the domain. |password_fd| is a file descriptor
  // password is read from. The caller should close it after the call.
  // |callback| is called after getting (or failing to get) D-BUS response.
  virtual void JoinAdDomain(const std::string& machine_name,
                            const std::string& user_principal_name,
                            int password_fd,
                            const JoinCallback& callback) = 0;

  // Calls AuthenticateUser. It runs "kinit <user_principal_name> .. " which
  // does kerberos authentication against Active Directory server.
  // |password_fd| is similar to the one in the JoinAdDomain.
  // |callback| is called after getting (or failing to get) D-BUS response.
  virtual void AuthenticateUser(const std::string& user_principal_name,
                                int password_fd,
                                const AuthCallback& callback) = 0;

  // Calls RefreshDevicePolicy - handle policy for the device.
  // Fetch GPO files from Active directory server, parse it, encode it into
  // protobuf and send to SessionManager. Callback is called after that.
  virtual void RefreshDevicePolicy(const RefreshPolicyCallback& callback) = 0;

  // Calls RefreshUserPolicy - handle policy for the user specified by
  // |account_id|. Similar to RefreshDevicePolicy.
  virtual void RefreshUserPolicy(const AccountId& account_id,
                                 const RefreshPolicyCallback& callback) = 0;

 protected:
  // Create() should be used instead.
  AuthPolicyClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthPolicyClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_AUTH_POLICY_CLIENT_H_
