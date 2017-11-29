// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/authpolicy_login_helper.h"

#include "base/bind.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_auth_policy_client.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class MockAuthPolicyClient : public FakeAuthPolicyClient {
 public:
  MockAuthPolicyClient() = default;
  ~MockAuthPolicyClient() override = default;

  void JoinAdDomain(const authpolicy::JoinDomainRequest& request,
                    int password_fd,
                    JoinCallback callback) override {
    EXPECT_FALSE(join_ad_domain_called_);
    EXPECT_FALSE(refresh_device_policy_called_);
    join_ad_domain_called_ = true;
    std::move(callback).Run(authpolicy::ERROR_NONE);
  }

  void RefreshDevicePolicy(RefreshPolicyCallback callback) override {
    EXPECT_TRUE(join_ad_domain_called_);
    EXPECT_FALSE(refresh_device_policy_called_);
    refresh_device_policy_called_ = true;
    std::move(callback).Run(
        authpolicy::ERROR_DEVICE_POLICY_CACHED_BUT_NOT_SENT);
  }

  void CheckExpectations() {
    EXPECT_TRUE(join_ad_domain_called_);
    EXPECT_TRUE(refresh_device_policy_called_);
  }

 private:
  bool join_ad_domain_called_ = false;
  bool refresh_device_policy_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockAuthPolicyClient);
};

}  // namespace

// Check that helper calls RefreshDevicePolicy after JoinAdDomain.
TEST(AuthPolicyLoginHelper, JoinFollowedByRefreshDevicePolicy) {
  std::unique_ptr<MockAuthPolicyClient> mock_client =
      std::make_unique<MockAuthPolicyClient>();
  MockAuthPolicyClient* mock_client_ptr = mock_client.get();
  DBusThreadManager::GetSetterForTesting()->SetAuthPolicyClient(
      std::move(mock_client));
  DBusThreadManager::GetSetterForTesting()->SetCryptohomeClient(
      std::make_unique<FakeCryptohomeClient>());
  AuthPolicyLoginHelper helper;
  helper.JoinAdDomain(std::string(), std::string(), std::string(),
                      base::BindOnce([](authpolicy::ErrorType error) {
                        EXPECT_EQ(authpolicy::ERROR_NONE, error);
                      }));
  mock_client_ptr->CheckExpectations();
}

}  // namespace chromeos
