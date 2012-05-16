// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/policy/asynchronous_policy_loader.h"
#include "chrome/browser/policy/asynchronous_policy_provider.h"
#include "chrome/browser/policy/asynchronous_policy_test_base.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;

namespace policy {

// Creating the provider should provide initial policy.
TEST_F(AsynchronousPolicyTestBase, Provide) {
  InSequence s;
  PolicyBundle bundle;
  bundle.Get(POLICY_DOMAIN_CHROME, "")
      .Set(key::kSyncDisabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           base::Value::CreateBooleanValue(true));
  ProviderDelegateMock* delegate = new ProviderDelegateMock();
  EXPECT_CALL(*delegate, MockLoad()).WillOnce(Return(&bundle));
  AsynchronousPolicyProvider provider(
      GetChromePolicyDefinitionList(),
      new AsynchronousPolicyLoader(delegate, 10));
  EXPECT_TRUE(provider.policies().Equals(bundle));
}

// Trigger a refresh manually and ensure that policy gets reloaded.
TEST_F(AsynchronousPolicyTestBase, ProvideAfterRefresh) {
  InSequence s;
  PolicyBundle original_policies;
  original_policies.Get(POLICY_DOMAIN_CHROME, "")
      .Set(key::kSyncDisabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           base::Value::CreateBooleanValue(true));
  ProviderDelegateMock* delegate = new ProviderDelegateMock();
  EXPECT_CALL(*delegate, MockLoad()).WillOnce(Return(&original_policies));
  PolicyBundle refresh_policies;
  refresh_policies.Get(POLICY_DOMAIN_CHROME, "")
      .Set(key::kJavascriptEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           base::Value::CreateBooleanValue(true));
  EXPECT_CALL(*delegate, MockLoad()).WillOnce(Return(&refresh_policies));
  AsynchronousPolicyLoader* loader = new AsynchronousPolicyLoader(delegate, 10);
  AsynchronousPolicyProvider provider(GetChromePolicyDefinitionList(), loader);
  // The original policies have been loaded.
  EXPECT_TRUE(provider.policies().Equals(original_policies));

  MockConfigurationPolicyObserver observer;
  ConfigurationPolicyObserverRegistrar registrar;
  registrar.Init(&provider, &observer);
  EXPECT_CALL(observer, OnUpdatePolicy(&provider)).Times(1);
  provider.RefreshPolicies();
  loop_.RunAllPending();
  // The refreshed policies are now provided.
  EXPECT_TRUE(provider.policies().Equals(refresh_policies));
}

}  // namespace policy
