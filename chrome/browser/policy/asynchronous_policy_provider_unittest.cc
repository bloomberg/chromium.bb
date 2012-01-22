// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  DictionaryValue* policies = new DictionaryValue();
  policies->SetBoolean(key::kSyncDisabled, true);
  ProviderDelegateMock* delegate = new ProviderDelegateMock();
  EXPECT_CALL(*delegate, Load()).WillOnce(Return(policies));
  AsynchronousPolicyProvider provider(
      GetChromePolicyDefinitionList(),
      POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER,
      new AsynchronousPolicyLoader(delegate, 10));
  PolicyMap policy_map;
  provider.Provide(&policy_map);
  EXPECT_TRUE(policy_map.Get(key::kSyncDisabled));
  EXPECT_EQ(1U, policy_map.size());
}


// Trigger a refresh manually and ensure that policy gets reloaded.
TEST_F(AsynchronousPolicyTestBase, ProvideAfterRefresh) {
  InSequence s;
  DictionaryValue* original_policies = new DictionaryValue();
  original_policies->SetBoolean(key::kSyncDisabled, true);
  ProviderDelegateMock* delegate = new ProviderDelegateMock();
  EXPECT_CALL(*delegate, Load()).WillOnce(Return(original_policies));
  DictionaryValue* refresh_policies = new DictionaryValue();
  refresh_policies->SetBoolean(key::kJavascriptEnabled, true);
  EXPECT_CALL(*delegate, Load()).WillOnce(Return(refresh_policies));
  AsynchronousPolicyLoader* loader = new AsynchronousPolicyLoader(delegate, 10);
  AsynchronousPolicyProvider provider(GetChromePolicyDefinitionList(),
                                      POLICY_LEVEL_MANDATORY,
                                      POLICY_SCOPE_USER,
                                      loader);
  // The original policies have been loaded.
  PolicyMap policy_map;
  provider.Provide(&policy_map);
  EXPECT_EQ(1U, policy_map.size());
  EXPECT_TRUE(policy_map.Get(key::kSyncDisabled));
  EXPECT_FALSE(policy_map.Get(key::kJavascriptEnabled));

  MockConfigurationPolicyObserver observer;
  ConfigurationPolicyObserverRegistrar registrar;
  registrar.Init(&provider, &observer);
  EXPECT_CALL(observer, OnUpdatePolicy(&provider)).Times(1);
  provider.RefreshPolicies();
  loop_.RunAllPending();
  // The refreshed policies are now provided.
  policy_map.Clear();
  provider.Provide(&policy_map);
  EXPECT_EQ(1U, policy_map.size());
  EXPECT_FALSE(policy_map.Get(key::kSyncDisabled));
  EXPECT_TRUE(policy_map.Get(key::kJavascriptEnabled));
}

}  // namespace policy
