// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/auth_policy_client.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
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
                    const std::string& user,
                    int password_fd,
                    const JoinCallback& callback) override {
    dbus::ObjectPath objectPath(authpolicy::kAuthPolicyServicePath);
    dbus::ObjectProxy* proxy =
        bus_->GetObjectProxy(authpolicy::kAuthPolicyServiceName, objectPath);
    dbus::MethodCall method_call(authpolicy::kAuthPolicyInterface,
                                 authpolicy::kAuthPolicyJoinADDomain);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(machine_name);
    writer.AppendString(user);
    writer.AppendFileDescriptor(password_fd);
    proxy->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                      base::Bind(&AuthPolicyClientImpl::HandleJoinCallback,
                                 weak_ptr_factory_.GetWeakPtr(), callback));
  }

 protected:
  void Init(dbus::Bus* bus) override { bus_ = bus; }

 private:
  void HandleJoinCallback(const JoinCallback& callback,
                          dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Join: Couldn't call to authpolicy";
      // TODO(rsorokin): make proper call, after defining possible errors codes.
      callback.Run(authpolicy::AD_JOIN_ERROR_UNKNOWN);
      return;
    }

    dbus::MessageReader reader(response);
    int res = authpolicy::AD_JOIN_ERROR_UNKNOWN;
    if (!reader.PopInt32(&res)) {
      LOG(ERROR) << "Join: Couldn't get an error from the response";
      // TODO(rsorokin): make proper call, after defining possible errors codes.
      callback.Run(authpolicy::AD_JOIN_ERROR_DBUS_FAIL);
      return;
    }

    callback.Run(res);
  }

  dbus::Bus* bus_ = nullptr;

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
