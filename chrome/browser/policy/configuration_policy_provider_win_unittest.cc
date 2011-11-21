// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <windows.h>

#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/policy/asynchronous_policy_test_base.h"
#include "chrome/browser/policy/configuration_policy_provider_test.h"
#include "chrome/browser/policy/configuration_policy_provider_win.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

namespace policy {

namespace {

const wchar_t kUnitTestRegistrySubKey[] = L"SOFTWARE\\Chromium Unit Tests";
const wchar_t kUnitTestMachineOverrideSubKey[] =
    L"SOFTWARE\\Chromium Unit Tests\\HKLM Override";
const wchar_t kUnitTestUserOverrideSubKey[] =
    L"SOFTWARE\\Chromium Unit Tests\\HKCU Override";

// This class provides sandboxing and mocking for the parts of the Windows
// Registry implementing Group Policy. It prepares two temporary sandbox keys
// in |kUnitTestRegistrySubKey|, one for HKLM and one for HKCU. A test's calls
// to the registry are redirected by Windows to these sandboxes, allowing the
// tests to manipulate and access policy as if it were active, but without
// actually changing the parts of the Registry that are managed by Group
// Policy.
class ScopedGroupPolicyRegistrySandbox {
 public:
  ScopedGroupPolicyRegistrySandbox();
  ~ScopedGroupPolicyRegistrySandbox();

 private:
  void ActivateOverrides();
  void RemoveOverrides();

  // Deletes the sandbox keys.
  void DeleteKeys();

  // Keys are created for the lifetime of a test to contain
  // the sandboxed HKCU and HKLM hives, respectively.
  RegKey temp_hkcu_hive_key_;
  RegKey temp_hklm_hive_key_;

  DISALLOW_COPY_AND_ASSIGN(ScopedGroupPolicyRegistrySandbox);
};

class TestHarness : public PolicyProviderTestHarness {
 public:
  explicit TestHarness(HKEY hive);
  virtual ~TestHarness();

  virtual void SetUp() OVERRIDE;

  virtual AsynchronousPolicyProvider* CreateProvider(
      const PolicyDefinitionList* policy_definition_list) OVERRIDE;

  virtual void InstallEmptyPolicy() OVERRIDE;
  virtual void InstallStringPolicy(const std::string& policy_name,
                                   const std::string& policy_value) OVERRIDE;
  virtual void InstallIntegerPolicy(const std::string& policy_name,
                                    int policy_value) OVERRIDE;
  virtual void InstallBooleanPolicy(const std::string& policy_name,
                                    bool policy_value) OVERRIDE;
  virtual void InstallStringListPolicy(const std::string& policy_name,
                                       const ListValue* policy_value) OVERRIDE;

  // Creates a harness instance that will install policy in HKCU or HKLM,
  // respectively.
  static PolicyProviderTestHarness* CreateHKCU();
  static PolicyProviderTestHarness* CreateHKLM();

 private:
  HKEY hive_;

  ScopedGroupPolicyRegistrySandbox registry_sandbox_;

