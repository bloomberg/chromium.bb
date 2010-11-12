// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/file_path.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/chrome_switches.h"

namespace policy {

// Holds a set of test parameters, consisting of pref name and policy type.
class TypeAndName {
 public:
  TypeAndName(ConfigurationPolicyType type, const char* pref_name)
      : type_(type),
        pref_name_(pref_name) {}

  ConfigurationPolicyType type() const { return type_; }
  const char* pref_name() const { return pref_name_; }

 private:
  ConfigurationPolicyType type_;
  const char* pref_name_;
};

// Test cases for list-valued policy settings.
class ConfigurationPolicyPrefStoreListTest
    : public testing::TestWithParam<TypeAndName> {
};

TEST_P(ConfigurationPolicyPrefStoreListTest, GetDefault) {
  ConfigurationPolicyPrefStore store(NULL);
  ListValue* list = NULL;
  EXPECT_FALSE(store.prefs()->GetList(GetParam().pref_name(), &list));
}

TEST_P(ConfigurationPolicyPrefStoreListTest, SetValue) {
  ConfigurationPolicyPrefStore store(NULL);
  ListValue* in_value = new ListValue();
  in_value->Append(Value::CreateStringValue("test1"));
  in_value->Append(Value::CreateStringValue("test2,"));
  store.Apply(GetParam().type(), in_value);
  ListValue* list = NULL;
  EXPECT_TRUE(store.prefs()->GetList(GetParam().pref_name(), &list));
  ListValue::const_iterator current(list->begin());
  const ListValue::const_iterator end(list->end());
  ASSERT_TRUE(current != end);
  std::string value;
  (*current)->GetAsString(&value);
  EXPECT_EQ("test1", value);
  ++current;
  ASSERT_TRUE(current != end);
  (*current)->GetAsString(&value);
  EXPECT_EQ("test2,", value);
  ++current;
  EXPECT_TRUE(current == end);
}

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreListTestInstance,
    ConfigurationPolicyPrefStoreListTest,
    testing::Values(
        TypeAndName(kPolicyURLsToRestoreOnStartup,
                    prefs::kURLsToRestoreOnStartup),
        TypeAndName(kPolicyExtensionInstallAllowList,
                    prefs::kExtensionInstallAllowList),
        TypeAndName(kPolicyExtensionInstallDenyList,
                    prefs::kExtensionInstallDenyList),
        TypeAndName(kPolicyDisabledPlugins,
                    prefs::kPluginsPluginsBlacklist)));

// Test cases for string-valued policy settings.
class ConfigurationPolicyPrefStoreStringTest
    : public testing::TestWithParam<TypeAndName> {
};

TEST_P(ConfigurationPolicyPrefStoreStringTest, GetDefault) {
  ConfigurationPolicyPrefStore store(NULL);
  std::string result;
  EXPECT_FALSE(store.prefs()->GetString(GetParam().pref_name(), &result));
}

TEST_P(ConfigurationPolicyPrefStoreStringTest, SetValue) {
  ConfigurationPolicyPrefStore store(NULL);
  store.Apply(GetParam().type(),
              Value::CreateStringValue("http://chromium.org"));
  std::string result;
  EXPECT_TRUE(store.prefs()->GetString(GetParam().pref_name(), &result));
  EXPECT_EQ(result, "http://chromium.org");
}

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreStringTestInstance,
    ConfigurationPolicyPrefStoreStringTest,
    testing::Values(
        TypeAndName(kPolicyHomePage,
                    prefs::kHomePage),
        TypeAndName(kPolicyProxyServer,
                    prefs::kProxyServer),
        TypeAndName(kPolicyProxyPacUrl,
                    prefs::kProxyPacUrl),
        TypeAndName(kPolicyProxyBypassList,
                    prefs::kProxyBypassList),
        TypeAndName(kPolicyApplicationLocale,
                    prefs::kApplicationLocale),
        TypeAndName(kPolicyApplicationLocale,
                    prefs::kApplicationLocale),
        TypeAndName(kPolicyAuthSchemes,
                    prefs::kAuthSchemes),
        TypeAndName(kPolicyAuthServerWhitelist,
                    prefs::kAuthServerWhitelist),
        TypeAndName(kPolicyAuthNegotiateDelegateWhitelist,
                    prefs::kAuthNegotiateDelegateWhitelist),
        TypeAndName(kPolicyGSSAPILibraryName,
                    prefs::kGSSAPILibraryName)));

