// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_auth_policy_client.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/signin/core/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

const char kCorrectMachineName[] = "machine_name";
const char kCorrectUserName[] = "user@realm.com";
const char kObjectGUID[] = "user-object-guid";

}  // namespace

class FakeAuthPolicyClientTest : public ::testing::Test {
 public:
  FakeAuthPolicyClientTest() {}

 protected:
  FakeAuthPolicyClient* authpolicy_client() { return &client_; }

 private:
  FakeAuthPolicyClient client_;
  base::MessageLoop loop_;

  DISALLOW_COPY_AND_ASSIGN(FakeAuthPolicyClientTest);
};

// Tests parsing machine name.
TEST_F(FakeAuthPolicyClientTest, JoinAdDomain_ParseMachineName) {
  authpolicy_client()->set_started(true);
  authpolicy_client()->JoinAdDomain("correct_length1", kCorrectUserName,
                                    /* password_fd */ -1,
                                    base::Bind([](authpolicy::ErrorType error) {
                                      EXPECT_EQ(authpolicy::ERROR_NONE, error);
                                    }));
  authpolicy_client()->JoinAdDomain(
      "", kCorrectUserName, /* password_fd */ -1,
      base::Bind([](authpolicy::ErrorType error) {
        EXPECT_EQ(authpolicy::ERROR_BAD_MACHINE_NAME, error);
      }));
  authpolicy_client()->JoinAdDomain(
      "too_long_machine_name", kCorrectUserName, /* password_fd */ -1,
      base::Bind([](authpolicy::ErrorType error) {
        EXPECT_EQ(authpolicy::ERROR_MACHINE_NAME_TOO_LONG, error);
      }));
  authpolicy_client()->JoinAdDomain(
      "invalid:name", kCorrectUserName, /* password_fd */ -1,
      base::Bind([](authpolicy::ErrorType error) {
        EXPECT_EQ(authpolicy::ERROR_BAD_MACHINE_NAME, error);
      }));
  authpolicy_client()->JoinAdDomain(
      ">nvalidname", kCorrectUserName, /* password_fd */ -1,
      base::Bind([](authpolicy::ErrorType error) {
        EXPECT_EQ(authpolicy::ERROR_BAD_MACHINE_NAME, error);
      }));

  base::RunLoop().RunUntilIdle();
}

// Tests parsing user name.
TEST_F(FakeAuthPolicyClientTest, JoinAdDomain_ParseUPN) {
  authpolicy_client()->set_started(true);
  authpolicy_client()->JoinAdDomain(kCorrectMachineName, "user@realm.com",
                                    /* password_fd */ -1,
                                    base::Bind([](authpolicy::ErrorType error) {
                                      EXPECT_EQ(authpolicy::ERROR_NONE, error);
                                    }));
  authpolicy_client()->JoinAdDomain(
      kCorrectMachineName, "user", /* password_fd */ -1,
      base::Bind([](authpolicy::ErrorType error) {
        EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
      }));
  authpolicy_client()->JoinAdDomain(
      kCorrectMachineName, "", /* password_fd */ -1,
      base::Bind([](authpolicy::ErrorType error) {
        EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
      }));
  authpolicy_client()->JoinAdDomain(
      kCorrectMachineName, "user@", /* password_fd */ -1,
      base::Bind([](authpolicy::ErrorType error) {
        EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
      }));
  authpolicy_client()->JoinAdDomain(
      kCorrectMachineName, "@realm", /* password_fd */ -1,
      base::Bind([](authpolicy::ErrorType error) {
        EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
      }));
  authpolicy_client()->JoinAdDomain(
      kCorrectMachineName, "user@realm@com",
      /* password_fd */ -1, base::Bind([](authpolicy::ErrorType error) {
        EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
      }));

  base::RunLoop().RunUntilIdle();
}

// Test AuthenticateUser.
TEST_F(FakeAuthPolicyClientTest, AuthenticateUser_ByObjectGUID) {
  authpolicy_client()->set_started(true);
  // Check that objectGUID do not change.
  authpolicy_client()->AuthenticateUser(
      kCorrectUserName, kObjectGUID, /* password_fd */ -1,
      base::Bind(
          [](authpolicy::ErrorType error,
             const authpolicy::ActiveDirectoryAccountData& account_data) {
            EXPECT_EQ(authpolicy::ERROR_NONE, error);
            EXPECT_EQ(kObjectGUID, account_data.account_id());
          }));
}

// Tests calls to not started authpolicyd fails.
TEST_F(FakeAuthPolicyClientTest, NotStartedAuthPolicyService) {
  authpolicy_client()->JoinAdDomain(
      kCorrectMachineName, kCorrectUserName,
      /* password_fd */ -1, base::Bind([](authpolicy::ErrorType error) {
        EXPECT_EQ(authpolicy::ERROR_DBUS_FAILURE, error);
      }));
  authpolicy_client()->AuthenticateUser(
      kCorrectUserName, std::string() /* object_guid */, /* password_fd */ -1,
      base::Bind([](authpolicy::ErrorType error,
                    const authpolicy::ActiveDirectoryAccountData&) {
        EXPECT_EQ(authpolicy::ERROR_DBUS_FAILURE, error);
      }));
  authpolicy_client()->RefreshDevicePolicy(
      base::Bind([](bool success) { EXPECT_FALSE(success); }));
  authpolicy_client()->RefreshUserPolicy(
      AccountId::FromUserEmail(kCorrectUserName),
      base::Bind([](bool success) { EXPECT_FALSE(success); }));

  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
