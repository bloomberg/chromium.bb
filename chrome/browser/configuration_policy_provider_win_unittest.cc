// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <windows.h>

#include "base/logging.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/configuration_policy_provider_win.h"
#include "chrome/browser/mock_configuration_policy_store.h"
#include "chrome/common/pref_names.h"

namespace {

const wchar_t kUnitTestRegistrySubKey[] = L"SOFTWARE\\Chromium Unit Tests";
const wchar_t kUnitTestMachineOverrideSubKey[] =
    L"SOFTWARE\\Chromium Unit Tests\\HKLM Override";
const wchar_t kUnitTestUserOverrideSubKey[] =
    L"SOFTWARE\\Chromium Unit Tests\\HKCU Override";

}  // namespace

// A subclass of |ConfigurationPolicyProviderWin| providing access to
// internal protected constants without an orgy of FRIEND_TESTS.
class TestConfigurationPolicyProviderWin
    : public ConfigurationPolicyProviderWin {
 public:
  TestConfigurationPolicyProviderWin() : ConfigurationPolicyProviderWin() { }
  virtual ~TestConfigurationPolicyProviderWin() { }

  void SetHomepageRegistryValue(HKEY hive, const wchar_t* value);
  void SetHomepageRegistryValueWrongType(HKEY hive);
  void SetBooleanPolicy(ConfigurationPolicyStore::PolicyType type,
                        HKEY hive, bool value);
  void SetCookiesMode(HKEY hive, uint32 value);

  typedef std::vector<PolicyValueMapEntry> PolicyValueMap;
  static const PolicyValueMap* PolicyValueMapping() {
    return ConfigurationPolicyProvider::PolicyValueMapping();
  }
};

namespace {

std::wstring NameForPolicy(ConfigurationPolicyStore::PolicyType policy) {
  const TestConfigurationPolicyProviderWin::PolicyValueMap* mapping =
      TestConfigurationPolicyProviderWin::PolicyValueMapping();
  for (TestConfigurationPolicyProviderWin::PolicyValueMap::const_iterator
       current = mapping->begin(); current != mapping->end(); ++current) {
    if (current->policy_type == policy)
      return UTF8ToWide(current->name);
  }
  return NULL;
}

}  // namespace

void TestConfigurationPolicyProviderWin::SetHomepageRegistryValue(
    HKEY hive,
    const wchar_t* value) {
  RegKey key(hive,
      ConfigurationPolicyProviderWin::kPolicyRegistrySubKey,
      KEY_ALL_ACCESS);
  EXPECT_TRUE(key.WriteValue(
      NameForPolicy(ConfigurationPolicyStore::kPolicyHomePage).c_str(),
      value));
}

void TestConfigurationPolicyProviderWin::SetHomepageRegistryValueWrongType(
    HKEY hive) {
  RegKey key(hive,
      ConfigurationPolicyProviderWin::kPolicyRegistrySubKey,
      KEY_ALL_ACCESS);
  EXPECT_TRUE(key.WriteValue(
      NameForPolicy(ConfigurationPolicyStore::kPolicyHomePage).c_str(),
      5));
}

void TestConfigurationPolicyProviderWin::SetBooleanPolicy(
    ConfigurationPolicyStore::PolicyType type,
    HKEY hive,
    bool value) {
  RegKey key(hive, ConfigurationPolicyProviderWin::kPolicyRegistrySubKey,
      KEY_ALL_ACCESS);
  EXPECT_TRUE(key.WriteValue(NameForPolicy(type).c_str(), value));
}

// This test class provides sandboxing and mocking for the parts of the
// Windows Registry implementing Group Policy. The |SetUp| method prepares
// two temporary sandbox keys in |kUnitTestRegistrySubKey|, one for HKLM and one
// for HKCU. A test's calls to the registry are redirected by Windows to these
// sandboxes, allowing the tests to manipulate and access policy as if it
// were active, but without actually changing the parts of the Registry that
// are managed by Group Policy.
class ConfigurationPolicyProviderWinTest : public testing::Test {
 public:
  ConfigurationPolicyProviderWinTest();

