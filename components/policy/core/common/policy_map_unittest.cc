// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_map.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/external_data_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Dummy policy names.
const char kTestPolicyName1[] = "policy.test.1";
const char kTestPolicyName2[] = "policy.test.2";
const char kTestPolicyName3[] = "policy.test.3";
const char kTestPolicyName4[] = "policy.test.4";
const char kTestPolicyName5[] = "policy.test.5";
const char kTestPolicyName6[] = "policy.test.6";
const char kTestPolicyName7[] = "policy.test.7";
const char kTestPolicyName8[] = "policy.test.8";

// Utility functions for the tests.
void SetPolicy(PolicyMap* map, const char* name, base::Value* value) {
  map->Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, value, NULL);
}

void SetPolicy(PolicyMap* map,
               const char* name,
               ExternalDataFetcher* external_data_fetcher) {
  map->Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           NULL, external_data_fetcher);
}

}  // namespace

class PolicyMapTest : public testing::Test {
 protected:
  scoped_ptr<ExternalDataFetcher> CreateExternalDataFetcher(
      const std::string& policy) const;
};

scoped_ptr<ExternalDataFetcher> PolicyMapTest::CreateExternalDataFetcher(
    const std::string& policy) const {
  return make_scoped_ptr(
      new ExternalDataFetcher(base::WeakPtr<ExternalDataManager>(), policy));
}

TEST_F(PolicyMapTest, SetAndGet) {
  PolicyMap map;
  SetPolicy(&map, kTestPolicyName1, new base::StringValue("aaa"));
  base::StringValue expected("aaa");
  EXPECT_TRUE(expected.Equals(map.GetValue(kTestPolicyName1)));
  SetPolicy(&map, kTestPolicyName1, new base::StringValue("bbb"));
  base::StringValue expected_b("bbb");
  EXPECT_TRUE(expected_b.Equals(map.GetValue(kTestPolicyName1)));
  SetPolicy(&map, kTestPolicyName1,
            CreateExternalDataFetcher("dummy").release());
  EXPECT_FALSE(map.GetValue(kTestPolicyName1));
  const PolicyMap::Entry* entry = map.Get(kTestPolicyName1);
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(POLICY_LEVEL_MANDATORY, entry->level);
  EXPECT_EQ(POLICY_SCOPE_USER, entry->scope);
  EXPECT_TRUE(ExternalDataFetcher::Equals(
      entry->external_data_fetcher, CreateExternalDataFetcher("dummy").get()));
  map.Set(kTestPolicyName1, POLICY_LEVEL_RECOMMENDED,
          POLICY_SCOPE_MACHINE, NULL, NULL);
  EXPECT_FALSE(map.GetValue(kTestPolicyName1));
  entry = map.Get(kTestPolicyName1);
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(POLICY_LEVEL_RECOMMENDED, entry->level);
  EXPECT_EQ(POLICY_SCOPE_MACHINE, entry->scope);
  EXPECT_FALSE(entry->external_data_fetcher);
}

