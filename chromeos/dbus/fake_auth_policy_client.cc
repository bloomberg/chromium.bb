// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/fake_auth_policy_client.h"

namespace chromeos {

FakeAuthPolicyClient::FakeAuthPolicyClient() {}

FakeAuthPolicyClient::~FakeAuthPolicyClient() {}

void FakeAuthPolicyClient::Init(dbus::Bus* bus) {}

void FakeAuthPolicyClient::JoinAdDomain(const std::string& machine_name,
                                        const std::string& user,
                                        int password_fd,
                                        const JoinCallback& callback) {
  callback.Run(authpolicy::AD_JOIN_ERROR_NONE);
}

void FakeAuthPolicyClient::RefreshDevicePolicy(
    const RefreshPolicyCallback& callback) {
  callback.Run(true);
};

void FakeAuthPolicyClient::RefreshUserPolicy(
    const std::string& account_id,
    const RefreshPolicyCallback& callback) {
  callback.Run(true);
};

}  // namespace chromeos