  DISALLOW_COPY_AND_ASSIGN(TestHarness);
};

ScopedGroupPolicyRegistrySandbox::ScopedGroupPolicyRegistrySandbox() {
  // Cleanup any remnants of previous tests.
  DeleteKeys();

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

ScopedGroupPolicyRegistrySandbox::~ScopedGroupPolicyRegistrySandbox() {
  RemoveOverrides();
  DeleteKeys();
}

void ScopedGroupPolicyRegistrySandbox::ActivateOverrides() {
  ASSERT_HRESULT_SUCCEEDED(RegOverridePredefKey(HKEY_LOCAL_MACHINE,
                                                temp_hklm_hive_key_.Handle()));
  ASSERT_HRESULT_SUCCEEDED(RegOverridePredefKey(HKEY_CURRENT_USER,
                                                temp_hkcu_hive_key_.Handle()));
}

void ScopedGroupPolicyRegistrySandbox::RemoveOverrides() {
  ASSERT_HRESULT_SUCCEEDED(RegOverridePredefKey(HKEY_LOCAL_MACHINE, 0));
  ASSERT_HRESULT_SUCCEEDED(RegOverridePredefKey(HKEY_CURRENT_USER, 0));
}

void ScopedGroupPolicyRegistrySandbox::DeleteKeys() {
  RegKey key(HKEY_CURRENT_USER, kUnitTestRegistrySubKey, KEY_ALL_ACCESS);
  key.DeleteKey(L"");
}

TestHarness::TestHarness(HKEY hive)
    : hive_(hive) {}

TestHarness::~TestHarness() {}

void TestHarness::SetUp() {}

AsynchronousPolicyProvider* TestHarness::CreateProvider(
    const PolicyDefinitionList* policy_definition_list) {
  return new ConfigurationPolicyProviderWin(policy_definition_list);
}

void TestHarness::InstallEmptyPolicy() {}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  RegKey key(hive_, policy::kRegistrySubKey, KEY_ALL_ACCESS);
  key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                 UTF8ToUTF16(policy_value).c_str());
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  RegKey key(hive_, policy::kRegistrySubKey, KEY_ALL_ACCESS);
  key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                 static_cast<DWORD>(policy_value));
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  RegKey key(hive_, policy::kRegistrySubKey, KEY_ALL_ACCESS);
  key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                 static_cast<DWORD>(policy_value));
}

void TestHarness::InstallStringListPolicy(const std::string& policy_name,
                                          const ListValue* policy_value) {
  RegKey key(hive_,
             (string16(policy::kRegistrySubKey) + ASCIIToUTF16("\\") +
              UTF8ToUTF16(policy_name)).c_str(),
             KEY_ALL_ACCESS);
  int index = 1;
  for (ListValue::const_iterator element(policy_value->begin());
       element != policy_value->end();
       ++element) {
    std::string element_value;
    if (!(*element)->GetAsString(&element_value))
      continue;
    std::string name(base::IntToString(index++));
    key.WriteValue(UTF8ToUTF16(name).c_str(),
                   UTF8ToUTF16(element_value).c_str());
  }
}

// static
PolicyProviderTestHarness* TestHarness::CreateHKCU() {
  return new TestHarness(HKEY_CURRENT_USER);
}

// static
PolicyProviderTestHarness* TestHarness::CreateHKLM() {
  return new TestHarness(HKEY_LOCAL_MACHINE);
}

}  // namespace

// Instantiate abstract test case for basic policy reading tests.
INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyProviderWinTest,
    ConfigurationPolicyProviderTest,
    testing::Values(TestHarness::CreateHKCU, TestHarness::CreateHKLM));

// Test cases for windows policy provider specific functionality.
class ConfigurationPolicyProviderWinTest : public AsynchronousPolicyTestBase {
 protected:
  ConfigurationPolicyProviderWinTest()
      : provider_(&test_policy_definitions::kList) {}
  virtual ~ConfigurationPolicyProviderWinTest() {}

  ScopedGroupPolicyRegistrySandbox registry_sandbox_;
  ConfigurationPolicyProviderWin provider_;
};

TEST_F(ConfigurationPolicyProviderWinTest, HKLMOverHKCU) {
  RegKey hklm_key(HKEY_LOCAL_MACHINE, policy::kRegistrySubKey, KEY_ALL_ACCESS);
  hklm_key.WriteValue(UTF8ToUTF16(test_policy_definitions::kKeyString).c_str(),
                      UTF8ToUTF16("hklm").c_str());
  RegKey hkcu_key(HKEY_CURRENT_USER, policy::kRegistrySubKey, KEY_ALL_ACCESS);
  hkcu_key.WriteValue(UTF8ToUTF16(test_policy_definitions::kKeyString).c_str(),
                      UTF8ToUTF16("hkcu").c_str());

  provider_.RefreshPolicies();
  loop_.RunAllPending();

  PolicyMap policy_map;
  provider_.Provide(&policy_map);
  const Value* value = policy_map.Get(test_policy_definitions::kPolicyString);
  EXPECT_TRUE(StringValue("hklm").Equals(value));
}

}  // namespace policy
