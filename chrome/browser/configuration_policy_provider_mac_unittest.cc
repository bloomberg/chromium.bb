// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/stl_util-inl.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/configuration_policy_provider_mac.h"
#include "chrome/browser/mock_configuration_policy_store.h"
#include "chrome/browser/preferences_mock_mac.h"

// A subclass of |ConfigurationPolicyProviderMac| providing access to various
// internal things without an orgy of FRIEND_TESTS.
class TestConfigurationPolicyProviderMac
    : public ConfigurationPolicyProviderMac {
 public:
  TestConfigurationPolicyProviderMac() :
      ConfigurationPolicyProviderMac(new MockPreferences()) { }
  virtual ~TestConfigurationPolicyProviderMac() { }

  void AddTestItem(ConfigurationPolicyStore::PolicyType policy,
                   CFPropertyListRef value,
                   bool is_forced);

  typedef std::vector<PolicyValueMapEntry> PolicyValueMap;
  static const PolicyValueMap* PolicyValueMapping() {
    return ConfigurationPolicyProvider::PolicyValueMapping();
  }
};

void TestConfigurationPolicyProviderMac::AddTestItem(
    ConfigurationPolicyStore::PolicyType policy,
    CFPropertyListRef value,
    bool is_forced) {
  const PolicyValueMap* mapping = PolicyValueMapping();
  for (PolicyValueMap::const_iterator current = mapping->begin();
       current != mapping->end(); ++current) {
    if (current->policy_type == policy) {
      scoped_cftyperef<CFStringRef> name(
          base::SysUTF8ToCFStringRef(current->name));
      static_cast<MockPreferences*>(preferences_.get())->
          AddTestItem(name, value, is_forced);
    }
  }
}

TEST(ConfigurationPolicyProviderMacTest, TestPolicyDefault) {
  MockConfigurationPolicyStore store;
  TestConfigurationPolicyProviderMac provider;

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  EXPECT_FALSE(ContainsKey(map, ConfigurationPolicyStore::kPolicyHomePage));
}

TEST(ConfigurationPolicyProviderMacTest, TestHomePagePolicy) {
  MockConfigurationPolicyStore store;
  TestConfigurationPolicyProviderMac provider;
  provider.AddTestItem(ConfigurationPolicyStore::kPolicyHomePage,
                       CFSTR("http://chromium.org"),
                       true);

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyHomePage);
  ASSERT_TRUE(i != map.end());
  std::string value;
  i->second->GetAsString(&value);
  EXPECT_EQ("http://chromium.org", value);
}

TEST(ConfigurationPolicyProviderMacTest, TestHomePagePolicyUnforced) {
  MockConfigurationPolicyStore store;
  TestConfigurationPolicyProviderMac provider;
  provider.AddTestItem(ConfigurationPolicyStore::kPolicyHomePage,
                       CFSTR("http://chromium.org"),
                       false);

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  EXPECT_FALSE(ContainsKey(map, ConfigurationPolicyStore::kPolicyHomePage));
}

TEST(ConfigurationPolicyProviderMacTest, TestHomePagePolicyWrongType) {
  MockConfigurationPolicyStore store;
  TestConfigurationPolicyProviderMac provider;
  provider.AddTestItem(ConfigurationPolicyStore::kPolicyHomePage,
                       kCFBooleanTrue,
                       true);

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  EXPECT_FALSE(ContainsKey(map, ConfigurationPolicyStore::kPolicyHomePage));
}

TEST(ConfigurationPolicyProviderMacTest, TestHomepageIsNewTabPagePolicy) {
  MockConfigurationPolicyStore store;
  TestConfigurationPolicyProviderMac provider;
  provider.AddTestItem(ConfigurationPolicyStore::kPolicyHomepageIsNewTabPage,
                       kCFBooleanTrue,
                       true);

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyHomepageIsNewTabPage);
  ASSERT_TRUE(i != map.end());
  bool value = false;
  i->second->GetAsBoolean(&value);
  EXPECT_EQ(true, value);
}

