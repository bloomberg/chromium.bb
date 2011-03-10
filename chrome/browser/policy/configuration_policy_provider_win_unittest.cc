// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <windows.h>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/policy/asynchronous_policy_loader.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider_win.h"
#include "chrome/browser/policy/mock_configuration_policy_store.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::RegKey;

namespace policy {

const wchar_t kUnitTestRegistrySubKey[] = L"SOFTWARE\\Chromium Unit Tests";
const wchar_t kUnitTestMachineOverrideSubKey[] =
    L"SOFTWARE\\Chromium Unit Tests\\HKLM Override";
const wchar_t kUnitTestUserOverrideSubKey[] =
    L"SOFTWARE\\Chromium Unit Tests\\HKCU Override";

namespace {

// Holds policy type, corresponding policy name string and a valid value for use
// in parametrized value tests.
class PolicyTestParams {
 public:
  // Assumes ownership of |hklm_value| and |hkcu_value|.
  PolicyTestParams(ConfigurationPolicyType type,
                   const char* policy_name,
                   Value* hklm_value,
                   Value* hkcu_value)
      : type_(type),
        policy_name_(policy_name),
        hklm_value_(hklm_value),
        hkcu_value_(hkcu_value) {}

  // testing::TestWithParam does copy the parameters, so provide copy
  // constructor and assignment operator.
  PolicyTestParams(const PolicyTestParams& other)
      : type_(other.type_),
        policy_name_(other.policy_name_),
        hklm_value_(other.hklm_value_->DeepCopy()),
        hkcu_value_(other.hkcu_value_->DeepCopy()) {}

  const PolicyTestParams& operator=(PolicyTestParams other) {
    swap(other);
    return *this;
  }

  void swap(PolicyTestParams& other) {
    std::swap(type_, other.type_);
    std::swap(policy_name_, other.policy_name_);
    hklm_value_.swap(other.hklm_value_);
    hkcu_value_.swap(other.hkcu_value_);
  }

  ConfigurationPolicyType type() const { return type_; }
  const char* policy_name() const { return policy_name_; }
  const Value* hklm_value() const { return hklm_value_.get(); }
  const Value* hkcu_value() const { return hkcu_value_.get(); }

  // Factory methods for different value types.
  static PolicyTestParams ForStringPolicy(
      ConfigurationPolicyType type,
      const char* policy_name) {
    return PolicyTestParams(type,
                            policy_name,
                            Value::CreateStringValue("string_a"),
                            Value::CreateStringValue("string_b"));
  }
  static PolicyTestParams ForBooleanPolicy(
      ConfigurationPolicyType type,
      const char* policy_name) {
    return PolicyTestParams(type,
                            policy_name,
                            Value::CreateBooleanValue(true),
                            Value::CreateBooleanValue(false));
  }
  static PolicyTestParams ForIntegerPolicy(
      ConfigurationPolicyType type,
      const char* policy_name) {
    return PolicyTestParams(type,
                            policy_name,
                            Value::CreateIntegerValue(42),
                            Value::CreateIntegerValue(17));
  }
  static PolicyTestParams ForListPolicy(
      ConfigurationPolicyType type,
      const char* policy_name) {
    ListValue* hklm_value = new ListValue;
    hklm_value->Set(0U, Value::CreateStringValue("It's a plane!"));
    ListValue* hkcu_value = new ListValue;
    hkcu_value->Set(0U, Value::CreateStringValue("It's a bird!"));
    hkcu_value->Set(0U, Value::CreateStringValue("It's a flying carpet!"));
    return PolicyTestParams(type, policy_name, hklm_value, hkcu_value);
  }