// Test cases for boolean-valued policy settings.
class ConfigurationPolicyPrefStoreBooleanTest
    : public testing::TestWithParam<TypeAndName> {
};

TEST_P(ConfigurationPolicyPrefStoreBooleanTest, GetDefault) {
  ConfigurationPolicyPrefStore store(NULL);
  bool result = false;
  EXPECT_FALSE(store.prefs()->GetBoolean(GetParam().pref_name(), &result));
}

TEST_P(ConfigurationPolicyPrefStoreBooleanTest, SetValue) {
  ConfigurationPolicyPrefStore store(NULL);
  store.Apply(GetParam().type(), Value::CreateBooleanValue(false));
  bool result = true;
  EXPECT_TRUE(store.prefs()->GetBoolean(GetParam().pref_name(), &result));
  EXPECT_FALSE(result);

  store.Apply(GetParam().type(), Value::CreateBooleanValue(true));
  result = false;
  EXPECT_TRUE(store.prefs()->GetBoolean(GetParam().pref_name(), &result));
  EXPECT_TRUE(result);
}

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreBooleanTestInstance,
    ConfigurationPolicyPrefStoreBooleanTest,
    testing::Values(
        TypeAndName(kPolicyHomepageIsNewTabPage,
                    prefs::kHomePageIsNewTabPage),
        TypeAndName(kPolicyAlternateErrorPagesEnabled,
                    prefs::kAlternateErrorPagesEnabled),
        TypeAndName(kPolicySearchSuggestEnabled,
                    prefs::kSearchSuggestEnabled),
        TypeAndName(kPolicyDnsPrefetchingEnabled,
                    prefs::kDnsPrefetchingEnabled),
        TypeAndName(kPolicyDisableSpdy,
                    prefs::kDisableSpdy),
        TypeAndName(kPolicySafeBrowsingEnabled,
                    prefs::kSafeBrowsingEnabled),
        TypeAndName(kPolicyMetricsReportingEnabled,
                    prefs::kMetricsReportingEnabled),
        TypeAndName(kPolicyPasswordManagerEnabled,
                    prefs::kPasswordManagerEnabled),
        TypeAndName(kPolicyPasswordManagerAllowShowPasswords,
                    prefs::kPasswordManagerAllowShowPasswords),
        TypeAndName(kPolicyShowHomeButton,
                    prefs::kShowHomeButton),
        TypeAndName(kPolicyPrintingEnabled,
                    prefs::kPrintingEnabled),
        TypeAndName(kPolicyJavascriptEnabled,
                    prefs::kWebKitJavascriptEnabled),
        TypeAndName(kPolicySavingBrowserHistoryDisabled,
                    prefs::kSavingBrowserHistoryDisabled),
        TypeAndName(kPolicySavingBrowserHistoryDisabled,
                    prefs::kSavingBrowserHistoryDisabled),
        TypeAndName(kPolicyDisableAuthNegotiateCnameLookup,
                    prefs::kDisableAuthNegotiateCnameLookup),
        TypeAndName(kPolicyEnableAuthNegotiatePort,
                    prefs::kEnableAuthNegotiatePort)));

#if defined(OS_CHROMEOS)
INSTANTIATE_TEST_CASE_P(
    CrosConfigurationPolicyPrefStoreBooleanTestInstance,
    ConfigurationPolicyPrefStoreBooleanTest,
    testing::Values(
        TypeAndName(kPolicyChromeOsLockOnIdleSuspend,
                    prefs::kEnableScreenLock)));
#endif  // defined(OS_CHROMEOS)

// Test cases for integer-valued policy settings.
class ConfigurationPolicyPrefStoreIntegerTest
    : public testing::TestWithParam<TypeAndName> {
};

TEST_P(ConfigurationPolicyPrefStoreIntegerTest, GetDefault) {
  ConfigurationPolicyPrefStore store(NULL);
  int result = 0;
  EXPECT_FALSE(store.prefs()->GetInteger(GetParam().pref_name(), &result));
}

