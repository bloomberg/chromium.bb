// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_loader_win.h"

#include <windows.h>

#include "base/json/json_writer.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/policy/async_policy_provider.h"
#include "chrome/browser/policy/configuration_policy_provider_test.h"
#include "chrome/browser/policy/policy_bundle.h"
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

// Installs |dict| at the given |path|, in the given |hive|. Currently only
// string, int and dictionary types are converted; other types cause a failure.
// Returns false if there was any failure, and true if |dict| was successfully
// written.
// TODO(joaodasilva): generate a schema for |dict| too, so that all types can
// be retrieved.
bool InstallDictionary(const base::DictionaryValue& dict,
                       HKEY hive,
                       const string16& path) {
  // KEY_ALL_ACCESS causes the ctor to create the key if it does not exist yet.
  RegKey key(hive, path.c_str(), KEY_ALL_ACCESS);
  const string16 kPathSep = ASCIIToUTF16("\\");

  for (base::DictionaryValue::Iterator it(dict); it.HasNext(); it.Advance()) {
    string16 name(UTF8ToUTF16(it.key()));
    switch (it.value().GetType()) {
      case base::Value::TYPE_STRING: {
        string16 value;
        if (!it.value().GetAsString(&value))
          return false;
        if (key.WriteValue(name.c_str(), value.c_str()) != ERROR_SUCCESS)
          return false;
        break;
      }

      case base::Value::TYPE_INTEGER: {
        int value;
        if (!it.value().GetAsInteger(&value))
          return false;
        if (key.WriteValue(name.c_str(), value) != ERROR_SUCCESS)
          return false;
        break;
      }

      case base::Value::TYPE_DICTIONARY: {
        const base::DictionaryValue* sub_dict = NULL;
        if (!it.value().GetAsDictionary(&sub_dict))
          return false;
        if (!InstallDictionary(*sub_dict, hive, path + kPathSep + name))
          return false;
        break;
      }

      default:
        return false;
    }
  }
  return true;
}

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
  explicit TestHarness(HKEY hive, PolicyScope scope);
  virtual ~TestHarness();

  virtual void SetUp() OVERRIDE;

  virtual ConfigurationPolicyProvider* CreateProvider(
      const PolicyDefinitionList* policy_definition_list) OVERRIDE;

  virtual void InstallEmptyPolicy() OVERRIDE;
  virtual void InstallStringPolicy(const std::string& policy_name,
                                   const std::string& policy_value) OVERRIDE;
  virtual void InstallIntegerPolicy(const std::string& policy_name,
                                    int policy_value) OVERRIDE;
  virtual void InstallBooleanPolicy(const std::string& policy_name,
                                    bool policy_value) OVERRIDE;
  virtual void InstallStringListPolicy(
      const std::string& policy_name,
      const base::ListValue* policy_value) OVERRIDE;
  virtual void InstallDictionaryPolicy(
      const std::string& policy_name,
      const base::DictionaryValue* policy_value) OVERRIDE;

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

TestHarness::TestHarness(HKEY hive, PolicyScope scope)
    : PolicyProviderTestHarness(POLICY_LEVEL_MANDATORY, scope), hive_(hive) {}

TestHarness::~TestHarness() {}

void TestHarness::SetUp() {}

ConfigurationPolicyProvider* TestHarness::CreateProvider(
    const PolicyDefinitionList* policy_list) {
  scoped_ptr<AsyncPolicyLoader> loader(new PolicyLoaderWin(policy_list));
  return new AsyncPolicyProvider(loader.Pass());
}

void TestHarness::InstallEmptyPolicy() {}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  RegKey key(hive_, kRegistryMandatorySubKey, KEY_ALL_ACCESS);
  key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                 UTF8ToUTF16(policy_value).c_str());
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  RegKey key(hive_, kRegistryMandatorySubKey, KEY_ALL_ACCESS);
  key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                 static_cast<DWORD>(policy_value));
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  RegKey key(hive_, kRegistryMandatorySubKey, KEY_ALL_ACCESS);
  key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                 static_cast<DWORD>(policy_value));
}

void TestHarness::InstallStringListPolicy(const std::string& policy_name,
                                          const base::ListValue* policy_value) {
  RegKey key(hive_,
             (string16(kRegistryMandatorySubKey) + ASCIIToUTF16("\\") +
              UTF8ToUTF16(policy_name)).c_str(),
             KEY_ALL_ACCESS);
  int index = 1;
  for (base::ListValue::const_iterator element(policy_value->begin());
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

void TestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::DictionaryValue* policy_value) {
  std::string json;
  base::JSONWriter::Write(policy_value, &json);
  RegKey key(hive_, kRegistryMandatorySubKey, KEY_ALL_ACCESS);
  key.WriteValue(UTF8ToUTF16(policy_name).c_str(),
                 UTF8ToUTF16(json).c_str());
}

// static
PolicyProviderTestHarness* TestHarness::CreateHKCU() {
  return new TestHarness(HKEY_CURRENT_USER, POLICY_SCOPE_USER);
}

// static
PolicyProviderTestHarness* TestHarness::CreateHKLM() {
  return new TestHarness(HKEY_LOCAL_MACHINE, POLICY_SCOPE_MACHINE);
}

}  // namespace

// Instantiate abstract test case for basic policy reading tests.
INSTANTIATE_TEST_CASE_P(
    PolicyProviderWinTest,
    ConfigurationPolicyProviderTest,
    testing::Values(TestHarness::CreateHKCU, TestHarness::CreateHKLM));

