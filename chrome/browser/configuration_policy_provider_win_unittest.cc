// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <windows.h>

#include "base/logging.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/configuration_policy_provider_win.h"
#include "chrome/browser/mock_configuration_policy_store.h"
#include "chrome/common/pref_names.h"

namespace {
const wchar_t kUnitTestRegistrySubKey[] = L"SOFTWARE\\Chromium Unit Tests";
const wchar_t kUnitTestMachineOverrideSubKey[] =
    L"SOFTWARE\\Chromium Unit Tests\\HKLM Override";
const wchar_t kUnitTestUserOverrideSubKey[] =
    L"SOFTWARE\\Chromium Unit Tests\\HKCU Override";
}

// A subclass of |WinConfigurationPolicyProvider| providing access to
// internal protected constants without an orgy of FRIEND_TESTS.
class TestWinConfigurationPolicyProvider
    : public WinConfigurationPolicyProvider {
 public:
  TestWinConfigurationPolicyProvider() : WinConfigurationPolicyProvider() { }
  virtual ~TestWinConfigurationPolicyProvider() { }

  void SetHomepageRegistryValue(HKEY hive, const wchar_t* value);
  void SetHomepageRegistryValueWrongType(HKEY hive);
  void SetHomepageIsNewTabPage(HKEY hive, bool value);
  void SetCookiesMode(HKEY hive, uint32 value);
};

void TestWinConfigurationPolicyProvider::SetHomepageRegistryValue(
    HKEY hive,
    const wchar_t* value) {
  RegKey key(hive,
      WinConfigurationPolicyProvider::kPolicyRegistrySubKey,
      KEY_ALL_ACCESS);
  EXPECT_TRUE(key.WriteValue(
      WinConfigurationPolicyProvider::kHomepageRegistryValueName,
      value));
}

void TestWinConfigurationPolicyProvider::SetHomepageRegistryValueWrongType(
    HKEY hive) {
  RegKey key(hive,
      WinConfigurationPolicyProvider::kPolicyRegistrySubKey,
      KEY_ALL_ACCESS);
  EXPECT_TRUE(key.WriteValue(
      WinConfigurationPolicyProvider::kHomepageRegistryValueName,
      5));
}

void TestWinConfigurationPolicyProvider::SetHomepageIsNewTabPage(
    HKEY hive,
    bool value) {
  RegKey key(hive,
      WinConfigurationPolicyProvider::kPolicyRegistrySubKey,
      KEY_ALL_ACCESS);
  EXPECT_TRUE(key.WriteValue(
      WinConfigurationPolicyProvider::kHomepageIsNewTabPageRegistryValueName,
      value));
}

void TestWinConfigurationPolicyProvider::SetCookiesMode(
    HKEY hive,
    uint32 value) {
  RegKey key(hive,
      WinConfigurationPolicyProvider::kPolicyRegistrySubKey,
      KEY_ALL_ACCESS);
  EXPECT_TRUE(key.WriteValue(
      WinConfigurationPolicyProvider::kCookiesModeRegistryValueName,
      value));
}

// This test class provides sandboxing and mocking for the parts of the
// Windows Registry implementing Group Policy. The |SetUp| method prepares
// two temporary sandbox keys in |kUnitTestRegistrySubKey|, one for HKLM and one
// for HKCU. A test's calls to the registry are redirected by Windows to these
// sandboxes, allowing the tests to manipulate and access policy as if it
// were active, but without actually changing the parts of the Registry that
// are managed by Group Policy.
class WinConfigurationPolicyProviderTest : public testing::Test {
 public:
  WinConfigurationPolicyProviderTest();

  // testing::Test method overrides:
  virtual void SetUp();
  virtual void TearDown();

  void ActivateOverrides();
  void DeactivateOverrides();

  // Deletes the registry key created during the tests.
  void DeleteRegistrySandbox();

 private:
  // Keys are created for the lifetime of a test to contain
  // the sandboxed HKCU and HKLM hives, respectively.
  RegKey temp_hkcu_hive_key_;
  RegKey temp_hklm_hive_key_;
};

WinConfigurationPolicyProviderTest::WinConfigurationPolicyProviderTest()
    : temp_hklm_hive_key_(HKEY_CURRENT_USER, kUnitTestMachineOverrideSubKey),
      temp_hkcu_hive_key_(HKEY_CURRENT_USER, kUnitTestUserOverrideSubKey) {
}