TEST_F(PolicyMapTest, Equals) {
  PolicyMap a;
  SetPolicy(&a, kTestPolicyName1, new base::StringValue("aaa"));
  PolicyMap a2;
  SetPolicy(&a2, kTestPolicyName1, new base::StringValue("aaa"));
  PolicyMap b;
  SetPolicy(&b, kTestPolicyName1, new base::StringValue("bbb"));
  PolicyMap c;
  SetPolicy(&c, kTestPolicyName1, new base::StringValue("aaa"));
  SetPolicy(&c, kTestPolicyName2, new base::FundamentalValue(true));
  PolicyMap d;
  SetPolicy(&d, kTestPolicyName1,
            CreateExternalDataFetcher("ddd").release());
  PolicyMap d2;
  SetPolicy(&d2, kTestPolicyName1,
            CreateExternalDataFetcher("ddd").release());
  PolicyMap e;
  SetPolicy(&e, kTestPolicyName1,
            CreateExternalDataFetcher("eee").release());
  EXPECT_FALSE(a.Equals(b));
  EXPECT_FALSE(a.Equals(c));
  EXPECT_FALSE(a.Equals(d));
  EXPECT_FALSE(a.Equals(e));
  EXPECT_FALSE(b.Equals(a));
  EXPECT_FALSE(b.Equals(c));
  EXPECT_FALSE(b.Equals(d));
  EXPECT_FALSE(b.Equals(e));
  EXPECT_FALSE(c.Equals(a));
  EXPECT_FALSE(c.Equals(b));
  EXPECT_FALSE(c.Equals(d));
  EXPECT_FALSE(c.Equals(e));
  EXPECT_FALSE(d.Equals(a));
  EXPECT_FALSE(d.Equals(b));
  EXPECT_FALSE(d.Equals(c));
  EXPECT_FALSE(d.Equals(e));
  EXPECT_FALSE(e.Equals(a));
  EXPECT_FALSE(e.Equals(b));
  EXPECT_FALSE(e.Equals(c));
  EXPECT_FALSE(e.Equals(d));
  EXPECT_TRUE(a.Equals(a2));
  EXPECT_TRUE(a2.Equals(a));
  EXPECT_TRUE(d.Equals(d2));
  EXPECT_TRUE(d2.Equals(d));
  PolicyMap empty1;
  PolicyMap empty2;
  EXPECT_TRUE(empty1.Equals(empty2));
  EXPECT_TRUE(empty2.Equals(empty1));
  EXPECT_FALSE(empty1.Equals(a));
  EXPECT_FALSE(a.Equals(empty1));
}

TEST_F(PolicyMapTest, Swap) {
  PolicyMap a;
  SetPolicy(&a, kTestPolicyName1, new base::StringValue("aaa"));
  SetPolicy(&a, kTestPolicyName2,
            CreateExternalDataFetcher("dummy").release());
  PolicyMap b;
  SetPolicy(&b, kTestPolicyName1, new base::StringValue("bbb"));
  SetPolicy(&b, kTestPolicyName3, new base::FundamentalValue(true));

  a.Swap(&b);
  base::StringValue expected("bbb");
  EXPECT_TRUE(expected.Equals(a.GetValue(kTestPolicyName1)));
  base::FundamentalValue expected_bool(true);
  EXPECT_TRUE(expected_bool.Equals(a.GetValue(kTestPolicyName3)));
  EXPECT_FALSE(a.GetValue(kTestPolicyName2));
  EXPECT_FALSE(a.Get(kTestPolicyName2));
  base::StringValue expected_a("aaa");
  EXPECT_TRUE(expected_a.Equals(b.GetValue(kTestPolicyName1)));
  EXPECT_FALSE(b.GetValue(kTestPolicyName3));
  EXPECT_FALSE(a.GetValue(kTestPolicyName2));
  const PolicyMap::Entry* entry = b.Get(kTestPolicyName2);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(ExternalDataFetcher::Equals(
      CreateExternalDataFetcher("dummy").get(), entry->external_data_fetcher));

  b.Clear();
  a.Swap(&b);
  PolicyMap empty;
  EXPECT_TRUE(a.Equals(empty));
  EXPECT_FALSE(b.Equals(empty));
}

