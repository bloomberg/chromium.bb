// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_AUTH_POLICY_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_AUTH_POLICY_CLIENT_H_

#include <string>

#include "base/macros.h"

#include "chromeos/dbus/auth_policy_client.h"

class AccountId;

namespace chromeos {

class CHROMEOS_EXPORT FakeAuthPolicyClient : public AuthPolicyClient {
 public:
  FakeAuthPolicyClient();
  ~FakeAuthPolicyClient() override;

  // DBusClient overrides.
  void Init(dbus::Bus* bus) override;
  // AuthPolicyClient overrides.
  void JoinAdDomain(const std::string& machine_name,
                    const std::string& user_principal_name,
                    int password_fd,
                    JoinCallback callback) override;
  void AuthenticateUser(const std::string& user_principal_name,
                        const std::string& object_guid,
                        int password_fd,
                        AuthCallback callback) override;
  void GetUserStatus(const std::string& object_guid,
                     GetUserStatusCallback callback) override;
  void RefreshDevicePolicy(RefreshPolicyCallback calllback) override;
  void RefreshUserPolicy(const AccountId& account_id,
                         RefreshPolicyCallback callback) override;

  // Mark service as started. It's getting started by the
  // UpstartClient::StartAuthPolicyService on the Active Directory managed
  // devices.
  void set_started(bool started) { started_ = started; }

  bool started() const { return started_; }

  void set_auth_error(authpolicy::ErrorType auth_error) {
    auth_error_ = auth_error;
  }

  void set_display_name(const std::string& display_name) {
    display_name_ = display_name;
  }

  void set_given_name(const std::string& given_name) {
    given_name_ = given_name;
  }

  void set_password_status(
      authpolicy::ActiveDirectoryUserStatus::PasswordStatus password_status) {
    password_status_ = password_status;
  }

  void set_tgt_status(
      authpolicy::ActiveDirectoryUserStatus::TgtStatus tgt_status) {
    tgt_status_ = tgt_status;
  }

  void set_on_get_status_closure(base::OnceClosure on_get_status_closure) {
    on_get_status_closure_ = std::move(on_get_status_closure);
  }

  void set_operation_delay(const base::TimeDelta operation_delay) {
    operation_delay_ = operation_delay;
  }

 private:
  bool started_ = false;
  // If valid called after GetUserStatusCallback is called.
  base::OnceClosure on_get_status_closure_;
  authpolicy::ErrorType auth_error_ = authpolicy::ERROR_NONE;
  std::string display_name_;
  std::string given_name_;
  authpolicy::ActiveDirectoryUserStatus::PasswordStatus password_status_ =
      authpolicy::ActiveDirectoryUserStatus::PASSWORD_VALID;
  authpolicy::ActiveDirectoryUserStatus::TgtStatus tgt_status_ =
      authpolicy::ActiveDirectoryUserStatus::TGT_VALID;
  base::TimeDelta operation_delay_;
  DISALLOW_COPY_AND_ASSIGN(FakeAuthPolicyClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_AUTH_POLICY_CLIENT_H_