 private:
  ConfigurationPolicyType type_;
  const char* policy_name_;
  scoped_ptr<Value> hklm_value_;
  scoped_ptr<Value> hkcu_value_;
};

}  // namespace

// This test class provides sandboxing and mocking for the parts of the
// Windows Registry implementing Group Policy. The |SetUp| method prepares
// two temporary sandbox keys in |kUnitTestRegistrySubKey|, one for HKLM and one
// for HKCU. A test's calls to the registry are redirected by Windows to these
// sandboxes, allowing the tests to manipulate and access policy as if it
// were active, but without actually changing the parts of the Registry that
// are managed by Group Policy.
class ConfigurationPolicyProviderWinTest
    : public testing::TestWithParam<PolicyTestParams> {
 public:
  ConfigurationPolicyProviderWinTest();

  // testing::Test method overrides:
  virtual void SetUp();
  virtual void TearDown();

  void ActivateOverrides();
  void DeactivateOverrides();

  // Deletes the registry key created during the tests.
  void DeleteRegistrySandbox();

  // Write a string value to the registry.
  void WriteString(HKEY hive, const char* name, const wchar_t* value);
  // Write a DWORD value to the registry.
  void WriteDWORD(HKEY hive, const char* name, DWORD value);

  // Write the given value to the registry.
  void WriteValue(HKEY hive, const char* name, const Value* value);
  // Write a value that is not compatible with the given |value|.
  void WriteInvalidValue(HKEY hive, const char* name, const Value* value);

 protected:
  scoped_ptr<MockConfigurationPolicyStore> store_;
  scoped_ptr<ConfigurationPolicyProviderWin> provider_;

  // A message loop must be declared and instantiated for these tests,
  // because Windows policy provider create WaitableEvents and
  // ObjectWatchers that require the tests to have a MessageLoop associated
  // with the thread executing the tests.
  MessageLoop loop_;

 private:
  BrowserThread ui_thread_;
  BrowserThread file_thread_;

  // Keys are created for the lifetime of a test to contain
  // the sandboxed HKCU and HKLM hives, respectively.
  RegKey temp_hkcu_hive_key_;
  RegKey temp_hklm_hive_key_;
};

ConfigurationPolicyProviderWinTest::ConfigurationPolicyProviderWinTest()
    : ui_thread_(BrowserThread::UI, &loop_),
      file_thread_(BrowserThread::FILE, &loop_),
      temp_hklm_hive_key_(HKEY_CURRENT_USER, kUnitTestMachineOverrideSubKey,
                          KEY_READ),
      temp_hkcu_hive_key_(HKEY_CURRENT_USER, kUnitTestUserOverrideSubKey,
                          KEY_READ) {
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

  store_.reset(new MockConfigurationPolicyStore);
  provider_.reset(new ConfigurationPolicyProviderWin(
      ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList()));
}

void ConfigurationPolicyProviderWinTest::TearDown() {
  DeactivateOverrides();
  DeleteRegistrySandbox();
  loop_.RunAllPending();
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

void ConfigurationPolicyProviderWinTest::DeleteRegistrySandbox() {
  temp_hklm_hive_key_.Close();
  temp_hkcu_hive_key_.Close();
  RegKey key(HKEY_CURRENT_USER, kUnitTestRegistrySubKey, KEY_ALL_ACCESS);
  key.DeleteKey(L"");
}

void ConfigurationPolicyProviderWinTest::WriteString(HKEY hive,
                                                     const char* name,
                                                     const wchar_t* value) {
  RegKey key(hive, policy::kRegistrySubKey, KEY_ALL_ACCESS);
  key.WriteValue(UTF8ToUTF16(name).c_str(), value);
}

void ConfigurationPolicyProviderWinTest::WriteDWORD(HKEY hive,
                                                    const char* name,
                                                    DWORD value) {
  RegKey key(hive, policy::kRegistrySubKey, KEY_ALL_ACCESS);
  key.WriteValue(UTF8ToUTF16(name).c_str(), value);
}

void ConfigurationPolicyProviderWinTest::WriteValue(HKEY hive,
                                                    const char* name,
                                                    const Value* value) {
  switch (value->GetType()) {
    case Value::TYPE_BOOLEAN: {
      bool v;
      ASSERT_TRUE(value->GetAsBoolean(&v));
      WriteDWORD(hive, name, v);
      break;
    }
    case Value::TYPE_INTEGER: {
      int v;
      ASSERT_TRUE(value->GetAsInteger(&v));
      WriteDWORD(hive, name, v);
      break;
    }
    case Value::TYPE_STRING: {
      std::string v;
      ASSERT_TRUE(value->GetAsString(&v));
      WriteString(hive, name, UTF8ToUTF16(v).c_str());
      break;
    }
    case Value::TYPE_LIST: {
      const ListValue* list = static_cast<const ListValue*>(value);
      RegKey key(hive,
                 (string16(policy::kRegistrySubKey) + ASCIIToUTF16("\\") +
                  UTF8ToUTF16(name)).c_str(),
                 KEY_ALL_ACCESS);
      int index = 1;
      for (ListValue::const_iterator element(list->begin());
           element != list->end(); ++element) {
        ASSERT_TRUE((*element)->IsType(Value::TYPE_STRING));
        std::string element_value;
        ASSERT_TRUE((*element)->GetAsString(&element_value));
        key.WriteValue(base::IntToString16(index++).c_str(),
                       UTF8ToUTF16(element_value).c_str());
      }
      break;
    }
    default:
      FAIL() << "Unsupported value type " << value->GetType();
      break;
  }
}

void ConfigurationPolicyProviderWinTest::WriteInvalidValue(HKEY hive,
                                                           const char* name,
                                                           const Value* value) {
  if (value->IsType(Value::TYPE_STRING))
    WriteDWORD(hive, name, -1);
  else
    WriteString(hive, name, L"bad value");
}

TEST_P(ConfigurationPolicyProviderWinTest, Default) {
  provider_->Provide(store_.get());
  EXPECT_TRUE(store_->policy_map().empty());
}

TEST_P(ConfigurationPolicyProviderWinTest, InvalidValue) {
  WriteInvalidValue(HKEY_LOCAL_MACHINE,
                    GetParam().policy_name(),
                    GetParam().hklm_value());
  WriteInvalidValue(HKEY_CURRENT_USER,
                    GetParam().policy_name(),
                    GetParam().hkcu_value());
  provider_->loader()->Reload();
  loop_.RunAllPending();
  provider_->Provide(store_.get());
  EXPECT_TRUE(store_->policy_map().empty());
}

TEST_P(ConfigurationPolicyProviderWinTest, HKLM) {
  WriteValue(HKEY_LOCAL_MACHINE,
             GetParam().policy_name(),
             GetParam().hklm_value());
  provider_->loader()->Reload();
  loop_.RunAllPending();
  provider_->Provide(store_.get());
  const Value* value = store_->Get(GetParam().type());
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->Equals(GetParam().hklm_value()));
}

TEST_P(ConfigurationPolicyProviderWinTest, HKCU) {
  WriteValue(HKEY_CURRENT_USER,
             GetParam().policy_name(),
             GetParam().hkcu_value());
  provider_->loader()->Reload();
  loop_.RunAllPending();
  provider_->Provide(store_.get());
  const Value* value = store_->Get(GetParam().type());
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->Equals(GetParam().hkcu_value()));
}

