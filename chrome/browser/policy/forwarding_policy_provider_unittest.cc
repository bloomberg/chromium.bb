// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/forwarding_policy_provider.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/browser/policy/schema_registry.h"
#include "components/policy/core/common/schema.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::Return;

namespace policy {

class ForwardingPolicyProviderTest : public testing::Test {
 protected:
  ForwardingPolicyProviderTest() : forwarding_provider_(&mock_provider_) {
    mock_provider_.Init();
    forwarding_provider_.Init(&schema_registry_);
    forwarding_provider_.AddObserver(&observer_);
  }

  virtual ~ForwardingPolicyProviderTest() {
    forwarding_provider_.RemoveObserver(&observer_);
    forwarding_provider_.Shutdown();
    mock_provider_.Shutdown();
  }

  SchemaRegistry schema_registry_;
  MockConfigurationPolicyObserver observer_;
  MockConfigurationPolicyProvider mock_provider_;
  ForwardingPolicyProvider forwarding_provider_;
};

TEST_F(ForwardingPolicyProviderTest, Empty) {
  EXPECT_FALSE(schema_registry_.IsReady());
  EXPECT_FALSE(
      forwarding_provider_.IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS));

  EXPECT_CALL(mock_provider_, IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .WillOnce(Return(false));
  EXPECT_FALSE(
      forwarding_provider_.IsInitializationComplete(POLICY_DOMAIN_CHROME));
  Mock::VerifyAndClearExpectations(&mock_provider_);

  const PolicyBundle empty_bundle;
  EXPECT_TRUE(forwarding_provider_.policies().Equals(empty_bundle));
}

TEST_F(ForwardingPolicyProviderTest, ForwardsChromePolicy) {
  PolicyBundle bundle;
  const PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, "");
  bundle.Get(chrome_ns).Set("policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                            base::Value::CreateStringValue("value"), NULL);

  EXPECT_CALL(observer_, OnUpdatePolicy(&forwarding_provider_));
  scoped_ptr<PolicyBundle> delegate_bundle(new PolicyBundle);
  delegate_bundle->CopyFrom(bundle);
  delegate_bundle->Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "xyz"))
      .Set("component policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           base::Value::CreateStringValue("not forwarded"), NULL);
  mock_provider_.UpdatePolicy(delegate_bundle.Pass());
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_FALSE(
      forwarding_provider_.IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS));
  EXPECT_TRUE(forwarding_provider_.policies().Equals(bundle));
}

TEST_F(ForwardingPolicyProviderTest, RefreshPolicies) {
  EXPECT_CALL(mock_provider_, RefreshPolicies());
  forwarding_provider_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&mock_provider_);
}

TEST_F(ForwardingPolicyProviderTest, SchemaReady) {
  EXPECT_CALL(observer_, OnUpdatePolicy(&forwarding_provider_));
  schema_registry_.SetReady(POLICY_DOMAIN_CHROME);
  schema_registry_.SetReady(POLICY_DOMAIN_EXTENSIONS);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(forwarding_provider_.IsInitializationComplete(
      policy::POLICY_DOMAIN_EXTENSIONS));
}

TEST_F(ForwardingPolicyProviderTest, SchemaReadyWithComponents) {
  PolicyMap policy_map;
  policy_map.Set("foo", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("omg"), NULL);
  scoped_ptr<PolicyBundle> bundle(new PolicyBundle);
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, "")).CopyFrom(policy_map);
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "xyz"))
      .CopyFrom(policy_map);
  EXPECT_CALL(observer_, OnUpdatePolicy(&forwarding_provider_));
  mock_provider_.UpdatePolicy(bundle.Pass());
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(mock_provider_, RefreshPolicies()).Times(0);
  schema_registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "xyz"), Schema());
  schema_registry_.SetReady(POLICY_DOMAIN_EXTENSIONS);
  Mock::VerifyAndClearExpectations(&mock_provider_);

  EXPECT_CALL(mock_provider_, RefreshPolicies());
  schema_registry_.SetReady(POLICY_DOMAIN_CHROME);
  Mock::VerifyAndClearExpectations(&mock_provider_);

  EXPECT_FALSE(forwarding_provider_.IsInitializationComplete(
      policy::POLICY_DOMAIN_EXTENSIONS));
  PolicyBundle expected_bundle;
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, ""))
      .CopyFrom(policy_map);
  EXPECT_TRUE(forwarding_provider_.policies().Equals(expected_bundle));

  EXPECT_CALL(observer_, OnUpdatePolicy(&forwarding_provider_));
  forwarding_provider_.OnUpdatePolicy(&mock_provider_);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(forwarding_provider_.IsInitializationComplete(
      policy::POLICY_DOMAIN_EXTENSIONS));
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "xyz"))
      .CopyFrom(policy_map);
  EXPECT_TRUE(forwarding_provider_.policies().Equals(expected_bundle));
}

TEST_F(ForwardingPolicyProviderTest, DelegateUpdates) {
  schema_registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, "xyz"), Schema());
  EXPECT_FALSE(schema_registry_.IsReady());
  EXPECT_FALSE(forwarding_provider_.IsInitializationComplete(
      policy::POLICY_DOMAIN_EXTENSIONS));

  PolicyMap policy_map;
  policy_map.Set("foo", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("omg"), NULL);
  // Chrome policy updates are forwarded even if the components aren't ready.
  EXPECT_CALL(observer_, OnUpdatePolicy(&forwarding_provider_));
  mock_provider_.UpdateChromePolicy(policy_map);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(mock_provider_, RefreshPolicies());
  schema_registry_.SetReady(POLICY_DOMAIN_CHROME);
  schema_registry_.SetReady(POLICY_DOMAIN_EXTENSIONS);
  EXPECT_TRUE(schema_registry_.IsReady());
  Mock::VerifyAndClearExpectations(&mock_provider_);
  EXPECT_FALSE(forwarding_provider_.IsInitializationComplete(
      policy::POLICY_DOMAIN_EXTENSIONS));

  // The forwarding provider becomes ready after this refresh completes, and
  // starts forwarding policy updates after that.
  EXPECT_CALL(observer_, OnUpdatePolicy(_));
  mock_provider_.UpdateChromePolicy(policy_map);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_TRUE(forwarding_provider_.IsInitializationComplete(
      policy::POLICY_DOMAIN_EXTENSIONS));

  // Keeps forwarding.
  EXPECT_CALL(observer_, OnUpdatePolicy(_));
  mock_provider_.UpdateChromePolicy(policy_map);
  Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace policy
