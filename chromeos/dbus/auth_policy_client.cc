// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/auth_policy_client.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/core/account_id/account_id.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

class AuthPolicyClientImpl : public AuthPolicyClient {
 public:
  AuthPolicyClientImpl() : weak_ptr_factory_(this) {}

  ~AuthPolicyClientImpl() override {}

  // AuthPolicyClient override.
  void JoinAdDomain(const std::string& machine_name,
                    const std::string& user_principal_name,
                    int password_fd,
                    const JoinCallback& callback) override {
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kAuthPolicyJoinADDomain);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(machine_name);
    writer.AppendString(user_principal_name);
    writer.AppendFileDescriptor(password_fd);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&AuthPolicyClientImpl::HandleJoinCallback,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void AuthenticateUser(const std::string& user_principal_name,
                        int password_fd,
                        const AuthCallback& callback) override {
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kAuthPolicyAuthenticateUser);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(user_principal_name);
    writer.AppendFileDescriptor(password_fd);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&AuthPolicyClientImpl::HandleAuthCallback,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void RefreshDevicePolicy(const RefreshPolicyCallback& callback) override {
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kAuthPolicyRefreshDevicePolicy);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&AuthPolicyClientImpl::HandleRefreshPolicyCallback,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void RefreshUserPolicy(const AccountId& account_id,
                         const RefreshPolicyCallback& callback) override {
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kAuthPolicyRefreshUserPolicy);
    dbus::MessageWriter writer(&method_call);
    // TODO(tnagel): Switch to GUID once authpolicyd, session_manager and
    // cryptohome support it, cf. https://crbug.com/677497.
    writer.AppendString(account_id.GetUserEmail());
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&AuthPolicyClientImpl::HandleRefreshPolicyCallback,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    bus_ = bus;
    proxy_ = bus_->GetObjectProxy(
        authpolicy::kAuthPolicyServiceName,
        dbus::ObjectPath(authpolicy::kAuthPolicyServicePath));
  }

 private:
  void HandleRefreshPolicyCallback(const RefreshPolicyCallback& callback,
                                   dbus::Response* response) {
    if (!response) {
      DLOG(ERROR) << "RefreshDevicePolicy: failed to call to authpolicy";
      callback.Run(false);
      return;
    }
    callback.Run(true);
  }

  void HandleJoinCallback(const JoinCallback& callback,
                          dbus::Response* response) {
    if (!response) {
      DLOG(ERROR) << "Join: Couldn't call to authpolicy";
      // TODO(rsorokin): make proper call, after defining possible errors codes.
      callback.Run(authpolicy::types::AD_JOIN_ERROR_UNKNOWN);
      return;
    }

    dbus::MessageReader reader(response);
    int res = authpolicy::types::AD_JOIN_ERROR_UNKNOWN;
    if (!reader.PopInt32(&res)) {
      DLOG(ERROR) << "Join: Couldn't get an error from the response";
      // TODO(rsorokin): make proper call, after defining possible errors codes.
      callback.Run(authpolicy::types::AD_JOIN_ERROR_DBUS_FAIL);
      return;
    }

    callback.Run(res);
  }

  void HandleAuthCallback(const AuthCallback& callback,
                          dbus::Response* response) {
    std::string user_id;
    int32_t res = static_cast<int32_t>(authpolicy::AUTH_USER_ERROR_UNKNOWN);
    if (!response) {
      DLOG(ERROR) << "Auth: Failed to  call to authpolicy";
      // TODO(rsorokin): make proper call, after defining possible errors codes.
      callback.Run(authpolicy::AUTH_USER_ERROR_DBUS_FAILURE, user_id);
      return;
    }
    dbus::MessageReader reader(response);
    if (!reader.PopInt32(&res)) {
      DLOG(ERROR) << "Auth: Failed to get an error from the response";
      // TODO(rsorokin): make proper call, after defining possible errors codes.
      callback.Run(static_cast<authpolicy::AuthUserErrorType>(res), user_id);
      return;
    }
    if (res < 0 || res >= authpolicy::AUTH_USER_ERROR_COUNT)
      res = static_cast<int32_t>(authpolicy::AUTH_USER_ERROR_UNKNOWN);
    if (!reader.PopString(&user_id))
      DLOG(ERROR) << "Auth: Failed to get user_id from the response";
    callback.Run(static_cast<authpolicy::AuthUserErrorType>(res), user_id);
  }

  dbus::Bus* bus_ = nullptr;
  dbus::ObjectProxy* proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<AuthPolicyClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuthPolicyClientImpl);
};

}  // namespace

AuthPolicyClient::AuthPolicyClient() {}

AuthPolicyClient::~AuthPolicyClient() {}

// static
AuthPolicyClient* AuthPolicyClient::Create() {
  return new AuthPolicyClientImpl();
}

}  // namespace chromeos
