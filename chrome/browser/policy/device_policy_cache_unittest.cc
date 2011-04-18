// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_cache.h"

#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/policy/device_policy_identity_strategy.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "policy/configuration_policy_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Test registration user name.
const char kTestUser[] = "test@example.com";

using ::chromeos::SignedSettings;
using ::chromeos::SignedSettingsHelper;
using ::testing::_;
using ::testing::InSequence;

class MockSignedSettingsHelper : public SignedSettingsHelper {
 public:
  MockSignedSettingsHelper() {}
  virtual ~MockSignedSettingsHelper() {}

  MOCK_METHOD2(StartStorePolicyOp, void(const em::PolicyFetchResponse&,
                                        SignedSettingsHelper::Callback*));
  MOCK_METHOD1(StartRetrievePolicyOp, void(SignedSettingsHelper::Callback*));
  MOCK_METHOD1(CancelCallback, void(SignedSettingsHelper::Callback*));

  // This test doesn't need these methods, but since they're pure virtual in
  // SignedSettingsHelper, they must be implemented:
  MOCK_METHOD2(StartCheckWhitelistOp, void(const std::string&,
                                           SignedSettingsHelper::Callback*));
  MOCK_METHOD3(StartWhitelistOp, void(const std::string&, bool,
                                      SignedSettingsHelper::Callback*));
  MOCK_METHOD3(StartStorePropertyOp, void(const std::string&,
                                          const std::string&,
                                          SignedSettingsHelper::Callback*));
  MOCK_METHOD2(StartRetrieveProperty, void(const std::string&,
                                           SignedSettingsHelper::Callback*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSignedSettingsHelper);
};

ACTION_P(MockSignedSettingsHelperStorePolicy, status_code) {
  arg1->OnStorePolicyCompleted(status_code);
}

ACTION_P2(MockSignedSettingsHelperRetrievePolicy, status_code, policy) {
  arg0->OnRetrievePolicyCompleted(status_code, policy);
}

void CreateRefreshRatePolicy(em::PolicyFetchResponse* policy,
                             const std::string& user,
                             int refresh_rate) {
  // This method omits a few fields which currently aren't needed by tests:
  // timestamp, machine_name, policy_type, public key info.
  em::PolicyData signed_response;
  em::ChromeDeviceSettingsProto settings;
  settings.mutable_policy_refresh_rate()->set_policy_refresh_rate(refresh_rate);
  signed_response.set_username(user);
  signed_response.set_request_token("dmtoken");
  signed_response.set_device_id("deviceid");
  EXPECT_TRUE(
      settings.SerializeToString(signed_response.mutable_policy_value()));
  std::string serialized_signed_response;
  EXPECT_TRUE(signed_response.SerializeToString(&serialized_signed_response));
  policy->set_policy_data(serialized_signed_response);
}

void CreateProxyPolicy(em::PolicyFetchResponse* policy,
                       const std::string& user,
                       const std::string& proxy_mode,
                       const std::string& proxy_server,
                       const std::string& proxy_pac_url,
                       const std::string& proxy_bypass_list) {
  em::PolicyData signed_response;
  em::ChromeDeviceSettingsProto settings;
  em::DeviceProxySettingsProto* proxy_settings =
      settings.mutable_device_proxy_settings();
  proxy_settings->set_proxy_mode(proxy_mode);
  proxy_settings->set_proxy_server(proxy_server);
  proxy_settings->set_proxy_pac_url(proxy_pac_url);
  proxy_settings->set_proxy_bypass_list(proxy_bypass_list);
  signed_response.set_username(user);
  signed_response.set_request_token("dmtoken");
  signed_response.set_device_id("deviceid");
  EXPECT_TRUE(
      settings.SerializeToString(signed_response.mutable_policy_value()));
  std::string serialized_signed_response;
  EXPECT_TRUE(signed_response.SerializeToString(&serialized_signed_response));
  policy->set_policy_data(serialized_signed_response);
}

}  // namespace

class DevicePolicyCacheTest : public testing::Test {
 protected:
  DevicePolicyCacheTest()
      : cryptohome_(chromeos::CryptohomeLibrary::GetImpl(true)),
        install_attributes_(cryptohome_.get()) {}

  virtual void SetUp() {
    cache_.reset(new DevicePolicyCache(&identity_strategy_,
                                       &install_attributes_,
                                       &signed_settings_helper_));
  }

  virtual void TearDown() {
    EXPECT_CALL(signed_settings_helper_, CancelCallback(_));
    cache_.reset();
  }

  void MakeEnterpriseDevice(const char* registration_user) {
    ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
              install_attributes_.LockDevice(registration_user));
  }

  const Value* GetMandatoryPolicy(ConfigurationPolicyType policy) {
    return cache_->mandatory_policy_.Get(policy);
  }

  const Value* GetRecommendedPolicy(ConfigurationPolicyType policy) {
    return cache_->recommended_policy_.Get(policy);
  }

  scoped_ptr<chromeos::CryptohomeLibrary> cryptohome_;
  EnterpriseInstallAttributes install_attributes_;
  DevicePolicyIdentityStrategy identity_strategy_;
  MockSignedSettingsHelper signed_settings_helper_;
  scoped_ptr<DevicePolicyCache> cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCacheTest);
};

