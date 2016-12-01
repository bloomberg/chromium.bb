// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_AUTH_POLICY_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_AUTH_POLICY_CLIENT_H_

#include <string>

#include "base/macros.h"

#include "chromeos/dbus/auth_policy_client.h"

namespace chromeos {

class CHROMEOS_EXPORT FakeAuthPolicyClient : public AuthPolicyClient {
 public:
  FakeAuthPolicyClient();
  ~FakeAuthPolicyClient() override;

  // DBusClient overrides.
  void Init(dbus::Bus* bus) override;
  // AuthPolicyClient overrides.
  void JoinAdDomain(const std::string& machine_name,
                    const std::string& user,
                    int password_fd,
                    const JoinCallback& callback) override;
  void RefreshDevicePolicy(const RefreshPolicyCallback& callback) override;
  void RefreshUserPolicy(const std::string& account_id,
                         const RefreshPolicyCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAuthPolicyClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_AUTH_POLICY_CLIENT_H_