  // testing::Test method overrides:
  virtual void SetUp();
  virtual void TearDown();

  void ActivateOverrides();
  void DeactivateOverrides();

  // Deletes the registry key created during the tests.
  void DeleteRegistrySandbox();

  void TestBooleanPolicyDefault(ConfigurationPolicyStore::PolicyType type);
  void TestBooleanPolicyHKLM(ConfigurationPolicyStore::PolicyType type);
  void TestBooleanPolicy(ConfigurationPolicyStore::PolicyType type);

 private:
  // Keys are created for the lifetime of a test to contain
  // the sandboxed HKCU and HKLM hives, respectively.
  RegKey temp_hkcu_hive_key_;
  RegKey temp_hklm_hive_key_;
};

ConfigurationPolicyProviderWinTest::ConfigurationPolicyProviderWinTest()
    : temp_hklm_hive_key_(HKEY_CURRENT_USER, kUnitTestMachineOverrideSubKey),
      temp_hkcu_hive_key_(HKEY_CURRENT_USER, kUnitTestUserOverrideSubKey) {
}

void ConfigurationPolicyProviderWinTest::SetUp() {
  // Cleanup any remnants of previous tests.
  DeleteRegistrySandbox();

  // Create the subkeys to hold the overridden HKLM and HKCU
  // policy settings.
  temp_hklm_hive_key_.Create(HKEY_CURRENT_USER,
                             kUnitTestMachineOverrideSubKey,
                             KEY_ALL_ACCESS);
  temp_hkcu_hive_key_.Create(HKEY_CURRENT_USER,
                             kUnitTestUserOverrideSubKey,
                             KEY_ALL_ACCESS);

  ActivateOverrides();
}

void ConfigurationPolicyProviderWinTest::ActivateOverrides() {
  HRESULT result = RegOverridePredefKey(HKEY_LOCAL_MACHINE,
                                        temp_hklm_hive_key_.Handle());
  EXPECT_EQ(ERROR_SUCCESS, result);
  result = RegOverridePredefKey(HKEY_CURRENT_USER,
                                temp_hkcu_hive_key_.Handle());
  EXPECT_EQ(ERROR_SUCCESS, result);
}

void ConfigurationPolicyProviderWinTest::DeactivateOverrides() {
  uint32 result = RegOverridePredefKey(HKEY_LOCAL_MACHINE, 0);
  EXPECT_EQ(ERROR_SUCCESS, result);
  result = RegOverridePredefKey(HKEY_CURRENT_USER, 0);
  EXPECT_EQ(ERROR_SUCCESS, result);
}

void ConfigurationPolicyProviderWinTest::TearDown() {
  DeactivateOverrides();
  DeleteRegistrySandbox();
}

void ConfigurationPolicyProviderWinTest::DeleteRegistrySandbox() {
  temp_hklm_hive_key_.Close();
  temp_hkcu_hive_key_.Close();
  RegKey key(HKEY_CURRENT_USER, kUnitTestRegistrySubKey, KEY_ALL_ACCESS);
  key.DeleteKey(L"");
}

void ConfigurationPolicyProviderWinTest::TestBooleanPolicyDefault(
    ConfigurationPolicyStore::PolicyType type) {
  MockConfigurationPolicyStore store;
  TestConfigurationPolicyProviderWin provider;
  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  EXPECT_FALSE(ContainsKey(map, type));
}

void ConfigurationPolicyProviderWinTest::TestBooleanPolicyHKLM(
    ConfigurationPolicyStore::PolicyType type) {
  MockConfigurationPolicyStore store;
  TestConfigurationPolicyProviderWin provider;
  provider.SetBooleanPolicy(type, HKEY_LOCAL_MACHINE, true);
  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i = map.find(type);
  ASSERT_TRUE(i != map.end());
  bool value = false;
  i->second->GetAsBoolean(&value);
  EXPECT_EQ(true, value);
}

void ConfigurationPolicyProviderWinTest::TestBooleanPolicy(
    ConfigurationPolicyStore::PolicyType type) {
  TestBooleanPolicyDefault(type);
  TestBooleanPolicyHKLM(type);
}

