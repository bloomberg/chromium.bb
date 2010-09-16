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
#include "chrome/common/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Holds parameters for the parametrized policy tests.
class PolicyTestParams {
 public:
  // Takes ownership of |test_value|.
  PolicyTestParams(ConfigurationPolicyStore::PolicyType type,
                   const char* policy_name,
                   Value* test_value)
      : type_(type),
        policy_name_(policy_name),
        test_value_(test_value) {}

  // testing::TestWithParam does copying, so provide copy constructor and
  // assignment operator.
  PolicyTestParams(const PolicyTestParams& other)
      : type_(other.type_),
        policy_name_(other.policy_name_),
        test_value_(other.test_value_->DeepCopy()) {}

  const PolicyTestParams& operator=(PolicyTestParams other) {
    swap(other);
    return *this;
  }

  void swap(PolicyTestParams& other) {
    std::swap(type_, other.type_);
    std::swap(policy_name_, other.policy_name_);
    test_value_.swap(other.test_value_);
  }

  ConfigurationPolicyStore::PolicyType type() const { return type_; }
  const char* policy_name() const { return policy_name_; }
  const Value* test_value() const { return test_value_.get(); }

  // Get the test value in the appropriate CFPropertyListRef representation.
  CFPropertyListRef GetPropertyListValue() const {
    switch (test_value_->GetType()) {
      case Value::TYPE_BOOLEAN: {
        bool v;
        if (!test_value_->GetAsBoolean(&v))
          return NULL;
        return CFRetain(v ? kCFBooleanTrue : kCFBooleanFalse);
      }
      case Value::TYPE_INTEGER: {
        int v;
        if (!test_value_->GetAsInteger(&v))
          return NULL;
        return CFNumberCreate(NULL, kCFNumberIntType, &v);
      }
      case Value::TYPE_STRING: {
        std::string v;
        if (!test_value_->GetAsString(&v))
          return NULL;
        return base::SysUTF8ToCFStringRef(v);
      }
      case Value::TYPE_LIST: {
        const ListValue* list =
            static_cast<const ListValue*>(test_value_.get());
        scoped_cftyperef<CFMutableArrayRef> array(
            CFArrayCreateMutable(NULL, list->GetSize(),
                                 &kCFTypeArrayCallBacks));
        for (ListValue::const_iterator element(list->begin());
             element != list->end(); ++element) {
          if (!(*element)->IsType(Value::TYPE_STRING))
            return NULL;
          std::string element_value;
          if (!(*element)->GetAsString(&element_value))
            return NULL;
          scoped_cftyperef<CFStringRef> cf_element_value(
              base::SysUTF8ToCFStringRef(element_value));
          CFArrayAppendValue(array, cf_element_value.get());
        }
        return array.release();
      }
      default:
        return NULL;
    }
  }

  // Factory methods that create parameter objects for different value types.
  static PolicyTestParams ForStringPolicy(
      ConfigurationPolicyStore::PolicyType type,
      const char* name) {
    return PolicyTestParams(type, name, Value::CreateStringValue("test"));
  }
  static PolicyTestParams ForBooleanPolicy(
      ConfigurationPolicyStore::PolicyType type,
      const char* name) {
    return PolicyTestParams(type, name, Value::CreateBooleanValue(true));
  }
  static PolicyTestParams ForIntegerPolicy(
      ConfigurationPolicyStore::PolicyType type,
      const char* name) {
    return PolicyTestParams(type, name, Value::CreateIntegerValue(42));
  }
  static PolicyTestParams ForListPolicy(
      ConfigurationPolicyStore::PolicyType type,
      const char* name) {
    ListValue* value = new ListValue;
    value->Set(0U, Value::CreateStringValue("first"));
    value->Set(1U, Value::CreateStringValue("second"));
    return PolicyTestParams(type, name, value);
  }

 private:
  ConfigurationPolicyStore::PolicyType type_;
  const char* policy_name_;
  scoped_ptr<Value> test_value_;
};

// Parametrized test class for testing whether ConfigurationPolicyProviderMac
// can handle all policies correctly.
class ConfigurationPolicyProviderMacTest
    : public testing::TestWithParam<PolicyTestParams> {
 public:
  virtual void SetUp() {
    prefs_ = new MockPreferences;
    store_.reset(new MockConfigurationPolicyStore);
    provider_.reset(
        new ConfigurationPolicyProviderMac(
            ConfigurationPolicyPrefStore::GetChromePolicyValueMap(),
            prefs_));
  }

 protected:
  MockPreferences* prefs_;
  scoped_ptr<MockConfigurationPolicyStore> store_;
  scoped_ptr<ConfigurationPolicyProviderMac> provider_;
};

TEST_P(ConfigurationPolicyProviderMacTest, Default) {
  EXPECT_TRUE(provider_->Provide(store_.get()));
  EXPECT_TRUE(store_->policy_map().empty());
}

