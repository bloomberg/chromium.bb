// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/mock_configuration_policy_store.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace policy {

TEST(MergingPolicyProviderTest, MergeProxySettings) {
  MockConfigurationPolicyProvider browser_provider;
  MockConfigurationPolicyProvider profile_provider;
  MergingPolicyProvider merging_provider(&browser_provider, &profile_provider);

  // First, test settings from profile and no proxy settings from browser.
  // Only the profile policies should be forwarded.
  browser_provider.AddPolicy(kPolicyJavascriptEnabled,
                             Value::CreateBooleanValue(true));
  profile_provider.AddPolicy(kPolicyPrintingEnabled,
                             Value::CreateBooleanValue(true));

  MockConfigurationPolicyStore store0;
  EXPECT_CALL(store0, Apply(_, _)).Times(1);
  EXPECT_TRUE(merging_provider.Provide(&store0));
  EXPECT_EQ(store0.policy_map().size(), 1u);
  EXPECT_TRUE(store0.Get(kPolicyPrintingEnabled) != NULL);

  // Now have a proxy policy that should be merged.
  browser_provider.AddPolicy(kPolicyProxyMode,
                             Value::CreateStringValue("direct"));

  MockConfigurationPolicyStore store1;
  EXPECT_CALL(store1, Apply(_, _)).Times(2);
  EXPECT_TRUE(merging_provider.Provide(&store1));
  EXPECT_EQ(store1.policy_map().size(), 2u);
  EXPECT_TRUE(store1.Get(kPolicyPrintingEnabled) != NULL);
  EXPECT_TRUE(store1.Get(kPolicyProxyMode) != NULL);

  // If the profile includes any proxy policy, no proxy policies should be
  // merged from the browser provider.
  profile_provider.AddPolicy(kPolicyProxyServer,
                             Value::CreateStringValue("http://proxy:8080"));

  MockConfigurationPolicyStore store2;
  EXPECT_CALL(store2, Apply(_, _)).Times(2);
  EXPECT_TRUE(merging_provider.Provide(&store2));
  EXPECT_EQ(store2.policy_map().size(), 2u);
  EXPECT_TRUE(store2.Get(kPolicyPrintingEnabled) != NULL);
  EXPECT_TRUE(store2.Get(kPolicyProxyServer) != NULL);
  EXPECT_TRUE(store2.Get(kPolicyProxyMode) == NULL);
}

}  // namespace policy
