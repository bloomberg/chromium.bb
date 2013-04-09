// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_map.h"

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Utility function for the tests.
void SetPolicy(PolicyMap* map, const char* name, Value* value) {
  map->Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, value);
}

TEST(PolicyMapTest, SetAndGet) {
  PolicyMap map;
  SetPolicy(&map, key::kHomepageLocation, Value::CreateStringValue("aaa"));
  StringValue expected("aaa");
  EXPECT_TRUE(expected.Equals(map.GetValue(key::kHomepageLocation)));
  SetPolicy(&map, key::kHomepageLocation, Value::CreateStringValue("bbb"));
  StringValue expected_b("bbb");
  EXPECT_TRUE(expected_b.Equals(map.GetValue(key::kHomepageLocation)));
  const PolicyMap::Entry* entry = map.Get(key::kHomepageLocation);
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(POLICY_LEVEL_MANDATORY, entry->level);
  EXPECT_EQ(POLICY_SCOPE_USER, entry->scope);
  map.Set(key::kHomepageLocation, POLICY_LEVEL_RECOMMENDED,
          POLICY_SCOPE_MACHINE, NULL);
  entry = map.Get(key::kHomepageLocation);
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(POLICY_LEVEL_RECOMMENDED, entry->level);
  EXPECT_EQ(POLICY_SCOPE_MACHINE, entry->scope);
}

TEST(PolicyMapTest, Equals) {
  PolicyMap a;
  SetPolicy(&a, key::kHomepageLocation, Value::CreateStringValue("aaa"));
  PolicyMap a2;
  SetPolicy(&a2, key::kHomepageLocation, Value::CreateStringValue("aaa"));
  PolicyMap b;
  SetPolicy(&b, key::kHomepageLocation, Value::CreateStringValue("bbb"));
  PolicyMap c;
  SetPolicy(&c, key::kHomepageLocation, Value::CreateStringValue("aaa"));
  SetPolicy(&c, key::kHomepageIsNewTabPage, Value::CreateBooleanValue(true));
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
  SetPolicy(&a, key::kHomepageLocation, Value::CreateStringValue("aaa"));
  PolicyMap b;
  SetPolicy(&b, key::kHomepageLocation, Value::CreateStringValue("bbb"));
  SetPolicy(&b, key::kHomepageIsNewTabPage, Value::CreateBooleanValue(true));
  a.Swap(&b);
  base::StringValue expected("bbb");
  EXPECT_TRUE(expected.Equals(a.GetValue(key::kHomepageLocation)));
  base::FundamentalValue expected_bool(true);
  EXPECT_TRUE(expected_bool.Equals(a.GetValue(key::kHomepageIsNewTabPage)));
  StringValue expected_a("aaa");
  EXPECT_TRUE(expected_a.Equals(b.GetValue(key::kHomepageLocation)));

  b.Clear();
  a.Swap(&b);
  PolicyMap empty;
  EXPECT_TRUE(a.Equals(empty));
  EXPECT_FALSE(b.Equals(empty));
}

TEST(PolicyMapTest, MergeFrom) {
  PolicyMap a;
  a.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        Value::CreateStringValue("google.com"));
  a.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        Value::CreateBooleanValue(true));
  a.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
        Value::CreateBooleanValue(false));
  a.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_MACHINE, Value::CreateStringValue("google.com/q={x}"));

  PolicyMap b;
  b.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        Value::CreateStringValue("chromium.org"));
  b.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        Value::CreateBooleanValue(false));
  b.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_MACHINE, Value::CreateBooleanValue(true));
  b.Set(key::kDefaultSearchProviderSearchURL,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        Value::CreateStringValue(std::string()));
  b.Set(key::kDisableSpdy,
        POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_USER,
        Value::CreateBooleanValue(true));

  a.MergeFrom(b);

  PolicyMap c;
  // POLICY_SCOPE_MACHINE over POLICY_SCOPE_USER.
  c.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        Value::CreateStringValue("chromium.org"));
  // |a| has precedence over |b|.
  c.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        Value::CreateBooleanValue(true));
  // POLICY_SCOPE_MACHINE over POLICY_SCOPE_USER for POLICY_LEVEL_RECOMMENDED.
  c.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_MACHINE, Value::CreateBooleanValue(true));
  // POLICY_LEVEL_MANDATORY over POLICY_LEVEL_RECOMMENDED.
  c.Set(key::kDefaultSearchProviderSearchURL,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        Value::CreateStringValue(std::string()));
  // Merge new ones.
  c.Set(key::kDisableSpdy, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
        Value::CreateBooleanValue(true));

  EXPECT_TRUE(a.Equals(c));
}

TEST(PolicyMapTest, GetDifferingKeys) {
  PolicyMap a;
  a.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        Value::CreateStringValue("google.com"));
  a.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        Value::CreateBooleanValue(true));
  a.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
        Value::CreateBooleanValue(false));
  a.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_MACHINE, Value::CreateStringValue("google.com/q={x}"));
  a.Set(key::kDisable3DAPIs, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        Value::CreateBooleanValue(true));

  PolicyMap b;
  b.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        Value::CreateStringValue("google.com"));
  b.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        Value::CreateBooleanValue(false));
  b.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        Value::CreateBooleanValue(false));
  b.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_USER, Value::CreateStringValue("google.com/q={x}"));
  b.Set(key::kDisableSpdy, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
        Value::CreateBooleanValue(true));

  std::set<std::string> diff;
  std::set<std::string> diff2;
  a.GetDifferingKeys(b, &diff);
  b.GetDifferingKeys(a, &diff2);
  // Order shouldn't matter.
  EXPECT_EQ(diff, diff2);
  // No change.
  EXPECT_TRUE(diff.find(key::kHomepageLocation) == diff.end());
  // Different values.
  EXPECT_TRUE(diff.find(key::kShowHomeButton) != diff.end());
  // Different levels.
  EXPECT_TRUE(diff.find(key::kBookmarkBarEnabled) != diff.end());
  // Different scopes.
  EXPECT_TRUE(diff.find(key::kDefaultSearchProviderSearchURL) != diff.end());
  // Not in |a|.
  EXPECT_TRUE(diff.find(key::kDisableSpdy) != diff.end());
  // Not in |b|.
  EXPECT_TRUE(diff.find(key::kDisable3DAPIs) != diff.end());
  // No surprises.
  EXPECT_EQ(5u, diff.size());
}

}  // namespace

}  // namespace policy
