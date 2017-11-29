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
const char kAccountId[] = "user-account-id";

}  // namespace

class FakeAuthPolicyClientTest : public ::testing::Test {
 public:
  FakeAuthPolicyClientTest() = default;

 protected:
  FakeAuthPolicyClient* authpolicy_client() { return &client_; }

  void JoinAdDomain(const std::string& machine_name,
                    const std::string& username,
                    AuthPolicyClient::JoinCallback callback) {
    authpolicy::JoinDomainRequest request;
    request.set_machine_name(machine_name);
    request.set_user_principal_name(username);
    authpolicy_client()->JoinAdDomain(request, /* password_fd */ -1,
                                      std::move(callback));
  }

  void AuthenticateUser(const std::string& username,
                        const std::string& account_id,
                        AuthPolicyClient::AuthCallback callback) {
    authpolicy::AuthenticateUserRequest request;
    request.set_user_principal_name(username);
    request.set_account_id(account_id);
    authpolicy_client()->AuthenticateUser(request, /* password_fd */ -1,
                                          std::move(callback));
  }

 private:
  FakeAuthPolicyClient client_;
  base::MessageLoop loop_;

  DISALLOW_COPY_AND_ASSIGN(FakeAuthPolicyClientTest);
};

// Tests parsing machine name.
TEST_F(FakeAuthPolicyClientTest, JoinAdDomain_ParseMachineName) {
  authpolicy_client()->set_started(true);
  JoinAdDomain("correct_length1", kCorrectUserName,
               base::Bind([](authpolicy::ErrorType error) {
                 EXPECT_EQ(authpolicy::ERROR_NONE, error);
               }));
  JoinAdDomain("", kCorrectUserName,
               base::Bind([](authpolicy::ErrorType error) {
                 EXPECT_EQ(authpolicy::ERROR_INVALID_MACHINE_NAME, error);
               }));
  JoinAdDomain("too_long_machine_name", kCorrectUserName,
               base::Bind([](authpolicy::ErrorType error) {
                 EXPECT_EQ(authpolicy::ERROR_MACHINE_NAME_TOO_LONG, error);
               }));
  JoinAdDomain("invalid:name", kCorrectUserName,
               base::Bind([](authpolicy::ErrorType error) {
                 EXPECT_EQ(authpolicy::ERROR_INVALID_MACHINE_NAME, error);
               }));
  JoinAdDomain(">nvalidname", kCorrectUserName,
               base::Bind([](authpolicy::ErrorType error) {
                 EXPECT_EQ(authpolicy::ERROR_INVALID_MACHINE_NAME, error);
               }));

  base::RunLoop().RunUntilIdle();
}

// Tests parsing user name.
TEST_F(FakeAuthPolicyClientTest, JoinAdDomain_ParseUPN) {
  authpolicy_client()->set_started(true);
  JoinAdDomain(kCorrectMachineName, "user@realm.com",
               base::Bind([](authpolicy::ErrorType error) {
                 EXPECT_EQ(authpolicy::ERROR_NONE, error);
               }));
  JoinAdDomain(kCorrectMachineName, "user",
               base::Bind([](authpolicy::ErrorType error) {
                 EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
               }));
  JoinAdDomain(kCorrectMachineName, "",
               base::Bind([](authpolicy::ErrorType error) {
                 EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
               }));
  JoinAdDomain(kCorrectMachineName, "user@",
               base::Bind([](authpolicy::ErrorType error) {
                 EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
               }));
  JoinAdDomain(kCorrectMachineName, "@realm",
               base::Bind([](authpolicy::ErrorType error) {
                 EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
               }));
  JoinAdDomain(kCorrectMachineName, "user@realm@com",
               base::Bind([](authpolicy::ErrorType error) {
                 EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
               }));

  base::RunLoop().RunUntilIdle();
}

// Test AuthenticateUser.
TEST_F(FakeAuthPolicyClientTest, AuthenticateUser_ByAccountId) {
  authpolicy_client()->set_started(true);
  // Check that account_id do not change.
  AuthenticateUser(
      kCorrectUserName, kAccountId,
      base::Bind(
          [](authpolicy::ErrorType error,
             const authpolicy::ActiveDirectoryAccountInfo& account_info) {
            EXPECT_EQ(authpolicy::ERROR_NONE, error);
            EXPECT_EQ(kAccountId, account_info.account_id());
          }));
}

// Tests calls to not started authpolicyd fails.
TEST_F(FakeAuthPolicyClientTest, NotStartedAuthPolicyService) {
  JoinAdDomain(kCorrectMachineName, kCorrectUserName,
               base::Bind([](authpolicy::ErrorType error) {
                 EXPECT_EQ(authpolicy::ERROR_DBUS_FAILURE, error);
               }));
  AuthenticateUser(
      kCorrectUserName, std::string() /* account_id */,
      base::Bind([](authpolicy::ErrorType error,
                    const authpolicy::ActiveDirectoryAccountInfo&) {
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