TEST_F(DevicePolicyCacheTest, Startup) {
  em::PolicyFetchResponse policy;
  CreateRefreshRatePolicy(&policy, kTestUser, 120);
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_)).WillOnce(
      MockSignedSettingsHelperRetrievePolicy(SignedSettings::SUCCESS,
                                             policy));
  cache_->Load();
  testing::Mock::VerifyAndClearExpectations(&signed_settings_helper_);
  FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetMandatoryPolicy(kPolicyPolicyRefreshRate)));
}

TEST_F(DevicePolicyCacheTest, SetPolicy) {
  InSequence s;

  MakeEnterpriseDevice(kTestUser);

  // Startup.
  em::PolicyFetchResponse policy;
  CreateRefreshRatePolicy(&policy, kTestUser, 120);
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_)).WillOnce(
      MockSignedSettingsHelperRetrievePolicy(SignedSettings::SUCCESS,
                                             policy));
  cache_->Load();
  testing::Mock::VerifyAndClearExpectations(&signed_settings_helper_);
  FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetMandatoryPolicy(kPolicyPolicyRefreshRate)));

  // Set new policy information.
  em::PolicyFetchResponse new_policy;
  CreateRefreshRatePolicy(&new_policy, kTestUser, 300);
  EXPECT_CALL(signed_settings_helper_, StartStorePolicyOp(_, _)).WillOnce(
      MockSignedSettingsHelperStorePolicy(chromeos::SignedSettings::SUCCESS));
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_)).WillOnce(
      MockSignedSettingsHelperRetrievePolicy(SignedSettings::SUCCESS,
                                             new_policy));
  cache_->SetPolicy(new_policy);
  testing::Mock::VerifyAndClearExpectations(&signed_settings_helper_);
  FundamentalValue updated_expected(300);
  EXPECT_TRUE(Value::Equals(&updated_expected,
                            GetMandatoryPolicy(kPolicyPolicyRefreshRate)));
}

TEST_F(DevicePolicyCacheTest, SetPolicyWrongUser) {
  InSequence s;

  MakeEnterpriseDevice(kTestUser);

  // Startup.
  em::PolicyFetchResponse policy;
  CreateRefreshRatePolicy(&policy, kTestUser, 120);
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_)).WillOnce(
      MockSignedSettingsHelperRetrievePolicy(SignedSettings::SUCCESS,
                                             policy));
  cache_->Load();
  testing::Mock::VerifyAndClearExpectations(&signed_settings_helper_);

  // Set new policy information. This should fail due to invalid user.
  em::PolicyFetchResponse new_policy;
  CreateRefreshRatePolicy(&new_policy, "foreign_user@example.com", 300);
  EXPECT_CALL(signed_settings_helper_, StartStorePolicyOp(_, _)).Times(0);
  cache_->SetPolicy(new_policy);
  testing::Mock::VerifyAndClearExpectations(&signed_settings_helper_);

  FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetMandatoryPolicy(kPolicyPolicyRefreshRate)));
}

TEST_F(DevicePolicyCacheTest, SetPolicyNonEnterpriseDevice) {
  InSequence s;

  // Startup.
  em::PolicyFetchResponse policy;
  CreateRefreshRatePolicy(&policy, kTestUser, 120);
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_)).WillOnce(
      MockSignedSettingsHelperRetrievePolicy(SignedSettings::SUCCESS,
                                             policy));
  cache_->Load();
  testing::Mock::VerifyAndClearExpectations(&signed_settings_helper_);

  // Set new policy information. This should fail due to invalid user.
  em::PolicyFetchResponse new_policy;
  CreateRefreshRatePolicy(&new_policy, kTestUser, 120);
  EXPECT_CALL(signed_settings_helper_, StartStorePolicyOp(_, _)).Times(0);
  cache_->SetPolicy(new_policy);
  testing::Mock::VerifyAndClearExpectations(&signed_settings_helper_);

  FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetMandatoryPolicy(kPolicyPolicyRefreshRate)));
}

TEST_F(DevicePolicyCacheTest, SetProxyPolicy) {
  InSequence s;

  MakeEnterpriseDevice(kTestUser);

  // Startup.
  em::PolicyFetchResponse policy;
  CreateProxyPolicy(&policy, kTestUser, "direct", "http://proxy:8080",
                    "http://proxy:8080/pac.js", "127.0.0.1,example.com");
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_)).WillOnce(
      MockSignedSettingsHelperRetrievePolicy(SignedSettings::SUCCESS,
                                             policy));
  cache_->Load();
  testing::Mock::VerifyAndClearExpectations(&signed_settings_helper_);
  StringValue expected_proxy_mode("direct");
  StringValue expected_proxy_server("http://proxy:8080");
  StringValue expected_proxy_pac_url("http://proxy:8080/pac.js");
  StringValue expected_proxy_bypass_list("127.0.0.1,example.com");
  EXPECT_TRUE(Value::Equals(&expected_proxy_mode,
                            GetRecommendedPolicy(kPolicyProxyMode)));
  EXPECT_TRUE(Value::Equals(&expected_proxy_server,
                            GetRecommendedPolicy(kPolicyProxyServer)));
  EXPECT_TRUE(Value::Equals(&expected_proxy_pac_url,
                            GetRecommendedPolicy(kPolicyProxyPacUrl)));
  EXPECT_TRUE(Value::Equals(&expected_proxy_bypass_list,
                            GetRecommendedPolicy(kPolicyProxyBypassList)));
}

}  // namespace policy
