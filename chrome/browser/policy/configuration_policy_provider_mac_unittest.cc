// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/stl_util-inl.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider_mac.h"
#include "chrome/browser/policy/mock_configuration_policy_store.h"
#include "chrome/browser/preferences_mock_mac.h"

namespace policy {

// A subclass of |ConfigurationPolicyProviderMac| providing access to various
// internal things without an orgy of FRIEND_TESTS.
class TestConfigurationPolicyProviderMac
    : public ConfigurationPolicyProviderMac {
 public:
  TestConfigurationPolicyProviderMac() :
      ConfigurationPolicyProviderMac(
          ConfigurationPolicyPrefStore::GetChromePolicyValueMap(),
          new MockPreferences()) { }
  virtual ~TestConfigurationPolicyProviderMac() { }

  void AddTestItem(ConfigurationPolicyStore::PolicyType policy,
                   CFPropertyListRef value,
                   bool is_forced);
};

void TestConfigurationPolicyProviderMac::AddTestItem(
    ConfigurationPolicyStore::PolicyType policy,
    CFPropertyListRef value,
    bool is_forced) {
  const ConfigurationPolicyProvider::StaticPolicyValueMap& mapping =
      ConfigurationPolicyPrefStore::GetChromePolicyValueMap();
  for (size_t i = 0; i < mapping.entry_count; ++i) {
    if (mapping.entries[i].policy_type == policy) {
      scoped_cftyperef<CFStringRef> name(
          base::SysUTF8ToCFStringRef(mapping.entries[i].name));
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

TEST(ConfigurationPolicyProviderMacTest, TestExtensionInstallBlacklist) {
  scoped_cftyperef<CFMutableArrayRef> blacklist(
      CFArrayCreateMutable(kCFAllocatorDefault,
                           2,
                           &kCFTypeArrayCallBacks));
  CFArrayAppendValue(blacklist.get(), CFSTR("abc"));
  CFArrayAppendValue(blacklist.get(), CFSTR("def"));

  MockConfigurationPolicyStore store;
  TestConfigurationPolicyProviderMac provider;
  provider.AddTestItem
      (ConfigurationPolicyStore::kPolicyExtensionInstallDenyList,
       blacklist.get(),
       true);
  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyExtensionInstallDenyList);
  ASSERT_TRUE(i != map.end());
  ASSERT_TRUE(i->second->IsType(Value::TYPE_LIST));
  ListValue* value = reinterpret_cast<ListValue*>(i->second);
  std::string str_value;
  ASSERT_EQ(2U, value->GetSize());
  EXPECT_TRUE(value->GetString(0, &str_value));
  EXPECT_STREQ("abc", str_value.c_str());
  EXPECT_TRUE(value->GetString(1, &str_value));
  EXPECT_STREQ("def", str_value.c_str());
}

}  // namespace policy
