// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/network_configuration_updater.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service_impl.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::Return;
using testing::_;

namespace policy {

static const char kFakeONC[] = "{ \"GUID\": \"1234\" }";

class NetworkConfigurationUpdaterTest
    : public testing::TestWithParam<const char*> {
 protected:
  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(network_library_, LoadOncNetworks(_, "", _, _))
        .WillRepeatedly(Return(true));
    PolicyServiceImpl::Providers providers;
    providers.push_back(&provider_);
    policy_service_.reset(new PolicyServiceImpl(providers));
  }

  // Maps configuration policy name to corresponding ONC source.
  static chromeos::NetworkUIData::ONCSource NameToONCSource(
      const std::string& name) {
    if (name == key::kDeviceOpenNetworkConfiguration)
      return chromeos::NetworkUIData::ONC_SOURCE_DEVICE_POLICY;
    if (name == key::kOpenNetworkConfiguration)
      return chromeos::NetworkUIData::ONC_SOURCE_USER_POLICY;
    return chromeos::NetworkUIData::ONC_SOURCE_NONE;
  }

  chromeos::MockNetworkLibrary network_library_;
  MockConfigurationPolicyProvider provider_;
  scoped_ptr<PolicyServiceImpl> policy_service_;
};

TEST_P(NetworkConfigurationUpdaterTest, InitialUpdate) {
  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             Value::CreateStringValue(kFakeONC));
  provider_.UpdateChromePolicy(policy);

  EXPECT_CALL(network_library_,
              LoadOncNetworks(kFakeONC, "", NameToONCSource(GetParam()), _))
      .WillOnce(Return(true));

  NetworkConfigurationUpdater updater(policy_service_.get(), &network_library_);
  Mock::VerifyAndClearExpectations(&network_library_);
}

TEST_P(NetworkConfigurationUpdaterTest, PolicyChange) {
  NetworkConfigurationUpdater updater(policy_service_.get(), &network_library_);

  // We should update if policy changes.
  EXPECT_CALL(network_library_,
              LoadOncNetworks(kFakeONC, "", NameToONCSource(GetParam()), _))
      .WillOnce(Return(true));
  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             Value::CreateStringValue(kFakeONC));
  provider_.UpdateChromePolicy(policy);
  Mock::VerifyAndClearExpectations(&network_library_);

  // No update if the set the same value again.
  EXPECT_CALL(network_library_,
              LoadOncNetworks(kFakeONC, "", NameToONCSource(GetParam()), _))
      .Times(0);
  provider_.UpdateChromePolicy(policy);
  Mock::VerifyAndClearExpectations(&network_library_);

  // Another update is expected if the policy goes away.
  EXPECT_CALL(network_library_,
              LoadOncNetworks(NetworkConfigurationUpdater::kEmptyConfiguration,
                              "", NameToONCSource(GetParam()), _))
      .WillOnce(Return(true));
  policy.Erase(GetParam());
  provider_.UpdateChromePolicy(policy);
  Mock::VerifyAndClearExpectations(&network_library_);
}

INSTANTIATE_TEST_CASE_P(
    NetworkConfigurationUpdaterTestInstance,
    NetworkConfigurationUpdaterTest,
    testing::Values(key::kDeviceOpenNetworkConfiguration,
                    key::kOpenNetworkConfiguration));

}  // namespace policy
