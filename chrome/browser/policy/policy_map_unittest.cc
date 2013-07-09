// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_map.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/external_data_manager.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Utility functions for the tests.
void SetPolicy(PolicyMap* map, const char* name, Value* value) {
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
  SetPolicy(&map, key::kHomepageLocation, Value::CreateStringValue("aaa"));
  StringValue expected("aaa");
  EXPECT_TRUE(expected.Equals(map.GetValue(key::kHomepageLocation)));
  SetPolicy(&map, key::kHomepageLocation, Value::CreateStringValue("bbb"));
  StringValue expected_b("bbb");
  EXPECT_TRUE(expected_b.Equals(map.GetValue(key::kHomepageLocation)));
  SetPolicy(&map, key::kHomepageLocation,
            CreateExternalDataFetcher("dummy").release());
  EXPECT_FALSE(map.GetValue(key::kHomepageLocation));
  const PolicyMap::Entry* entry = map.Get(key::kHomepageLocation);
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(POLICY_LEVEL_MANDATORY, entry->level);
  EXPECT_EQ(POLICY_SCOPE_USER, entry->scope);
  EXPECT_TRUE(ExternalDataFetcher::Equals(
      entry->external_data_fetcher, CreateExternalDataFetcher("dummy").get()));
  map.Set(key::kHomepageLocation, POLICY_LEVEL_RECOMMENDED,
          POLICY_SCOPE_MACHINE, NULL, NULL);
  EXPECT_FALSE(map.GetValue(key::kHomepageLocation));
  entry = map.Get(key::kHomepageLocation);
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(POLICY_LEVEL_RECOMMENDED, entry->level);
  EXPECT_EQ(POLICY_SCOPE_MACHINE, entry->scope);
  EXPECT_FALSE(entry->external_data_fetcher);
}

TEST_F(PolicyMapTest, Equals) {
  PolicyMap a;
  SetPolicy(&a, key::kHomepageLocation, Value::CreateStringValue("aaa"));
  PolicyMap a2;
  SetPolicy(&a2, key::kHomepageLocation, Value::CreateStringValue("aaa"));
  PolicyMap b;
  SetPolicy(&b, key::kHomepageLocation, Value::CreateStringValue("bbb"));
  PolicyMap c;
  SetPolicy(&c, key::kHomepageLocation, Value::CreateStringValue("aaa"));
  SetPolicy(&c, key::kHomepageIsNewTabPage, Value::CreateBooleanValue(true));
  PolicyMap d;
  SetPolicy(&d, key::kHomepageLocation,
            CreateExternalDataFetcher("ddd").release());
  PolicyMap d2;
  SetPolicy(&d2, key::kHomepageLocation,
            CreateExternalDataFetcher("ddd").release());
  PolicyMap e;
  SetPolicy(&e, key::kHomepageLocation,
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
  SetPolicy(&a, key::kHomepageLocation, Value::CreateStringValue("aaa"));
  SetPolicy(&a, key::kAlternateErrorPagesEnabled,
            CreateExternalDataFetcher("dummy").release());
  PolicyMap b;
  SetPolicy(&b, key::kHomepageLocation, Value::CreateStringValue("bbb"));
  SetPolicy(&b, key::kHomepageIsNewTabPage, Value::CreateBooleanValue(true));

  a.Swap(&b);
  base::StringValue expected("bbb");
  EXPECT_TRUE(expected.Equals(a.GetValue(key::kHomepageLocation)));
  base::FundamentalValue expected_bool(true);
  EXPECT_TRUE(expected_bool.Equals(a.GetValue(key::kHomepageIsNewTabPage)));
  EXPECT_FALSE(a.GetValue(key::kAlternateErrorPagesEnabled));
  EXPECT_FALSE(a.Get(key::kAlternateErrorPagesEnabled));
  StringValue expected_a("aaa");
  EXPECT_TRUE(expected_a.Equals(b.GetValue(key::kHomepageLocation)));
  EXPECT_FALSE(b.GetValue(key::kHomepageIsNewTabPage));
  EXPECT_FALSE(a.GetValue(key::kAlternateErrorPagesEnabled));
  const PolicyMap::Entry* entry = b.Get(key::kAlternateErrorPagesEnabled);
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
  a.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        Value::CreateStringValue("google.com"), NULL);
  a.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        Value::CreateBooleanValue(true), NULL);
  a.Set(key::kAlternateErrorPagesEnabled,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        NULL, CreateExternalDataFetcher("a").release());
  a.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
        Value::CreateBooleanValue(false), NULL);
  a.Set(key::kDefaultSearchProviderSearchURL,
        POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
        Value::CreateStringValue("google.com/q={x}"), NULL);

  PolicyMap b;
  b.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        Value::CreateStringValue("chromium.org"), NULL);
  b.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        Value::CreateBooleanValue(false), NULL);
  b.Set(key::kAlternateErrorPagesEnabled,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        NULL, CreateExternalDataFetcher("b").release());
  b.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_MACHINE, Value::CreateBooleanValue(true), NULL);
  b.Set(key::kDefaultSearchProviderSearchURL,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        Value::CreateStringValue(std::string()),
        NULL);
  b.Set(key::kDisableSpdy,
        POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_USER,
        Value::CreateBooleanValue(true),
        NULL);

  a.MergeFrom(b);

  PolicyMap c;
  // POLICY_SCOPE_MACHINE over POLICY_SCOPE_USER.
  c.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        Value::CreateStringValue("chromium.org"), NULL);
  // |a| has precedence over |b|.
  c.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        Value::CreateBooleanValue(true), NULL);
  c.Set(key::kAlternateErrorPagesEnabled,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        NULL, CreateExternalDataFetcher("a").release());
  // POLICY_SCOPE_MACHINE over POLICY_SCOPE_USER for POLICY_LEVEL_RECOMMENDED.
  c.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_MACHINE, Value::CreateBooleanValue(true), NULL);
  // POLICY_LEVEL_MANDATORY over POLICY_LEVEL_RECOMMENDED.
  c.Set(key::kDefaultSearchProviderSearchURL,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE,
        Value::CreateStringValue(std::string()),
        NULL);
  // Merge new ones.
  c.Set(key::kDisableSpdy, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
        Value::CreateBooleanValue(true), NULL);

  EXPECT_TRUE(a.Equals(c));
}