TEST_P(ConfigurationPolicyProviderMacTest, Invalid) {
  scoped_cftyperef<CFStringRef> name(
      base::SysUTF8ToCFStringRef(GetParam().policy_name()));
  scoped_cftyperef<CFDataRef> invalid_data(CFDataCreate(NULL, NULL, 0));
  prefs_->AddTestItem(name, invalid_data.get(), true);
  EXPECT_TRUE(provider_->Provide(store_.get()));
  EXPECT_TRUE(store_->policy_map().empty());
}

TEST_P(ConfigurationPolicyProviderMacTest, TestNonForcedValue) {
  scoped_cftyperef<CFStringRef> name(
      base::SysUTF8ToCFStringRef(GetParam().policy_name()));
  scoped_cftyperef<CFPropertyListRef> test_value(
      GetParam().GetPropertyListValue());
  ASSERT_TRUE(test_value.get());
  prefs_->AddTestItem(name, test_value.get(), false);
  EXPECT_TRUE(provider_->Provide(store_.get()));
  EXPECT_TRUE(store_->policy_map().empty());
}

TEST_P(ConfigurationPolicyProviderMacTest, TestValue) {
  scoped_cftyperef<CFStringRef> name(
      base::SysUTF8ToCFStringRef(GetParam().policy_name()));
  scoped_cftyperef<CFPropertyListRef> test_value(
      GetParam().GetPropertyListValue());
  ASSERT_TRUE(test_value.get());
  prefs_->AddTestItem(name, test_value, true);
  EXPECT_TRUE(provider_->Provide(store_.get()));
  ASSERT_EQ(1U, store_->policy_map().size());
  const Value* value = store_->Get(GetParam().type());
  ASSERT_TRUE(value);
  EXPECT_TRUE(GetParam().test_value()->Equals(value));
}

// Instantiate the test case for all policies.
INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyProviderMacTestInstance,
    ConfigurationPolicyProviderMacTest,
    testing::Values(
        PolicyTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyHomePage,
            key::kHomepageLocation),
        PolicyTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyHomepageIsNewTabPage,
            key::kHomepageIsNewTabPage),
        PolicyTestParams::ForIntegerPolicy(
            ConfigurationPolicyStore::kPolicyRestoreOnStartup,
            key::kRestoreOnStartup),
        PolicyTestParams::ForListPolicy(
            ConfigurationPolicyStore::kPolicyURLsToRestoreOnStartup,
            key::kURLsToRestoreOnStartup),
        PolicyTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyDefaultSearchProviderName,
            key::kDefaultSearchProviderName),
        PolicyTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyDefaultSearchProviderKeyword,
            key::kDefaultSearchProviderKeyword),
        PolicyTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyDefaultSearchProviderSearchURL,
            key::kDefaultSearchProviderSearchURL),
        PolicyTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyDefaultSearchProviderSuggestURL,
            key::kDefaultSearchProviderSuggestURL),
        PolicyTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyDefaultSearchProviderIconURL,
            key::kDefaultSearchProviderIconURL),
        PolicyTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyDefaultSearchProviderEncodings,
            key::kDefaultSearchProviderEncodings),
        PolicyTestParams::ForIntegerPolicy(
            ConfigurationPolicyStore::kPolicyProxyServerMode,
            key::kProxyServerMode),
        PolicyTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyProxyServer,
            key::kProxyServer),
        PolicyTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyProxyPacUrl,
            key::kProxyPacUrl),
        PolicyTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyProxyBypassList,
            key::kProxyBypassList),
        PolicyTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyAlternateErrorPagesEnabled,
            key::kAlternateErrorPagesEnabled),
        PolicyTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicySearchSuggestEnabled,
            key::kSearchSuggestEnabled),
        PolicyTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyDnsPrefetchingEnabled,
            key::kDnsPrefetchingEnabled),
        PolicyTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicySafeBrowsingEnabled,
            key::kSafeBrowsingEnabled),
        PolicyTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyMetricsReportingEnabled,
            key::kMetricsReportingEnabled),
        PolicyTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyPasswordManagerEnabled,
            key::kPasswordManagerEnabled),
        PolicyTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyPasswordManagerAllowShowPasswords,
            key::kPasswordManagerAllowShowPasswords),
        PolicyTestParams::ForListPolicy(
            ConfigurationPolicyStore::kPolicyDisabledPlugins,
            key::kDisabledPlugins),
        PolicyTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyAutoFillEnabled,
            key::kAutoFillEnabled),
        PolicyTestParams::ForStringPolicy(
            ConfigurationPolicyStore::kPolicyApplicationLocale,
            key::kApplicationLocaleValue),
        PolicyTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicySyncDisabled,
            key::kSyncDisabled),
        PolicyTestParams::ForListPolicy(
            ConfigurationPolicyStore::kPolicyExtensionInstallAllowList,
            key::kExtensionInstallAllowList),
        PolicyTestParams::ForListPolicy(
            ConfigurationPolicyStore::kPolicyExtensionInstallDenyList,
            key::kExtensionInstallDenyList),
        PolicyTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyShowHomeButton,
            key::kShowHomeButton),
        PolicyTestParams::ForBooleanPolicy(
            ConfigurationPolicyStore::kPolicyPrintingEnabled,
            key::kPrintingEnabled)));

}  // namespace policy