TEST_F(PolicyMapTest, MergeFrom) {
  PolicyMap a;
  a.Set(kTestPolicyName1,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_USER,
        new base::StringValue("google.com"),
        NULL);
  a.Set(kTestPolicyName2,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        new base::FundamentalValue(true),
        NULL);
  a.Set(kTestPolicyName3,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        NULL, CreateExternalDataFetcher("a").release());
  a.Set(kTestPolicyName4,
        POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_USER,
        new base::FundamentalValue(false),
        NULL);
  a.Set(kTestPolicyName5,
        POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_MACHINE,
        new base::StringValue("google.com/q={x}"),
        NULL);

  PolicyMap b;
  b.Set(kTestPolicyName1,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        new base::StringValue("chromium.org"),
        NULL);
  b.Set(kTestPolicyName2,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        new base::FundamentalValue(false),
        NULL);
  b.Set(kTestPolicyName3,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        NULL, CreateExternalDataFetcher("b").release());
  b.Set(kTestPolicyName4,
        POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_MACHINE,
        new base::FundamentalValue(true),
        NULL);
  b.Set(kTestPolicyName5,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        new base::StringValue(std::string()),
        NULL);
  b.Set(kTestPolicyName6,
        POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_USER,
        new base::FundamentalValue(true),
        NULL);

  a.MergeFrom(b);

  PolicyMap c;
  // POLICY_SCOPE_MACHINE over POLICY_SCOPE_USER.
  c.Set(kTestPolicyName1,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        new base::StringValue("chromium.org"),
        NULL);
  // |a| has precedence over |b|.
  c.Set(kTestPolicyName2,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        new base::FundamentalValue(true),
        NULL);
  c.Set(kTestPolicyName3,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        NULL, CreateExternalDataFetcher("a").release());
  // POLICY_SCOPE_MACHINE over POLICY_SCOPE_USER for POLICY_LEVEL_RECOMMENDED.
  c.Set(kTestPolicyName4,
        POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_MACHINE,
        new base::FundamentalValue(true),
        NULL);
  // POLICY_LEVEL_MANDATORY over POLICY_LEVEL_RECOMMENDED.
  c.Set(kTestPolicyName5,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        new base::StringValue(std::string()),
        NULL);
  // Merge new ones.
  c.Set(kTestPolicyName6,
        POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_USER,
        new base::FundamentalValue(true),
        NULL);

  EXPECT_TRUE(a.Equals(c));
}

TEST_F(PolicyMapTest, GetDifferingKeys) {
  PolicyMap a;
  a.Set(kTestPolicyName1,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_USER,
        new base::StringValue("google.com"),
        NULL);
  a.Set(kTestPolicyName2,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        NULL, CreateExternalDataFetcher("dummy").release());
  a.Set(kTestPolicyName3,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        new base::FundamentalValue(true),
        NULL);
  a.Set(kTestPolicyName4,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        NULL, CreateExternalDataFetcher("a").release());
  a.Set(kTestPolicyName5,
        POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_USER,
        new base::FundamentalValue(false),
        NULL);
  a.Set(kTestPolicyName6,
        POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_MACHINE,
        new base::StringValue("google.com/q={x}"),
        NULL);
  a.Set(kTestPolicyName7,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_USER,
        new base::FundamentalValue(true),
        NULL);

  PolicyMap b;
  b.Set(kTestPolicyName1,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_USER,
        new base::StringValue("google.com"),
        NULL);
  b.Set(kTestPolicyName2,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        NULL, CreateExternalDataFetcher("dummy").release());
  b.Set(kTestPolicyName3,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        new base::FundamentalValue(false),
        NULL);
  b.Set(kTestPolicyName4,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        NULL, CreateExternalDataFetcher("b").release());
  b.Set(kTestPolicyName5,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_USER,
        new base::FundamentalValue(false),
        NULL);
  b.Set(kTestPolicyName6,
        POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_USER,
        new base::StringValue("google.com/q={x}"),
        NULL);
  b.Set(kTestPolicyName8,
        POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_USER,
        new base::FundamentalValue(true),
        NULL);

  std::set<std::string> diff;
  std::set<std::string> diff2;
  a.GetDifferingKeys(b, &diff);
  b.GetDifferingKeys(a, &diff2);
  // Order shouldn't matter.
  EXPECT_EQ(diff, diff2);
  // No change.
  EXPECT_TRUE(diff.find(kTestPolicyName1) == diff.end());
  EXPECT_TRUE(diff.find(kTestPolicyName2) == diff.end());
  // Different values.
  EXPECT_TRUE(diff.find(kTestPolicyName3) != diff.end());
  // Different external data references.
  EXPECT_TRUE(diff.find(kTestPolicyName4) != diff.end());
  // Different levels.
  EXPECT_TRUE(diff.find(kTestPolicyName5) != diff.end());
  // Different scopes.
  EXPECT_TRUE(diff.find(kTestPolicyName6) != diff.end());
  // Not in |a|.
  EXPECT_TRUE(diff.find(kTestPolicyName8) != diff.end());
  // Not in |b|.
  EXPECT_TRUE(diff.find(kTestPolicyName7) != diff.end());
  // No surprises.
  EXPECT_EQ(6u, diff.size());
}

}  // namespace policy