void WinConfigurationPolicyProviderTest::SetUp() {
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

void WinConfigurationPolicyProviderTest::ActivateOverrides() {
  HRESULT result = RegOverridePredefKey(HKEY_LOCAL_MACHINE,
                                        temp_hklm_hive_key_.Handle());
  EXPECT_EQ(ERROR_SUCCESS, result);
  result = RegOverridePredefKey(HKEY_CURRENT_USER,
                                temp_hkcu_hive_key_.Handle());
  EXPECT_EQ(ERROR_SUCCESS, result);
}

void WinConfigurationPolicyProviderTest::DeactivateOverrides() {
  uint32 result = RegOverridePredefKey(HKEY_LOCAL_MACHINE, 0);
  EXPECT_EQ(ERROR_SUCCESS, result);
  result = RegOverridePredefKey(HKEY_CURRENT_USER, 0);
  EXPECT_EQ(ERROR_SUCCESS, result);
}

void WinConfigurationPolicyProviderTest::TearDown() {
  DeactivateOverrides();
  DeleteRegistrySandbox();
}

void WinConfigurationPolicyProviderTest::DeleteRegistrySandbox() {
  temp_hklm_hive_key_.Close();
  temp_hkcu_hive_key_.Close();
  RegKey key(HKEY_CURRENT_USER, kUnitTestRegistrySubKey, KEY_ALL_ACCESS);
  key.DeleteKey(L"");
}
TEST_F(WinConfigurationPolicyProviderTest, TestHomePagePolicyDefault) {
  MockConfigurationPolicyStore store;
  TestWinConfigurationPolicyProvider provider;

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyHomePage);
  EXPECT_TRUE(i == map.end());
}

TEST_F(WinConfigurationPolicyProviderTest, TestHomePagePolicyHKCU) {
  MockConfigurationPolicyStore store;
  TestWinConfigurationPolicyProvider provider;
  provider.SetHomepageRegistryValue(HKEY_CURRENT_USER,
                                    L"http://chromium.org");

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyHomePage);
  EXPECT_TRUE(i != map.end());
  string16 value;
  i->second->GetAsString(&value);
  EXPECT_EQ(L"http://chromium.org", value);
}

TEST_F(WinConfigurationPolicyProviderTest, TestHomePagePolicyHKCUWrongType) {
  MockConfigurationPolicyStore store;
  TestWinConfigurationPolicyProvider provider;
  provider.SetHomepageRegistryValueWrongType(HKEY_CURRENT_USER);

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyHomePage);
  EXPECT_TRUE(i == map.end());
}

TEST_F(WinConfigurationPolicyProviderTest, TestHomePagePolicyHKLM) {
  MockConfigurationPolicyStore store;
  TestWinConfigurationPolicyProvider provider;
  provider.SetHomepageRegistryValue(HKEY_LOCAL_MACHINE,
                                    L"http://chromium.org");

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyHomePage);
  EXPECT_TRUE(i != map.end());
  string16 value;
  i->second->GetAsString(&value);
  EXPECT_EQ(L"http://chromium.org", value);
}

TEST_F(WinConfigurationPolicyProviderTest, TestHomePagePolicyHKLMOverHKCU) {
  MockConfigurationPolicyStore store;
  TestWinConfigurationPolicyProvider provider;
  provider.SetHomepageRegistryValue(HKEY_CURRENT_USER,
                                    L"http://chromium.org");
  provider.SetHomepageRegistryValue(HKEY_LOCAL_MACHINE,
                                    L"http://crbug.com");

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyHomePage);
  EXPECT_TRUE(i != map.end());
  string16 value;
  i->second->GetAsString(&value);
  EXPECT_EQ(L"http://crbug.com", value);
}

TEST_F(WinConfigurationPolicyProviderTest,
    TestHomepageIsNewTabPagePolicyDefault) {
  MockConfigurationPolicyStore store;
  TestWinConfigurationPolicyProvider provider;

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyHomepageIsNewTabPage);
  EXPECT_TRUE(i == map.end());
}

TEST_F(WinConfigurationPolicyProviderTest,
    TestHomepageIsNewTabPagePolicyHKLM) {
  MockConfigurationPolicyStore store;
  TestWinConfigurationPolicyProvider provider;
  provider.SetHomepageIsNewTabPage(HKEY_LOCAL_MACHINE, true);

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyHomepageIsNewTabPage);
  EXPECT_TRUE(i != map.end());
  bool value = false;
  i->second->GetAsBoolean(&value);
  EXPECT_EQ(true, value);
}

TEST_F(WinConfigurationPolicyProviderTest,
    TestCookiesModePolicyDefault) {
  MockConfigurationPolicyStore store;
  TestWinConfigurationPolicyProvider provider;

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyCookiesMode);
  EXPECT_TRUE(i == map.end());
}

TEST_F(WinConfigurationPolicyProviderTest,
    TestCookiesModePolicyHKLM) {
  MockConfigurationPolicyStore store;
  TestWinConfigurationPolicyProvider provider;
  provider.SetCookiesMode(HKEY_LOCAL_MACHINE, 2);

  provider.Provide(&store);

  const MockConfigurationPolicyStore::PolicyMap& map(store.policy_map());
  MockConfigurationPolicyStore::PolicyMap::const_iterator i =
      map.find(ConfigurationPolicyStore::kPolicyCookiesMode);
  EXPECT_TRUE(i != map.end());
  int value = 0;
  i->second->GetAsInteger(&value);
  EXPECT_EQ(2, value);
}

