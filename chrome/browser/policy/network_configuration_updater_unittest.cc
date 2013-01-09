// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/network_configuration_updater.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service_impl.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_utils.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AtLeast;
using testing::Mock;
using testing::Ne;
using testing::Return;
using testing::_;

namespace policy {

static const char kFakeONC[] = "{ \"GUID\": \"1234\" }";

class NetworkConfigurationUpdaterTest
    : public testing::TestWithParam<const char*>{
 protected:
  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(provider_, IsInitializationComplete())
        .WillRepeatedly(Return(true));
    provider_.Init();
    PolicyServiceImpl::Providers providers;
    providers.push_back(&provider_);
    policy_service_.reset(new PolicyServiceImpl(providers));
  }

  virtual void TearDown() OVERRIDE {
    provider_.Shutdown();
  }

  void UpdateProviderPolicy(const PolicyMap& policy) {
    provider_.UpdateChromePolicy(policy);
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

  // Maps configuration policy name to corresponding ONC source.
  static chromeos::onc::ONCSource NameToONCSource(
      const std::string& name) {
    if (name == key::kDeviceOpenNetworkConfiguration)
      return chromeos::onc::ONC_SOURCE_DEVICE_POLICY;
    if (name == key::kOpenNetworkConfiguration)
      return chromeos::onc::ONC_SOURCE_USER_POLICY;
    return chromeos::onc::ONC_SOURCE_NONE;
  }

  chromeos::MockNetworkLibrary network_library_;
  MockConfigurationPolicyProvider provider_;
  scoped_ptr<PolicyServiceImpl> policy_service_;
  MessageLoop loop_;
};

TEST_P(NetworkConfigurationUpdaterTest, InitialUpdates) {
  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             Value::CreateStringValue(kFakeONC));
  UpdateProviderPolicy(policy);

  EXPECT_CALL(network_library_, AddNetworkProfileObserver(_));

  // Initially, only the device policy is applied. The user policy is only
  // applied after the user profile was initialized.
  const char* device_onc = GetParam() == key::kDeviceOpenNetworkConfiguration ?
      kFakeONC : chromeos::onc::kEmptyUnencryptedConfiguration;
  EXPECT_CALL(network_library_, LoadOncNetworks(
      device_onc, "", chromeos::onc::ONC_SOURCE_DEVICE_POLICY, _));

  {
    NetworkConfigurationUpdater updater(policy_service_.get(),
                                        &network_library_);
    Mock::VerifyAndClearExpectations(&network_library_);

    // After the user policy is initialized, we always push both policies to the
    // NetworkLibrary.
    EXPECT_CALL(network_library_, LoadOncNetworks(
        kFakeONC, "", NameToONCSource(GetParam()), _));
    EXPECT_CALL(network_library_, LoadOncNetworks(
        chromeos::onc::kEmptyUnencryptedConfiguration,
        "",
        Ne(NameToONCSource(GetParam())),
        _));

    EXPECT_CALL(network_library_, RemoveNetworkProfileObserver(_));

    updater.OnUserPolicyInitialized();
  }
  Mock::VerifyAndClearExpectations(&network_library_);
}

TEST_P(NetworkConfigurationUpdaterTest, AllowWebTrust) {
  {
    EXPECT_CALL(network_library_, AddNetworkProfileObserver(_));

    // Initially web trust is disabled.
    EXPECT_CALL(network_library_, LoadOncNetworks(_, _, _, false))
        .Times(AtLeast(0));
    NetworkConfigurationUpdater updater(policy_service_.get(),
                                        &network_library_);
    updater.OnUserPolicyInitialized();
    Mock::VerifyAndClearExpectations(&network_library_);

    // Web trust should be forwarded to LoadOncNetworks.
    EXPECT_CALL(network_library_, LoadOncNetworks(_, _, _, true))
        .Times(AtLeast(0));

    updater.set_allow_web_trust(true);

    PolicyMap policy;
    policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               Value::CreateStringValue(kFakeONC));
    UpdateProviderPolicy(policy);
    Mock::VerifyAndClearExpectations(&network_library_);

    EXPECT_CALL(network_library_, RemoveNetworkProfileObserver(_));
  }
  Mock::VerifyAndClearExpectations(&network_library_);
}

TEST_P(NetworkConfigurationUpdaterTest, PolicyChange) {
  {
    EXPECT_CALL(network_library_, AddNetworkProfileObserver(_));

    // Ignore the initial updates.
    EXPECT_CALL(network_library_, LoadOncNetworks(_, _, _, _))
        .Times(AtLeast(0));
    NetworkConfigurationUpdater updater(policy_service_.get(),
                                        &network_library_);
    updater.OnUserPolicyInitialized();
    Mock::VerifyAndClearExpectations(&network_library_);

    // We should update if policy changes.
    EXPECT_CALL(network_library_, LoadOncNetworks(
        kFakeONC, "", NameToONCSource(GetParam()), _));

    // In the current implementation, we always apply both policies.
    EXPECT_CALL(network_library_, LoadOncNetworks(
        chromeos::onc::kEmptyUnencryptedConfiguration,
        "",
        Ne(NameToONCSource(GetParam())),
        _));

    PolicyMap policy;
    policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               Value::CreateStringValue(kFakeONC));
    UpdateProviderPolicy(policy);
    Mock::VerifyAndClearExpectations(&network_library_);

    // Another update is expected if the policy goes away. In the current
    // implementation, we always apply both policies.
    EXPECT_CALL(network_library_, LoadOncNetworks(
        chromeos::onc::kEmptyUnencryptedConfiguration, "",
        chromeos::onc::ONC_SOURCE_DEVICE_POLICY, _));

    EXPECT_CALL(network_library_, LoadOncNetworks(
        chromeos::onc::kEmptyUnencryptedConfiguration, "",
        chromeos::onc::ONC_SOURCE_USER_POLICY, _));

    EXPECT_CALL(network_library_, RemoveNetworkProfileObserver(_));

    policy.Erase(GetParam());
    UpdateProviderPolicy(policy);
  }
  Mock::VerifyAndClearExpectations(&network_library_);
}

INSTANTIATE_TEST_CASE_P(
    NetworkConfigurationUpdaterTestInstance,
    NetworkConfigurationUpdaterTest,
    testing::Values(key::kDeviceOpenNetworkConfiguration,
                    key::kOpenNetworkConfiguration));

}  // namespace policy
