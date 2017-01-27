// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_auth_policy_client.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

const char kCorrectMachineName[] = "machine_name";
const char kCorrectUserName[] = "user@realm.com";

void JoinAdCallback(const std::string& machine_name,
                    const std::string& user_principal_name,
                    authpolicy::ErrorType expected,
                    authpolicy::ErrorType actual) {
  EXPECT_EQ(expected, actual) << "with machine name: " << machine_name
                              << ", and user name: " << user_principal_name;
}

void TestDomainJoin(const std::string& machine_name,
                    const std::string& user_principal_name,
                    authpolicy::ErrorType expected) {
  FakeAuthPolicyClient client;
  client.JoinAdDomain(
      machine_name, user_principal_name, /* password_fd */ -1,
      base::Bind(&JoinAdCallback, machine_name, user_principal_name, expected));
}

}  // namespace

// Tests parsing machine name.
TEST(FakeAuthPolicyClientTest, JoinAdDomain_ParseMachineName) {
  TestDomainJoin("correct_length1", kCorrectUserName, authpolicy::ERROR_NONE);
  TestDomainJoin("", kCorrectUserName, authpolicy::ERROR_BAD_MACHINE_NAME);
  TestDomainJoin("too_long_machine_name ", kCorrectUserName,
                 authpolicy::ERROR_MACHINE_NAME_TOO_LONG);
  TestDomainJoin("invalid:name", kCorrectUserName,
                 authpolicy::ERROR_BAD_MACHINE_NAME);
  TestDomainJoin(">nvalidname", kCorrectUserName,
                 authpolicy::ERROR_BAD_MACHINE_NAME);
}

// Tests parsing user name.
TEST(FakeAuthPolicyClientTest, JoinAdDomain_ParseUPN) {
  TestDomainJoin(kCorrectMachineName, "user@realm.com", authpolicy::ERROR_NONE);
  TestDomainJoin(kCorrectMachineName, "user",
                 authpolicy::ERROR_PARSE_UPN_FAILED);
  TestDomainJoin(kCorrectMachineName, "", authpolicy::ERROR_PARSE_UPN_FAILED);
  TestDomainJoin(kCorrectMachineName, "user@",
                 authpolicy::ERROR_PARSE_UPN_FAILED);
  TestDomainJoin(kCorrectMachineName, "@realm",
                 authpolicy::ERROR_PARSE_UPN_FAILED);
  TestDomainJoin(kCorrectMachineName, "user@realm@com",
                 authpolicy::ERROR_PARSE_UPN_FAILED);
}

}  // namespace chromeos