// Test cases for windows policy provider specific functionality.
class PolicyLoaderWinTest : public PolicyTestBase {
 protected:
  PolicyLoaderWinTest() {}
  virtual ~PolicyLoaderWinTest() {}

  ScopedGroupPolicyRegistrySandbox registry_sandbox_;
};

TEST_F(PolicyLoaderWinTest, HKLMOverHKCU) {
  RegKey hklm_key(HKEY_LOCAL_MACHINE, kRegistryMandatorySubKey, KEY_ALL_ACCESS);
  hklm_key.WriteValue(UTF8ToUTF16(test_policy_definitions::kKeyString).c_str(),
                      UTF8ToUTF16("hklm").c_str());
  RegKey hkcu_key(HKEY_CURRENT_USER, kRegistryMandatorySubKey, KEY_ALL_ACCESS);
  hkcu_key.WriteValue(UTF8ToUTF16(test_policy_definitions::kKeyString).c_str(),
                      UTF8ToUTF16("hkcu").c_str());

  PolicyLoaderWin loader(&test_policy_definitions::kList);
  scoped_ptr<PolicyBundle> bundle(loader.Load());

  PolicyBundle expected_bundle;
  expected_bundle.Get(POLICY_DOMAIN_CHROME, "")
      .Set(test_policy_definitions::kKeyString,
           POLICY_LEVEL_MANDATORY,
           POLICY_SCOPE_MACHINE,
           base::Value::CreateStringValue("hklm"));
  EXPECT_TRUE(bundle->Equals(expected_bundle));
}

// TODO(joaodasilva): share tests for 3rd party policy with
// ConfigDirPolicyProvider once PolicyLoaderWin is able to load all types.
TEST_F(PolicyLoaderWinTest, Load3rdParty) {
  base::DictionaryValue dict;
  dict.SetString("str", "string value");
  dict.SetInteger("int", 123);
  dict.Set("subdict", dict.DeepCopy());
  dict.Set("subsubdict", dict.DeepCopy());
  dict.Set("subsubsubdict", dict.DeepCopy());

  base::DictionaryValue policy_dict;
  policy_dict.Set("3rdparty.extensions.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.policy",
                  dict.DeepCopy());
  policy_dict.Set("3rdparty.extensions.bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.policy",
                  dict.DeepCopy());
  EXPECT_TRUE(InstallDictionary(policy_dict, HKEY_LOCAL_MACHINE,
                                kRegistryMandatorySubKey));

  PolicyBundle expected;
  expected.Get(POLICY_DOMAIN_EXTENSIONS, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
      .LoadFrom(&dict, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE);
  expected.Get(POLICY_DOMAIN_EXTENSIONS, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
      .LoadFrom(&dict, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE);

  PolicyLoaderWin loader(&test_policy_definitions::kList);
  scoped_ptr<PolicyBundle> loaded(loader.Load());
  EXPECT_TRUE(loaded->Equals(expected));
}

TEST_F(PolicyLoaderWinTest, Merge3rdPartyPolicies) {
  // Policy for the same extension will be provided at the 4 level/scope
  // combinations, to verify that they overlap as expected.

  const string16 kPathSuffix =
      kRegistryMandatorySubKey + ASCIIToUTF16("\\3rdparty\\extensions\\merge");
  const string16 kMandatoryPath = kPathSuffix + ASCIIToUTF16("\\policy");
  const string16 kRecommendedPath = kPathSuffix + ASCIIToUTF16("\\recommended");

  const char kUserMandatory[] = "user-mandatory";
  const char kUserRecommended[] = "user-recommended";
  const char kMachineMandatory[] = "machine-mandatory";
  const char kMachineRecommended[] = "machine-recommended";

  base::DictionaryValue policy;
  policy.SetString("a", kMachineMandatory);
  EXPECT_TRUE(InstallDictionary(policy, HKEY_LOCAL_MACHINE, kMandatoryPath));
  policy.SetString("a", kUserMandatory);
  policy.SetString("b", kUserMandatory);
  EXPECT_TRUE(InstallDictionary(policy, HKEY_CURRENT_USER, kMandatoryPath));
  policy.SetString("a", kMachineRecommended);
  policy.SetString("b", kMachineRecommended);
  policy.SetString("c", kMachineRecommended);
  EXPECT_TRUE(InstallDictionary(policy, HKEY_LOCAL_MACHINE, kRecommendedPath));
  policy.SetString("a", kUserRecommended);
  policy.SetString("b", kUserRecommended);
  policy.SetString("c", kUserRecommended);
  policy.SetString("d", kUserRecommended);
  EXPECT_TRUE(InstallDictionary(policy, HKEY_CURRENT_USER, kRecommendedPath));

  PolicyBundle expected;
  PolicyMap& expected_policy = expected.Get(POLICY_DOMAIN_EXTENSIONS, "merge");
  expected_policy.Set("a", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                      base::Value::CreateStringValue(kMachineMandatory));
  expected_policy.Set("b", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                      base::Value::CreateStringValue(kUserMandatory));
  expected_policy.Set("c", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
                      base::Value::CreateStringValue(kMachineRecommended));
  expected_policy.Set("d", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                      base::Value::CreateStringValue(kUserRecommended));

  PolicyLoaderWin loader(&test_policy_definitions::kList);
  scoped_ptr<PolicyBundle> loaded(loader.Load());
  EXPECT_TRUE(loaded->Equals(expected));
}

}  // namespace policy
