// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_auth_policy_client.h"

#include "base/bind.h"
#include "components/signin/core/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

const char kCorrectMachineName[] = "machine_name";
const char kCorrectUserName[] = "user@realm.com";

}  // namespace

// Tests parsing machine name.
TEST(FakeAuthPolicyClientTest, JoinAdDomain_ParseMachineName) {
  FakeAuthPolicyClient client;
  client.set_started(true);
  client.JoinAdDomain("correct_length1", kCorrectUserName, /* password_fd */ -1,
                      base::Bind([](authpolicy::ErrorType error) {
                        EXPECT_EQ(authpolicy::ERROR_NONE, error);
                      }));
  client.JoinAdDomain("", kCorrectUserName, /* password_fd */ -1,
                      base::Bind([](authpolicy::ErrorType error) {
                        EXPECT_EQ(authpolicy::ERROR_BAD_MACHINE_NAME, error);
                      }));
  client.JoinAdDomain(
      "too_long_machine_name", kCorrectUserName, /* password_fd */ -1,
      base::Bind([](authpolicy::ErrorType error) {
        EXPECT_EQ(authpolicy::ERROR_MACHINE_NAME_TOO_LONG, error);
      }));
  client.JoinAdDomain("invalid:name", kCorrectUserName, /* password_fd */ -1,
                      base::Bind([](authpolicy::ErrorType error) {
                        EXPECT_EQ(authpolicy::ERROR_BAD_MACHINE_NAME, error);
                      }));
  client.JoinAdDomain(">nvalidname", kCorrectUserName, /* password_fd */ -1,
                      base::Bind([](authpolicy::ErrorType error) {
                        EXPECT_EQ(authpolicy::ERROR_BAD_MACHINE_NAME, error);
                      }));
}

// Tests parsing user name.
TEST(FakeAuthPolicyClientTest, JoinAdDomain_ParseUPN) {
  FakeAuthPolicyClient client;
  client.set_started(true);
  client.JoinAdDomain(kCorrectMachineName, "user@realm.com",
                      /* password_fd */ -1,
                      base::Bind([](authpolicy::ErrorType error) {
                        EXPECT_EQ(authpolicy::ERROR_NONE, error);
                      }));
  client.JoinAdDomain(kCorrectMachineName, "user", /* password_fd */ -1,
                      base::Bind([](authpolicy::ErrorType error) {
                        EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
                      }));
  client.JoinAdDomain(kCorrectMachineName, "", /* password_fd */ -1,
                      base::Bind([](authpolicy::ErrorType error) {
                        EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
                      }));
  client.JoinAdDomain(kCorrectMachineName, "user@", /* password_fd */ -1,
                      base::Bind([](authpolicy::ErrorType error) {
                        EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
                      }));
  client.JoinAdDomain(kCorrectMachineName, "@realm", /* password_fd */ -1,
                      base::Bind([](authpolicy::ErrorType error) {
                        EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
                      }));
  client.JoinAdDomain(kCorrectMachineName, "user@realm@com",
                      /* password_fd */ -1,
                      base::Bind([](authpolicy::ErrorType error) {
                        EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
                      }));
}

// Tests calls to not started authpolicyd fails.
TEST(FakeAuthPolicyClientTest, NotStartedAuthPolicyService) {
  FakeAuthPolicyClient client;
  client.JoinAdDomain(kCorrectMachineName, kCorrectUserName,
                      /* password_fd */ -1,
                      base::Bind([](authpolicy::ErrorType error) {
                        EXPECT_EQ(authpolicy::ERROR_DBUS_FAILURE, error);
                      }));
  client.AuthenticateUser(
      kCorrectUserName, /* password_fd */ -1,
      base::Bind([](authpolicy::ErrorType error,
                    const authpolicy::ActiveDirectoryAccountData&) {
        EXPECT_EQ(authpolicy::ERROR_DBUS_FAILURE, error);
      }));
  client.RefreshDevicePolicy(
      base::Bind([](bool success) { EXPECT_EQ(false, success); }));
  client.RefreshUserPolicy(
      AccountId::FromUserEmail(kCorrectUserName),
      base::Bind([](bool success) { EXPECT_EQ(false, success); }));
}

}  // namespace chromeos