TEST_P(ConfigurationPolicyPrefStoreIntegerTest, SetValue) {
  ConfigurationPolicyPrefStore store(NULL);
  store.Apply(GetParam().type(), Value::CreateIntegerValue(2));
  int result = 0;
  EXPECT_TRUE(store.prefs()->GetInteger(GetParam().pref_name(), &result));
  EXPECT_EQ(result, 2);
}

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreIntegerTestInstance,
    ConfigurationPolicyPrefStoreIntegerTest,
    testing::Values(
        TypeAndName(kPolicyRestoreOnStartup,
                    prefs::kRestoreOnStartup)));

// Test cases for the proxy policy settings.
class ConfigurationPolicyPrefStoreProxyTest : public testing::Test {
};

TEST_F(ConfigurationPolicyPrefStoreProxyTest, ManualOptions) {
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(kPolicyProxyBypassList,
                      Value::CreateStringValue("http://chromium.org/override"));
  provider->AddPolicy(kPolicyProxyPacUrl,
                      Value::CreateStringValue("http://short.org/proxy.pac"));
  provider->AddPolicy(kPolicyProxyServer,
                      Value::CreateStringValue("chromium.org"));
  provider->AddPolicy(kPolicyProxyServerMode,
                      Value::CreateIntegerValue(
                          kPolicyManuallyConfiguredProxyMode));

  ConfigurationPolicyPrefStore store(provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::string string_result;
  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyBypassList,
                                       &string_result));
  EXPECT_EQ("http://chromium.org/override", string_result);
  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_EQ("http://short.org/proxy.pac", string_result);
  EXPECT_TRUE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  EXPECT_EQ("chromium.org", string_result);
  bool bool_result;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_FALSE(bool_result);
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect, &bool_result));
  EXPECT_FALSE(bool_result);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, NoProxy) {
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(kPolicyProxyBypassList,
                      Value::CreateStringValue("http://chromium.org/override"));
  provider->AddPolicy(kPolicyProxyServerMode,
                      Value::CreateIntegerValue(
                          kPolicyNoProxyServerMode));

  ConfigurationPolicyPrefStore store(provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::string string_result;
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyBypassList,
                                        &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  bool bool_result;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_TRUE(bool_result);
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect, &bool_result));
  EXPECT_FALSE(bool_result);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, NoProxyReversedApplyOrder) {
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(kPolicyProxyServerMode,
                      Value::CreateIntegerValue(
                          kPolicyNoProxyServerMode));
  provider->AddPolicy(kPolicyProxyBypassList,
                      Value::CreateStringValue("http://chromium.org/override"));

  ConfigurationPolicyPrefStore store(provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::string string_result;
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyBypassList,
                                        &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  bool bool_result;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_TRUE(bool_result);
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect, &bool_result));
  EXPECT_FALSE(bool_result);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, AutoDetect) {
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(kPolicyProxyServerMode,
                      Value::CreateIntegerValue(
                          kPolicyAutoDetectProxyMode));

  ConfigurationPolicyPrefStore store(provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::string string_result;
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyBypassList,
                                        &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  bool bool_result;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_FALSE(bool_result);
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect, &bool_result));
  EXPECT_TRUE(bool_result);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, UseSystem) {
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(kPolicyProxyBypassList,
                      Value::CreateStringValue("http://chromium.org/override"));
  provider->AddPolicy(kPolicyProxyServerMode,
                      Value::CreateIntegerValue(
                          kPolicyUseSystemProxyMode));

  ConfigurationPolicyPrefStore store(provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::string string_result;
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyBypassList,
                                        &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  bool bool_result;
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect,
                                         &bool_result));
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, UseSystemReversedApplyOrder) {
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(kPolicyProxyServerMode,
                      Value::CreateIntegerValue(
                          kPolicyUseSystemProxyMode));
  provider->AddPolicy(kPolicyProxyBypassList,
                      Value::CreateStringValue("http://chromium.org/override"));

  ConfigurationPolicyPrefStore store(provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);

  std::string string_result;
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyBypassList,
                                        &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyPacUrl, &string_result));
  EXPECT_FALSE(store.prefs()->GetString(prefs::kProxyServer, &string_result));
  bool bool_result;
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kNoProxyServer, &bool_result));
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kProxyAutoDetect,
                                         &bool_result));
}

