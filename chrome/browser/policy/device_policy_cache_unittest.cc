// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_cache.h"

#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/mock_signed_settings_helper.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "content/test/test_browser_thread.h"
#include "policy/configuration_policy_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Test registration user name.
const char kTestUser[] = "test@example.com";

using ::chromeos::SignedSettings;
using ::testing::InSequence;
using ::testing::_;

void CreatePolicy(em::PolicyFetchResponse* policy,
                  const std::string& user,
                  em::ChromeDeviceSettingsProto& settings) {
  // This method omits a few fields which currently aren't needed by tests:
  // timestamp, machine_name, policy_type, public key info.
  em::PolicyData signed_response;
  signed_response.set_username(user);
  signed_response.set_request_token("dmtoken");
  signed_response.set_device_id("deviceid");
  EXPECT_TRUE(
      settings.SerializeToString(signed_response.mutable_policy_value()));
  std::string serialized_signed_response;
  EXPECT_TRUE(signed_response.SerializeToString(&serialized_signed_response));
  policy->set_policy_data(serialized_signed_response);
}

void CreateRefreshRatePolicy(em::PolicyFetchResponse* policy,
                             const std::string& user,
                             int refresh_rate) {
  em::ChromeDeviceSettingsProto settings;
  settings.mutable_device_policy_refresh_rate()->
      set_device_policy_refresh_rate(refresh_rate);
  CreatePolicy(policy, user, settings);
}

void CreateProxyPolicy(em::PolicyFetchResponse* policy,
                       const std::string& user,
                       const std::string& proxy_mode,
                       const std::string& proxy_server,
                       const std::string& proxy_pac_url,
                       const std::string& proxy_bypass_list) {
  em::ChromeDeviceSettingsProto settings;
  em::DeviceProxySettingsProto* proxy_settings =
      settings.mutable_device_proxy_settings();
  proxy_settings->set_proxy_mode(proxy_mode);
  proxy_settings->set_proxy_server(proxy_server);
  proxy_settings->set_proxy_pac_url(proxy_pac_url);
  proxy_settings->set_proxy_bypass_list(proxy_bypass_list);
  CreatePolicy(policy, user, settings);
}

}  // namespace

class DevicePolicyCacheTest : public testing::Test {
 protected:
  DevicePolicyCacheTest()
      : cryptohome_(chromeos::CryptohomeLibrary::GetImpl(true)),
        install_attributes_(cryptohome_.get()),
        message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_) {}

  virtual void SetUp() {
    data_store_.reset(CloudPolicyDataStore::CreateForUserPolicies());
    cache_.reset(new DevicePolicyCache(data_store_.get(),
                                       &install_attributes_,
                                       &signed_settings_helper_));
  }

  virtual void TearDown() {
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
  scoped_ptr<CloudPolicyDataStore> data_store_;
  chromeos::MockSignedSettingsHelper signed_settings_helper_;
  scoped_ptr<DevicePolicyCache> cache_;

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

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
  base::FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetMandatoryPolicy(
                                kPolicyDevicePolicyRefreshRate)));
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
  base::FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetMandatoryPolicy(
                                kPolicyDevicePolicyRefreshRate)));

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
  base::FundamentalValue updated_expected(300);
  EXPECT_TRUE(Value::Equals(&updated_expected,
                            GetMandatoryPolicy(
                                kPolicyDevicePolicyRefreshRate)));
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

  base::FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetMandatoryPolicy(
                                kPolicyDevicePolicyRefreshRate)));
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

  base::FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetMandatoryPolicy(
                                kPolicyDevicePolicyRefreshRate)));
}

TEST_F(DevicePolicyCacheTest, SetProxyPolicy) {
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

TEST_F(DevicePolicyCacheTest, SetDeviceNetworkConfigurationPolicy) {
  MakeEnterpriseDevice(kTestUser);

  // Startup.
  std::string fake_config("{ 'NetworkConfigurations': [] }");
  em::PolicyFetchResponse policy;
  em::ChromeDeviceSettingsProto settings;
  settings.mutable_open_network_configuration()->set_open_network_configuration(
      fake_config);
  CreatePolicy(&policy, kTestUser, settings);
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_)).WillOnce(
      MockSignedSettingsHelperRetrievePolicy(SignedSettings::SUCCESS,
                                             policy));
  cache_->Load();
  testing::Mock::VerifyAndClearExpectations(&signed_settings_helper_);
  StringValue expected_config(fake_config);
  EXPECT_TRUE(
      Value::Equals(&expected_config,
                    GetMandatoryPolicy(kPolicyDeviceOpenNetworkConfiguration)));
}

}  // namespace policy
