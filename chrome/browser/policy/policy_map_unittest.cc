// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_map.h"

#include "base/memory/scoped_ptr.h"
#include "policy/configuration_policy_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(PolicyMapTest, SetAndGet) {
  PolicyMap map;
  map.Set(kPolicyHomepageLocation, Value::CreateStringValue("aaa"));
  StringValue expected("aaa");
  EXPECT_TRUE(expected.Equals(map.Get(kPolicyHomepageLocation)));
  map.Set(kPolicyHomepageLocation, Value::CreateStringValue("bbb"));
  StringValue expected_b("bbb");
  EXPECT_TRUE(expected_b.Equals(map.Get(kPolicyHomepageLocation)));
}

TEST(PolicyMapTest, Equals) {
  PolicyMap a;
  a.Set(kPolicyHomepageLocation, Value::CreateStringValue("aaa"));
  PolicyMap a2;
  a2.Set(kPolicyHomepageLocation, Value::CreateStringValue("aaa"));
  PolicyMap b;
  b.Set(kPolicyHomepageLocation, Value::CreateStringValue("bbb"));
  PolicyMap c;
  c.Set(kPolicyHomepageLocation, Value::CreateStringValue("aaa"));
  c.Set(kPolicyHomepageIsNewTabPage, Value::CreateBooleanValue(true));
  EXPECT_FALSE(a.Equals(b));
  EXPECT_FALSE(b.Equals(a));
  EXPECT_FALSE(a.Equals(c));
  EXPECT_FALSE(c.Equals(a));
  EXPECT_TRUE(a.Equals(a2));
  EXPECT_TRUE(a2.Equals(a));
  PolicyMap empty1;
  PolicyMap empty2;
  EXPECT_TRUE(empty1.Equals(empty2));
  EXPECT_TRUE(empty2.Equals(empty1));
  EXPECT_FALSE(empty1.Equals(a));
  EXPECT_FALSE(a.Equals(empty1));
}

TEST(PolicyMapTest, Swap) {
  PolicyMap a;
  a.Set(kPolicyHomepageLocation, Value::CreateStringValue("aaa"));
  PolicyMap b;
  b.Set(kPolicyHomepageLocation, Value::CreateStringValue("bbb"));
  b.Set(kPolicyHomepageIsNewTabPage, Value::CreateBooleanValue(true));
  a.Swap(&b);
  StringValue expected("bbb");
  EXPECT_TRUE(expected.Equals(a.Get(kPolicyHomepageLocation)));
  FundamentalValue expected_bool(true);
  EXPECT_TRUE(expected_bool.Equals(a.Get(kPolicyHomepageIsNewTabPage)));
  StringValue expected_a("aaa");
  EXPECT_TRUE(expected_a.Equals(b.Get(kPolicyHomepageLocation)));

  b.Clear();
  a.Swap(&b);
  PolicyMap empty;
  EXPECT_TRUE(a.Equals(empty));
  EXPECT_FALSE(b.Equals(empty));
}

}  // namespace policy