class ConfigurationPolicyPrefStoreDefaultSearchTest : public testing::Test {
};

// Checks that if the policy for default search is valid, i.e. there's a
// search URL, that all the elements have been given proper defaults.
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, MinimallyDefined) {
  const char* const search_url = "http://test.com/search?t={searchTerms}";
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(
      kPolicyDefaultSearchProviderEnabled,
      Value::CreateBooleanValue(true));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderSearchURL,
      Value::CreateStringValue(search_url));

  ConfigurationPolicyPrefStore store(provider.get());

  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);
  const DictionaryValue* prefs = store.prefs();

  std::string string_result;
  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderSearchURL,
                               &string_result));
  EXPECT_EQ(string_result, search_url);

  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderName,
                               &string_result));
  EXPECT_EQ(string_result, "test.com");

  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderKeyword,
                               &string_result));
  EXPECT_EQ(string_result, std::string());

  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderSuggestURL,
                               &string_result));
  EXPECT_EQ(string_result, std::string());

  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderIconURL,
                               &string_result));
  EXPECT_EQ(string_result, std::string());

  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderEncodings,
                               &string_result));
  EXPECT_EQ(string_result, std::string());
}

// Checks that for a fully defined search policy, all elements have been
// read properly.
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, FullyDefined) {
  const char* const search_url = "http://test.com/search?t={searchTerms}";
  const char* const suggest_url = "http://test.com/sugg?={searchTerms}";
  const char* const icon_url = "http://test.com/icon.jpg";
  const char* const name = "MyName";
  const char* const keyword = "MyKeyword";
  const char* const encodings = "UTF-16;UTF-8";
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(
      kPolicyDefaultSearchProviderEnabled,
      Value::CreateBooleanValue(true));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderSearchURL,
      Value::CreateStringValue(search_url));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderName,
      Value::CreateStringValue(name));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderKeyword,
      Value::CreateStringValue(keyword));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderSuggestURL,
      Value::CreateStringValue(suggest_url));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderIconURL,
      Value::CreateStringValue(icon_url));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderEncodings,
      Value::CreateStringValue(encodings));

  ConfigurationPolicyPrefStore store(provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);
  const DictionaryValue* prefs = store.prefs();

  std::string result_search_url;
  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderSearchURL,
                               &result_search_url));
  EXPECT_EQ(result_search_url, search_url);

  std::string result_name;
  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderName,
                               &result_name));
  EXPECT_EQ(result_name, name);

  std::string result_keyword;
  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderKeyword,
                               &result_keyword));
  EXPECT_EQ(result_keyword, keyword);

  std::string result_suggest_url;
  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderSuggestURL,
                               &result_suggest_url));
  EXPECT_EQ(result_suggest_url, suggest_url);

  std::string result_icon_url;
  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderIconURL,
                               &result_icon_url));
  EXPECT_EQ(result_icon_url, icon_url);

  std::string result_encodings;
  EXPECT_TRUE(prefs->GetString(prefs::kDefaultSearchProviderEncodings,
                               &result_encodings));
  EXPECT_EQ(result_encodings, encodings);
}

// Checks that if the default search policy is missing, that no elements of the
// default search policy will be present.
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, MissingUrl) {
  const char* const suggest_url = "http://test.com/sugg?t={searchTerms}";
  const char* const icon_url = "http://test.com/icon.jpg";
  const char* const name = "MyName";
  const char* const keyword = "MyKeyword";
  const char* const encodings = "UTF-16;UTF-8";
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(
      kPolicyDefaultSearchProviderEnabled,
      Value::CreateBooleanValue(true));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderName,
      Value::CreateStringValue(name));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderKeyword,
      Value::CreateStringValue(keyword));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderSuggestURL,
      Value::CreateStringValue(suggest_url));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderIconURL,
      Value::CreateStringValue(icon_url));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderEncodings,
      Value::CreateStringValue(encodings));

  ConfigurationPolicyPrefStore store(provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);
  const DictionaryValue* prefs = store.prefs();

  std::string string_result;
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderSearchURL,
                                &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderName,
                                &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderKeyword,
                                &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderSuggestURL,
                                &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderIconURL,
                                &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderEncodings,
                                &string_result));
}

