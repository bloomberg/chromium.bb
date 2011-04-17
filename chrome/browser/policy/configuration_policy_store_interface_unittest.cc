// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_store_interface.h"
#include "chrome/browser/policy/mock_configuration_policy_store.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace policy {

TEST(ConfigurationPolicyStoreInterfaceTest, Observer) {
  MockConfigurationPolicyStore store;
  EXPECT_CALL(store, Apply(_, _)).Times(3);
  ObservingPolicyStoreInterface observer(&store);

  EXPECT_FALSE(observer.IsProxyPolicyApplied());
  observer.Apply(kPolicyJavascriptEnabled, Value::CreateBooleanValue(true));
  EXPECT_FALSE(observer.IsProxyPolicyApplied());
  observer.Apply(kPolicyProxyMode, Value::CreateStringValue("direct"));
  EXPECT_TRUE(observer.IsProxyPolicyApplied());
  observer.Apply(kPolicyIncognitoEnabled, Value::CreateBooleanValue(true));
  EXPECT_TRUE(observer.IsProxyPolicyApplied());

  EXPECT_TRUE(store.Get(kPolicyJavascriptEnabled) != NULL);
  EXPECT_TRUE(store.Get(kPolicyProxyMode) != NULL);
  EXPECT_TRUE(store.Get(kPolicyIncognitoEnabled) != NULL);
  EXPECT_TRUE(store.Get(kPolicyPrintingEnabled) == NULL);
}

TEST(ConfigurationPolicyStoreInterfaceTest, Filter) {
  MockConfigurationPolicyStore store_pass;
  EXPECT_CALL(store_pass, Apply(_, _)).Times(1);
  FilteringPolicyStoreInterface filter_pass(&store_pass, true);

  EXPECT_TRUE(store_pass.policy_map().empty());
  filter_pass.Apply(kPolicyJavascriptEnabled, Value::CreateBooleanValue(true));
  EXPECT_TRUE(store_pass.policy_map().empty());
  filter_pass.Apply(kPolicyProxyMode, Value::CreateStringValue("direct"));
  EXPECT_FALSE(store_pass.policy_map().empty());
  EXPECT_EQ(store_pass.policy_map().size(), 1u);
  EXPECT_TRUE(store_pass.Get(kPolicyJavascriptEnabled) == NULL);
  EXPECT_TRUE(store_pass.Get(kPolicyProxyMode) != NULL);

  MockConfigurationPolicyStore store_block;
  EXPECT_CALL(store_block, Apply(_, _)).Times(0);
  FilteringPolicyStoreInterface filter_block(&store_block, false);

  EXPECT_TRUE(store_block.policy_map().empty());
  filter_block.Apply(kPolicyJavascriptEnabled, Value::CreateBooleanValue(true));
  EXPECT_TRUE(store_block.policy_map().empty());
  filter_block.Apply(kPolicyProxyMode, Value::CreateStringValue("direct"));
  EXPECT_TRUE(store_block.policy_map().empty());
}

}  // namespace policy
