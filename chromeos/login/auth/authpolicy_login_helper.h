// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_AUTHPOLICY_LOGIN_HELPER_H_
#define CHROMEOS_LOGIN_AUTH_AUTHPOLICY_LOGIN_HELPER_H_

#include <string>

#include "base/callback.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/auth_policy_client.h"

namespace chromeos {

// Helper class to use AuthPolicyClient. For Active Directory domain join and
// authenticate users this class should be used instead of AuthPolicyClient.
// Allows canceling all pending calls and restarting AuthPolicy service. Used
// for enrollment and login UI to proper cancel the flows.
class CHROMEOS_EXPORT AuthPolicyLoginHelper {
 public:
  using AuthCallback = AuthPolicyClient::AuthCallback;
  using JoinCallback = AuthPolicyClient::JoinCallback;

  AuthPolicyLoginHelper();
  ~AuthPolicyLoginHelper();

  // Try to get Kerberos TGT. To get an error code of this call one should use
  // last_auth_error_ returned from AuthPolicyClient::GetUserStatus afterwards.
  // (see https://crbug.com/710452).
  static void TryAuthenticateUser(const std::string& username,
                                  const std::string& object_guid,
                                  const std::string& password);

  // Restarts AuthPolicy service.
  static void Restart();

  // See AuthPolicyClient::JoinAdDomain.
  void JoinAdDomain(const std::string& machine_name,
                    const std::string& username,
                    const std::string& password,
                    JoinCallback callback);

  // See AuthPolicyClient::AuthenticateUser.
  void AuthenticateUser(const std::string& username,
                        const std::string& object_guid,
                        const std::string& password,
                        AuthCallback callback);

  // Cancel pending requests and restarts AuthPolicy service.
  void CancelRequestsAndRestart();

 private:
  // Called from AuthPolicyClient::JoinAdDomain.
  void OnJoinCallback(JoinCallback callback, authpolicy::ErrorType error);

  // Called from AuthPolicyClient::AuthenticateUser.
  void OnAuthCallback(
      AuthCallback callback,
      authpolicy::ErrorType error,
      const authpolicy::ActiveDirectoryAccountInfo& account_info);

  base::WeakPtrFactory<AuthPolicyLoginHelper> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(AuthPolicyLoginHelper);
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_AUTHPOLICY_LOGIN_HELPER_H_