TEST_F(PolicyMapTest, GetDifferingKeys) {
  PolicyMap a;
  a.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        Value::CreateStringValue("google.com"), NULL);
  a.Set(key::kSearchSuggestEnabled,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        NULL, CreateExternalDataFetcher("dummy").release());
  a.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        Value::CreateBooleanValue(true), NULL);
  a.Set(key::kAlternateErrorPagesEnabled,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        NULL, CreateExternalDataFetcher("a").release());
  a.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
        Value::CreateBooleanValue(false), NULL);
  a.Set(key::kDefaultSearchProviderSearchURL,
        POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
        Value::CreateStringValue("google.com/q={x}"), NULL);
  a.Set(key::kDisable3DAPIs, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        Value::CreateBooleanValue(true), NULL);

  PolicyMap b;
  b.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        Value::CreateStringValue("google.com"), NULL);
  b.Set(key::kSearchSuggestEnabled,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        NULL, CreateExternalDataFetcher("dummy").release());
  b.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        Value::CreateBooleanValue(false), NULL);
  b.Set(key::kAlternateErrorPagesEnabled,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        NULL, CreateExternalDataFetcher("b").release());
  b.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        Value::CreateBooleanValue(false), NULL);
  b.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_RECOMMENDED,
        POLICY_SCOPE_USER, Value::CreateStringValue("google.com/q={x}"), NULL);
  b.Set(key::kDisableSpdy, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
        Value::CreateBooleanValue(true), NULL);

  std::set<std::string> diff;
  std::set<std::string> diff2;
  a.GetDifferingKeys(b, &diff);
  b.GetDifferingKeys(a, &diff2);
  // Order shouldn't matter.
  EXPECT_EQ(diff, diff2);
  // No change.
  EXPECT_TRUE(diff.find(key::kHomepageLocation) == diff.end());
  EXPECT_TRUE(diff.find(key::kSearchSuggestEnabled) == diff.end());
  // Different values.
  EXPECT_TRUE(diff.find(key::kShowHomeButton) != diff.end());
  // Different external data references.
  EXPECT_TRUE(diff.find(key::kAlternateErrorPagesEnabled) != diff.end());
  // Different levels.
  EXPECT_TRUE(diff.find(key::kBookmarkBarEnabled) != diff.end());
  // Different scopes.
  EXPECT_TRUE(diff.find(key::kDefaultSearchProviderSearchURL) != diff.end());
  // Not in |a|.
  EXPECT_TRUE(diff.find(key::kDisableSpdy) != diff.end());
  // Not in |b|.
  EXPECT_TRUE(diff.find(key::kDisable3DAPIs) != diff.end());
  // No surprises.
  EXPECT_EQ(6u, diff.size());
}

}  // namespace policy