// Checks that if the default search policy is invalid, that no elements of the
// default search policy will be present.
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, Invalid) {
  const char* const bad_search_url = "http://test.com/noSearchTerms";
  const char* const suggest_url = "http://test.com/sugg?t={searchTerms}";
  const char* const icon_url = "http://test.com/icon.jpg";
  const char* const name = "MyName";
  const char* const keyword = "MyKeyword";
  const char* const encodings = "UTF-16;UTF-8";
  scoped_ptr<MockConfigurationPolicyProvider> provider(
      new MockConfigurationPolicyProvider());
  provider->AddPolicy(
      kPolicyDefaultSearchProviderEnabled,
      Value::CreateBooleanValue(true));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderSearchURL,
      Value::CreateStringValue(bad_search_url));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderName,
      Value::CreateStringValue(name));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderKeyword,
      Value::CreateStringValue(keyword));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderSuggestURL,
      Value::CreateStringValue(suggest_url));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderIconURL,
      Value::CreateStringValue(icon_url));
  provider->AddPolicy(
      kPolicyDefaultSearchProviderEncodings,
      Value::CreateStringValue(encodings));

  ConfigurationPolicyPrefStore store(provider.get());
  EXPECT_EQ(store.ReadPrefs(), PrefStore::PREF_READ_ERROR_NONE);
  const DictionaryValue* const prefs = store.prefs();

  std::string string_result;
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderEnabled,
                                &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderSearchURL,
                                &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderName,
                                &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderKeyword,
                                &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderSuggestURL,
                                &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderIconURL,
                                &string_result));
  EXPECT_FALSE(prefs->GetString(prefs::kDefaultSearchProviderEncodings,
                                &string_result));
}

// Test cases for the Sync policy setting.
class ConfigurationPolicyPrefStoreSyncTest : public testing::Test {
};

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Default) {
  ConfigurationPolicyPrefStore store(NULL);
  bool result = false;
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kSyncManaged, &result));
}

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Enabled) {
  ConfigurationPolicyPrefStore store(NULL);
  store.Apply(kPolicySyncDisabled, Value::CreateBooleanValue(false));
  // Enabling Sync should not set the pref.
  bool result = false;
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kSyncManaged, &result));
}

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Disabled) {
  ConfigurationPolicyPrefStore store(NULL);
  store.Apply(kPolicySyncDisabled, Value::CreateBooleanValue(true));
  // Sync should be flagged as managed.
  bool result = false;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kSyncManaged, &result));
  EXPECT_TRUE(result);
}

// Test cases for the AutoFill policy setting.
class ConfigurationPolicyPrefStoreAutoFillTest : public testing::Test {
};

TEST_F(ConfigurationPolicyPrefStoreAutoFillTest, Default) {
  ConfigurationPolicyPrefStore store(NULL);
  bool result = false;
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kAutoFillEnabled, &result));
}

TEST_F(ConfigurationPolicyPrefStoreAutoFillTest, Enabled) {
  ConfigurationPolicyPrefStore store(NULL);
  store.Apply(kPolicyAutoFillEnabled, Value::CreateBooleanValue(true));
  // Enabling AutoFill should not set the pref.
  bool result = false;
  EXPECT_FALSE(store.prefs()->GetBoolean(prefs::kAutoFillEnabled, &result));
}

TEST_F(ConfigurationPolicyPrefStoreAutoFillTest, Disabled) {
  ConfigurationPolicyPrefStore store(NULL);
  store.Apply(kPolicyAutoFillEnabled, Value::CreateBooleanValue(false));
  // Disabling AutoFill should switch the pref to managed.
  bool result = true;
  EXPECT_TRUE(store.prefs()->GetBoolean(prefs::kAutoFillEnabled, &result));
  EXPECT_FALSE(result);
}

}  // namespace policy
