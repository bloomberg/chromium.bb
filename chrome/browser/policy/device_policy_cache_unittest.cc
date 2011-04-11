// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_policy_cache.h"

#include "chrome/browser/policy/device_policy_identity_strategy.h"
#include "policy/configuration_policy_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

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

em::PolicyFetchResponse* CreateProxyPolicy(const std::string& proxy) {
  // This method omits a few fields which currently aren't needed by tests:
  // timestamp, machine_name, request_token, policy_type, public key info.
  em::PolicyData signed_response;
  em::ChromeDeviceSettingsProto settings;
  em::DeviceProxySettingsProto* proxy_proto =
      settings.mutable_device_proxy_settings();
  proxy_proto->set_proxy_server(proxy);
  proxy_proto->set_proxy_mode("fixed_servers");
  EXPECT_TRUE(
      settings.SerializeToString(signed_response.mutable_policy_value()));
  std::string serialized_signed_response;
  EXPECT_TRUE(signed_response.SerializeToString(&serialized_signed_response));
  em::PolicyFetchResponse* response = new em::PolicyFetchResponse;
  response->set_policy_data(serialized_signed_response);
  return response;
}

}  // namespace

class DevicePolicyCacheTest : public testing::Test {
 protected:
  DevicePolicyCacheTest() {
  }

  virtual void SetUp() {
    cache_.reset(new DevicePolicyCache(&identity_strategy_,
                                       &signed_settings_helper_));
  }

  virtual void TearDown() {
    EXPECT_CALL(signed_settings_helper_, CancelCallback(_));
    cache_.reset();
  }

  const PolicyMap& mandatory_policy(const DevicePolicyCache& cache) {
    return cache.mandatory_policy_;
  }

  scoped_ptr<DevicePolicyCache> cache_;
  DevicePolicyIdentityStrategy identity_strategy_;
  MockSignedSettingsHelper signed_settings_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCacheTest);
};

TEST_F(DevicePolicyCacheTest, Startup) {
  scoped_ptr<em::PolicyFetchResponse> policy_response(
      CreateProxyPolicy("proxy.server"));
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_)).WillOnce(
      MockSignedSettingsHelperRetrievePolicy(SignedSettings::SUCCESS,
                                             *policy_response));
  cache_->Load();
  // TODO(jkummerow): This will be EXPECT_GT once policy decoding is
  // implemented in DevicePolicyCache::DecodeDevicePolicy(...).
  EXPECT_EQ(mandatory_policy(*cache_).size(), 0U);
}

TEST_F(DevicePolicyCacheTest, SetPolicy) {
  InSequence s;
  // Startup.
  scoped_ptr<em::PolicyFetchResponse> policy_response(
      CreateProxyPolicy("proxy.server.old"));
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_)).WillOnce(
      MockSignedSettingsHelperRetrievePolicy(SignedSettings::SUCCESS,
                                             *policy_response));
  cache_->Load();
  scoped_ptr<Value> expected(Value::CreateStringValue("proxy.server.old"));
  // TODO(jkummerow): This will be EXPECT_TRUE once policy decoding is
  // implemented in DevicePolicyCache::DecodeDevicePolicy(...).
  EXPECT_FALSE(Value::Equals(
      mandatory_policy(*cache_).Get(kPolicyProxyServer), expected.get()));
  testing::Mock::VerifyAndClearExpectations(&signed_settings_helper_);
  // Set new policy information.
  scoped_ptr<em::PolicyFetchResponse> new_policy_response(
      CreateProxyPolicy("proxy.server.new"));
  EXPECT_CALL(signed_settings_helper_, StartStorePolicyOp(_, _)).WillOnce(
      MockSignedSettingsHelperStorePolicy(chromeos::SignedSettings::SUCCESS));
  EXPECT_CALL(signed_settings_helper_, StartRetrievePolicyOp(_)).WillOnce(
      MockSignedSettingsHelperRetrievePolicy(SignedSettings::SUCCESS,
                                             *new_policy_response));
  cache_->SetPolicy(*new_policy_response);
  expected.reset(Value::CreateStringValue("proxy.server.new"));
  // TODO(jkummerow): This will be EXPECT_TRUE once policy decoding is
  // implemented in DevicePolicyCache::DecodeDevicePolicy(...).
  EXPECT_FALSE(Value::Equals(
      mandatory_policy(*cache_).Get(kPolicyProxyServer), expected.get()));
}

}  // namespace policy