TEST_F(ConfigurationPolicyProviderWinTest, TestHomePagePolicyDefault) {
  MockConfigurationPolicyStore store;
  TestConfigurationPolicyProviderWin provider;

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  EXPECT_FALSE(ContainsKey(map, ConfigurationPolicyStore::kPolicyHomePage));
}

TEST_F(ConfigurationPolicyProviderWinTest, TestHomePagePolicyHKCU) {
  MockConfigurationPolicyStore store;
  TestConfigurationPolicyProviderWin provider;
  provider.SetHomepageRegistryValue(HKEY_CURRENT_USER,
                                    L"http://chromium.org");

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyHomePage);
  ASSERT_TRUE(i != map.end());
  string16 value;
  i->second->GetAsString(&value);
  EXPECT_EQ(L"http://chromium.org", value);
}

TEST_F(ConfigurationPolicyProviderWinTest, TestHomePagePolicyHKCUWrongType) {
  MockConfigurationPolicyStore store;
  TestConfigurationPolicyProviderWin provider;
  provider.SetHomepageRegistryValueWrongType(HKEY_CURRENT_USER);

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  EXPECT_FALSE(ContainsKey(map, ConfigurationPolicyStore::kPolicyHomePage));
}

TEST_F(ConfigurationPolicyProviderWinTest, TestHomePagePolicyHKLM) {
  MockConfigurationPolicyStore store;
  TestConfigurationPolicyProviderWin provider;
  provider.SetHomepageRegistryValue(HKEY_LOCAL_MACHINE,
                                    L"http://chromium.org");

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyHomePage);
  ASSERT_TRUE(i != map.end());
  string16 value;
  i->second->GetAsString(&value);
  EXPECT_EQ(L"http://chromium.org", value);
}

TEST_F(ConfigurationPolicyProviderWinTest, TestHomePagePolicyHKLMOverHKCU) {
  MockConfigurationPolicyStore store;
  TestConfigurationPolicyProviderWin provider;
  provider.SetHomepageRegistryValue(HKEY_CURRENT_USER,
                                    L"http://chromium.org");
  provider.SetHomepageRegistryValue(HKEY_LOCAL_MACHINE,
                                    L"http://crbug.com");

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyHomePage);
  ASSERT_TRUE(i != map.end());
  string16 value;
  i->second->GetAsString(&value);
  EXPECT_EQ(L"http://crbug.com", value);
}

TEST_F(ConfigurationPolicyProviderWinTest,
    TestHomepageIsNewTabPagePolicy) {
  TestBooleanPolicy(ConfigurationPolicyStore::kPolicyHomepageIsNewTabPage);
}

TEST_F(ConfigurationPolicyProviderWinTest,
    TestPolicyAlternateErrorPagesEnabled) {
  TestBooleanPolicy(
      ConfigurationPolicyStore::kPolicyAlternateErrorPagesEnabled);
}

TEST_F(ConfigurationPolicyProviderWinTest,
    TestPolicySearchSuggestEnabled) {
  TestBooleanPolicy(ConfigurationPolicyStore::kPolicySearchSuggestEnabled);
}

TEST_F(ConfigurationPolicyProviderWinTest,
    TestPolicyDnsPrefetchingEnabled) {
  TestBooleanPolicy(ConfigurationPolicyStore::kPolicyDnsPrefetchingEnabled);
}

TEST_F(ConfigurationPolicyProviderWinTest,
    TestPolicySafeBrowsingEnabled) {
  TestBooleanPolicy(ConfigurationPolicyStore::kPolicySafeBrowsingEnabled);
}

TEST_F(ConfigurationPolicyProviderWinTest,
    TestPolicyMetricsReportingEnabled) {
  TestBooleanPolicy(ConfigurationPolicyStore::kPolicyMetricsReportingEnabled);
}

TEST_F(ConfigurationPolicyProviderWinTest,
    TestPolicyPasswordManagerEnabled) {
  TestBooleanPolicy(ConfigurationPolicyStore::kPolicyPasswordManagerEnabled);
}