TEST_P(ConfigurationPolicyProviderWinTest, HKLMOverHKCU) {
  WriteValue(HKEY_LOCAL_MACHINE,
             GetParam().policy_name(),
             GetParam().hklm_value());
  WriteValue(HKEY_CURRENT_USER,
             GetParam().policy_name(),
             GetParam().hkcu_value());
  provider_->loader()->Reload();
  loop_.RunAllPending();
  provider_->Provide(store_.get());
  const Value* value = store_->Get(GetParam().type());
  ASSERT_TRUE(value);
  EXPECT_TRUE(value->Equals(GetParam().hklm_value()));
}

// Instantiate the test case for all supported policies.
INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyProviderWinTestInstance,
    ConfigurationPolicyProviderWinTest,
    testing::Values(
        PolicyTestParams::ForStringPolicy(
            kPolicyHomepageLocation,
            key::kHomepageLocation),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyHomepageIsNewTabPage,
            key::kHomepageIsNewTabPage),
        PolicyTestParams::ForIntegerPolicy(
            kPolicyRestoreOnStartup,
            key::kRestoreOnStartup),
        PolicyTestParams::ForListPolicy(
            kPolicyRestoreOnStartupURLs,
            key::kRestoreOnStartupURLs),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyDefaultSearchProviderEnabled,
            key::kDefaultSearchProviderEnabled),
        PolicyTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderName,
            key::kDefaultSearchProviderName),
        PolicyTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderKeyword,
            key::kDefaultSearchProviderKeyword),
        PolicyTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderSearchURL,
            key::kDefaultSearchProviderSearchURL),
        PolicyTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderSuggestURL,
            key::kDefaultSearchProviderSuggestURL),
        PolicyTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderInstantURL,
            key::kDefaultSearchProviderInstantURL),
        PolicyTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderIconURL,
            key::kDefaultSearchProviderIconURL),
        PolicyTestParams::ForStringPolicy(
            kPolicyDefaultSearchProviderEncodings,
            key::kDefaultSearchProviderEncodings),
        PolicyTestParams::ForStringPolicy(
            kPolicyProxyMode,
            key::kProxyMode),
        PolicyTestParams::ForIntegerPolicy(
            kPolicyProxyServerMode,
            key::kProxyServerMode),
        PolicyTestParams::ForStringPolicy(
            kPolicyProxyServer,
            key::kProxyServer),
        PolicyTestParams::ForStringPolicy(
            kPolicyProxyPacUrl,
            key::kProxyPacUrl),
        PolicyTestParams::ForStringPolicy(
            kPolicyProxyBypassList,
            key::kProxyBypassList),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyAlternateErrorPagesEnabled,
            key::kAlternateErrorPagesEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicySearchSuggestEnabled,
            key::kSearchSuggestEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyDnsPrefetchingEnabled,
            key::kDnsPrefetchingEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicySafeBrowsingEnabled,
            key::kSafeBrowsingEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyMetricsReportingEnabled,
            key::kMetricsReportingEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyPasswordManagerEnabled,
            key::kPasswordManagerEnabled),
        PolicyTestParams::ForListPolicy(
            kPolicyDisabledPlugins,
            key::kDisabledPlugins),
        PolicyTestParams::ForListPolicy(
            kPolicyDisabledPluginsExceptions,
            key::kDisabledPluginsExceptions),
        PolicyTestParams::ForListPolicy(
            kPolicyEnabledPlugins,
            key::kEnabledPlugins),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyAutoFillEnabled,
            key::kAutoFillEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicySyncDisabled,
            key::kSyncDisabled),
        PolicyTestParams::ForStringPolicy(
            kPolicyApplicationLocaleValue,
            key::kApplicationLocaleValue),
        PolicyTestParams::ForListPolicy(
            kPolicyExtensionInstallWhitelist,
            key::kExtensionInstallWhitelist),
        PolicyTestParams::ForListPolicy(
            kPolicyExtensionInstallBlacklist,
            key::kExtensionInstallBlacklist),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyShowHomeButton,
            key::kShowHomeButton),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyPrintingEnabled,
            key::kPrintingEnabled),
        PolicyTestParams::ForIntegerPolicy(
            kPolicyPolicyRefreshRate,
            key::kPolicyRefreshRate),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyInstantEnabled,
            key::kInstantEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyIncognitoEnabled,
            key::kIncognitoEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyDisablePluginFinder,
            key::kDisablePluginFinder),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyClearSiteDataOnExit,
            key::kClearSiteDataOnExit),
        PolicyTestParams::ForStringPolicy(
            kPolicyDownloadDirectory,
            key::kDownloadDirectory),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyDefaultBrowserSettingEnabled,
            key::kDefaultBrowserSettingEnabled),
        PolicyTestParams::ForBooleanPolicy(
            kPolicyCloudPrintProxyEnabled,
            key::kCloudPrintProxyEnabled)));

}  // namespace policy
