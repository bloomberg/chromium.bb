// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_cache.h"

#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/mock_signed_settings_helper.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "content/public/test/test_browser_thread.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Test registration user name.
const char kTestUser[] = "test@example.com";

using ::chromeos::SignedSettings;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::_;

class MockCloudPolicyCacheObserver : public CloudPolicyCacheBase::Observer {
 public:
  MOCK_METHOD1(OnCacheGoingAway, void(CloudPolicyCacheBase*));
  MOCK_METHOD1(OnCacheUpdate, void(CloudPolicyCacheBase*));
};

void CreatePolicy(em::PolicyFetchResponse* policy,
                  const std::string& user,
                  em::ChromeDeviceSettingsProto& settings) {
  // This method omits a few fields which currently aren't needed by tests:
  // timestamp, machine_name, public key info.
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
    cache_->AddObserver(&observer_);
  }

  virtual void TearDown() {
    cache_->RemoveObserver(&observer_);
    cache_.reset();
  }

  void MakeEnterpriseDevice(const char* registration_user) {
    ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
              install_attributes_.LockDevice(
                  registration_user,
                  DEVICE_MODE_ENTERPRISE,
                  std::string()));
  }

  const Value* GetPolicy(const char* policy_name) {
    return cache_->policy()->GetValue(policy_name);
  }

  MockCloudPolicyCacheObserver observer_;
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
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
  cache_->Load();
  Mock::VerifyAndClearExpectations(&signed_settings_helper_);
  base::FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetPolicy(key::kDevicePolicyRefreshRate)));
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
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
  cache_->Load();
  Mock::VerifyAndClearExpectations(&signed_settings_helper_);
  Mock::VerifyAndClearExpectations(&observer_);
  base::FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetPolicy(key::kDevicePolicyRefreshRate)));

  // Set new policy information.
  chromeos::SignedSettingsHelper::StorePolicyCallback store_callback;
  em::PolicyFetchResponse new_policy;
  CreateRefreshRatePolicy(&new_policy, kTestUser, 300);
  EXPECT_CALL(signed_settings_helper_, StartStorePolicyOp(_, _)).WillOnce(
      SaveArg<1>(&store_callback));
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get())).Times(0);
  EXPECT_TRUE(cache_->SetPolicy(new_policy));
  cache_->SetFetchingDone();
  Mock::VerifyAndClearExpectations(&signed_settings_helper_);
  Mock::VerifyAndClearExpectations(&observer_);
  ASSERT_FALSE(store_callback.is_null());

  chromeos::SignedSettingsHelper::RetrievePolicyCallback retrieve_callback;
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_)).WillOnce(
      SaveArg<0>(&retrieve_callback));
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get())).Times(0);
  store_callback.Run(chromeos::SignedSettings::SUCCESS);
  Mock::VerifyAndClearExpectations(&signed_settings_helper_);
  Mock::VerifyAndClearExpectations(&observer_);
  ASSERT_FALSE(retrieve_callback.is_null());

  // Cache update notification should only fire in the retrieve callback.
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
  retrieve_callback.Run(chromeos::SignedSettings::SUCCESS, new_policy);
  base::FundamentalValue updated_expected(300);
  EXPECT_TRUE(Value::Equals(&updated_expected,
                            GetPolicy(key::kDevicePolicyRefreshRate)));
  Mock::VerifyAndClearExpectations(&observer_);

  cache_->RemoveObserver(&observer_);
}

TEST_F(DevicePolicyCacheTest, SetPolicyOtherUserSameDomain) {
  InSequence s;

  MakeEnterpriseDevice(kTestUser);

  // Startup.
  em::PolicyFetchResponse policy;
  CreateRefreshRatePolicy(&policy, kTestUser, 120);
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_)).WillOnce(
      MockSignedSettingsHelperRetrievePolicy(SignedSettings::SUCCESS,
                                             policy));
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
  cache_->Load();
  Mock::VerifyAndClearExpectations(&signed_settings_helper_);

  // Set new policy information. This should succeed as the domain is the same.
  em::PolicyFetchResponse new_policy;
  CreateRefreshRatePolicy(&new_policy, "another_user@example.com", 300);
  EXPECT_CALL(signed_settings_helper_, StartStorePolicyOp(_, _)).Times(1);
  EXPECT_TRUE(cache_->SetPolicy(new_policy));
  Mock::VerifyAndClearExpectations(&signed_settings_helper_);
}

TEST_F(DevicePolicyCacheTest, SetPolicyOtherUserOtherDomain) {
  InSequence s;

  MakeEnterpriseDevice(kTestUser);

  // Startup.
  em::PolicyFetchResponse policy;
  CreateRefreshRatePolicy(&policy, kTestUser, 120);
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_)).WillOnce(
      MockSignedSettingsHelperRetrievePolicy(SignedSettings::SUCCESS,
                                             policy));
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
  cache_->Load();
  Mock::VerifyAndClearExpectations(&signed_settings_helper_);

  // Set new policy information. This should fail because the user is from
  // different domain.
  em::PolicyFetchResponse new_policy;
  CreateRefreshRatePolicy(&new_policy, "foreign_user@hackers.com", 300);
  EXPECT_CALL(signed_settings_helper_, StartStorePolicyOp(_, _)).Times(0);
  EXPECT_FALSE(cache_->SetPolicy(new_policy));
  Mock::VerifyAndClearExpectations(&signed_settings_helper_);

  base::FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetPolicy(key::kDevicePolicyRefreshRate)));
}

TEST_F(DevicePolicyCacheTest, SetPolicyNonEnterpriseDevice) {
  InSequence s;

  // Startup.
  em::PolicyFetchResponse policy;
  CreateRefreshRatePolicy(&policy, kTestUser, 120);
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_)).WillOnce(
      MockSignedSettingsHelperRetrievePolicy(SignedSettings::SUCCESS,
                                             policy));
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
  cache_->Load();
  Mock::VerifyAndClearExpectations(&signed_settings_helper_);

  // Set new policy information. This should fail due to invalid user.
  em::PolicyFetchResponse new_policy;
  CreateRefreshRatePolicy(&new_policy, kTestUser, 120);
  EXPECT_CALL(signed_settings_helper_, StartStorePolicyOp(_, _)).Times(0);
  EXPECT_FALSE(cache_->SetPolicy(new_policy));
  Mock::VerifyAndClearExpectations(&signed_settings_helper_);

  base::FundamentalValue expected(120);
  EXPECT_TRUE(Value::Equals(&expected,
                            GetPolicy(key::kDevicePolicyRefreshRate)));
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
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
  cache_->Load();
  Mock::VerifyAndClearExpectations(&signed_settings_helper_);
  DictionaryValue expected;
  expected.SetString(key::kProxyMode, "direct");
  expected.SetString(key::kProxyServer, "http://proxy:8080");
  expected.SetString(key::kProxyPacUrl, "http://proxy:8080/pac.js");
  expected.SetString(key::kProxyBypassList, "127.0.0.1,example.com");
  EXPECT_TRUE(Value::Equals(&expected,
                            GetPolicy(key::kProxySettings)));
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
  EXPECT_CALL(observer_, OnCacheUpdate(cache_.get()));
  cache_->Load();
  Mock::VerifyAndClearExpectations(&signed_settings_helper_);
  StringValue expected_config(fake_config);
  EXPECT_TRUE(
      Value::Equals(&expected_config,
                    GetPolicy(key::kDeviceOpenNetworkConfiguration)));
}

}  // namespace policy
