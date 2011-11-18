// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_store_observer_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

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

template<typename TESTBASE>
class ConfigurationPolicyPrefStoreTestBase : public TESTBASE {
 protected:
  ConfigurationPolicyPrefStoreTestBase()
      : provider_(),
        store_(new ConfigurationPolicyPrefStore(&provider_)) {}

  MockConfigurationPolicyProvider provider_;
  scoped_refptr<ConfigurationPolicyPrefStore> store_;
};

// Test cases for list-valued policy settings.
class ConfigurationPolicyPrefStoreListTest
    : public ConfigurationPolicyPrefStoreTestBase<
                 testing::TestWithParam<TypeAndName> > {
};

TEST_P(ConfigurationPolicyPrefStoreListTest, GetDefault) {
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(GetParam().pref_name(), NULL));
}

TEST_P(ConfigurationPolicyPrefStoreListTest, SetValue) {
  ListValue* in_value = new ListValue();
  in_value->Append(Value::CreateStringValue("test1"));
  in_value->Append(Value::CreateStringValue("test2,"));
  provider_.AddPolicy(GetParam().type(), in_value);
  store_->OnUpdatePolicy();
  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_OK,
            store_->GetValue(GetParam().pref_name(), &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(in_value->Equals(value));
}

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreListTestInstance,
    ConfigurationPolicyPrefStoreListTest,
    testing::Values(
        TypeAndName(kPolicyRestoreOnStartupURLs,
                    prefs::kURLsToRestoreOnStartup),
        TypeAndName(kPolicyExtensionInstallWhitelist,
                    prefs::kExtensionInstallAllowList),
        TypeAndName(kPolicyExtensionInstallBlacklist,
                    prefs::kExtensionInstallDenyList),
        TypeAndName(kPolicyDisabledPlugins,
                    prefs::kPluginsDisabledPlugins),
        TypeAndName(kPolicyDisabledPluginsExceptions,
                    prefs::kPluginsDisabledPluginsExceptions),
        TypeAndName(kPolicyEnabledPlugins,
                    prefs::kPluginsEnabledPlugins),
        TypeAndName(kPolicyDisabledSchemes,
                    prefs::kDisabledSchemes),
        TypeAndName(kPolicyAutoSelectCertificateForUrls,
                    prefs::kManagedAutoSelectCertificateForUrls),
        TypeAndName(kPolicyURLBlacklist,
                    prefs::kUrlBlacklist),
        TypeAndName(kPolicyURLWhitelist,
                    prefs::kUrlWhitelist)));

// Test cases for string-valued policy settings.
class ConfigurationPolicyPrefStoreStringTest
    : public ConfigurationPolicyPrefStoreTestBase<
                 testing::TestWithParam<TypeAndName> > {
};

TEST_P(ConfigurationPolicyPrefStoreStringTest, GetDefault) {
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(GetParam().pref_name(), NULL));
}

TEST_P(ConfigurationPolicyPrefStoreStringTest, SetValue) {
  provider_.AddPolicy(GetParam().type(),
                      Value::CreateStringValue("http://chromium.org"));
  store_->OnUpdatePolicy();
  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_OK,
            store_->GetValue(GetParam().pref_name(), &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(StringValue("http://chromium.org").Equals(value));
}

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreStringTestInstance,
    ConfigurationPolicyPrefStoreStringTest,
    testing::Values(
        TypeAndName(kPolicyHomepageLocation,
                    prefs::kHomePage),
        TypeAndName(kPolicyApplicationLocaleValue,
                    prefs::kApplicationLocale),
        TypeAndName(kPolicyApplicationLocaleValue,
                    prefs::kApplicationLocale),
        TypeAndName(kPolicyAuthSchemes,
                    prefs::kAuthSchemes),
        TypeAndName(kPolicyAuthServerWhitelist,
                    prefs::kAuthServerWhitelist),
        TypeAndName(kPolicyAuthNegotiateDelegateWhitelist,
                    prefs::kAuthNegotiateDelegateWhitelist),
        TypeAndName(kPolicyGSSAPILibraryName,
                    prefs::kGSSAPILibraryName),
        TypeAndName(kPolicyDiskCacheDir,
                    prefs::kDiskCacheDir)));

#if !defined(OS_CHROMEOS)
INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreDownloadDirectoryInstance,
    ConfigurationPolicyPrefStoreStringTest,
    testing::Values(TypeAndName(kPolicyDownloadDirectory,
                                prefs::kDownloadDefaultDirectory)));
#endif  // !defined(OS_CHROMEOS)

// Test cases for boolean-valued policy settings.
class ConfigurationPolicyPrefStoreBooleanTest
    : public ConfigurationPolicyPrefStoreTestBase<
                 testing::TestWithParam<TypeAndName> > {
};

TEST_P(ConfigurationPolicyPrefStoreBooleanTest, GetDefault) {
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(GetParam().pref_name(), NULL));
}

TEST_P(ConfigurationPolicyPrefStoreBooleanTest, SetValue) {
  provider_.AddPolicy(GetParam().type(), Value::CreateBooleanValue(false));
  store_->OnUpdatePolicy();
  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_OK,
            store_->GetValue(GetParam().pref_name(), &value));
  ASSERT_TRUE(value);
  bool boolean_value = true;
  bool result = value->GetAsBoolean(&boolean_value);
  ASSERT_TRUE(result);
  EXPECT_FALSE(boolean_value);

  provider_.AddPolicy(GetParam().type(), Value::CreateBooleanValue(true));
  store_->OnUpdatePolicy();
  value = NULL;
  EXPECT_EQ(PrefStore::READ_OK,
            store_->GetValue(GetParam().pref_name(), &value));
  boolean_value = false;
  result = value->GetAsBoolean(&boolean_value);
  ASSERT_TRUE(result);
  EXPECT_TRUE(boolean_value);
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
                    prefs::kNetworkPredictionEnabled),
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
        TypeAndName(kPolicyRemoteAccessHostFirewallTraversal,
                    prefs::kRemoteAccessHostFirewallTraversal),
        TypeAndName(kPolicyCloudPrintProxyEnabled,
                    prefs::kCloudPrintProxyEnabled),
        TypeAndName(kPolicyCloudPrintSubmitEnabled,
                    prefs::kCloudPrintSubmitEnabled),
        TypeAndName(kPolicySavingBrowserHistoryDisabled,
                    prefs::kSavingBrowserHistoryDisabled),
        TypeAndName(kPolicySavingBrowserHistoryDisabled,
                    prefs::kSavingBrowserHistoryDisabled),
        TypeAndName(kPolicyDisableAuthNegotiateCnameLookup,
                    prefs::kDisableAuthNegotiateCnameLookup),
        TypeAndName(kPolicyEnableAuthNegotiatePort,
                    prefs::kEnableAuthNegotiatePort),
        TypeAndName(kPolicyInstantEnabled,
                    prefs::kInstantEnabled),
        TypeAndName(kPolicyDisablePluginFinder,
                    prefs::kDisablePluginFinder),
        TypeAndName(kPolicyClearSiteDataOnExit,
                    prefs::kClearSiteDataOnExit),
        TypeAndName(kPolicyDefaultBrowserSettingEnabled,
                    prefs::kDefaultBrowserSettingEnabled),
        TypeAndName(kPolicyDisable3DAPIs,
                    prefs::kDisable3DAPIs),
        TypeAndName(kPolicyTranslateEnabled,
                    prefs::kEnableTranslate),
        TypeAndName(kPolicyAllowOutdatedPlugins,
                    prefs::kPluginsAllowOutdated),
        TypeAndName(kPolicyAlwaysAuthorizePlugins,
                    prefs::kPluginsAlwaysAuthorize),
        TypeAndName(kPolicyBookmarkBarEnabled,
                    prefs::kShowBookmarkBar),
        TypeAndName(kPolicyEditBookmarksEnabled,
                    prefs::kEditBookmarksEnabled),
        TypeAndName(kPolicyAllowFileSelectionDialogs,
                    prefs::kAllowFileSelectionDialogs),
        TypeAndName(kPolicyAllowCrossOriginAuthPrompt,
                    prefs::kAllowCrossOriginAuthPrompt),
        TypeAndName(kPolicyImportBookmarks,
                    prefs::kImportBookmarks),
        TypeAndName(kPolicyImportHistory,
                    prefs::kImportHistory),
        TypeAndName(kPolicyImportHomepage,
                    prefs::kImportHomepage),
        TypeAndName(kPolicyImportSearchEngine,
                    prefs::kImportSearchEngine),
        TypeAndName(kPolicyImportSavedPasswords,
                    prefs::kImportSavedPasswords),
        TypeAndName(kPolicyDeveloperToolsDisabled,
                    prefs::kDevToolsDisabled)));

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
    : public ConfigurationPolicyPrefStoreTestBase<
                 testing::TestWithParam<TypeAndName> > {
};

TEST_P(ConfigurationPolicyPrefStoreIntegerTest, GetDefault) {
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(GetParam().pref_name(), NULL));
}

TEST_P(ConfigurationPolicyPrefStoreIntegerTest, SetValue) {
  provider_.AddPolicy(GetParam().type(), Value::CreateIntegerValue(2));
  store_->OnUpdatePolicy();
  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_OK,
            store_->GetValue(GetParam().pref_name(), &value));
  EXPECT_TRUE(base::FundamentalValue(2).Equals(value));
}

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreIntegerTestInstance,
    ConfigurationPolicyPrefStoreIntegerTest,
    testing::Values(
        TypeAndName(kPolicyDefaultCookiesSetting,
                    prefs::kManagedDefaultCookiesSetting),
        TypeAndName(kPolicyDefaultImagesSetting,
                    prefs::kManagedDefaultImagesSetting),
        TypeAndName(kPolicyDefaultPluginsSetting,
                    prefs::kManagedDefaultPluginsSetting),
        TypeAndName(kPolicyDefaultPopupsSetting,
                    prefs::kManagedDefaultPopupsSetting),
        TypeAndName(kPolicyDefaultNotificationsSetting,
                    prefs::kManagedDefaultNotificationsSetting),
        TypeAndName(kPolicyDefaultGeolocationSetting,
                    prefs::kManagedDefaultGeolocationSetting),
        TypeAndName(kPolicyRestoreOnStartup,
                    prefs::kRestoreOnStartup),
        TypeAndName(kPolicyDiskCacheSize,
                    prefs::kDiskCacheSize),
        TypeAndName(kPolicyMediaCacheSize,
                    prefs::kMediaCacheSize),
        TypeAndName(kPolicyPolicyRefreshRate,
                    prefs::kUserPolicyRefreshRate),
        TypeAndName(kPolicyMaxConnectionsPerProxy,
                    prefs::kMaxConnectionsPerProxy)));

// Test cases for the proxy policy settings.
class ConfigurationPolicyPrefStoreProxyTest : public testing::Test {
 protected:
  // Verify that all the proxy prefs are set to the specified expected values.
  static void VerifyProxyPrefs(
      const ConfigurationPolicyPrefStore& store,
      const std::string& expected_proxy_server,
      const std::string& expected_proxy_pac_url,
      const std::string& expected_proxy_bypass_list,
      const ProxyPrefs::ProxyMode& expected_proxy_mode) {
    const Value* value = NULL;
    ASSERT_EQ(PrefStore::READ_OK,
              store.GetValue(prefs::kProxy, &value));
    ASSERT_EQ(Value::TYPE_DICTIONARY, value->GetType());
    ProxyConfigDictionary dict(static_cast<const DictionaryValue*>(value));
    std::string s;
    if (expected_proxy_server.empty()) {
      EXPECT_FALSE(dict.GetProxyServer(&s));
    } else {
      ASSERT_TRUE(dict.GetProxyServer(&s));
      EXPECT_EQ(expected_proxy_server, s);
    }
    if (expected_proxy_pac_url.empty()) {
      EXPECT_FALSE(dict.GetPacUrl(&s));
    } else {
      ASSERT_TRUE(dict.GetPacUrl(&s));
      EXPECT_EQ(expected_proxy_pac_url, s);
    }
    if (expected_proxy_bypass_list.empty()) {
      EXPECT_FALSE(dict.GetBypassList(&s));
    } else {
      ASSERT_TRUE(dict.GetBypassList(&s));
      EXPECT_EQ(expected_proxy_bypass_list, s);
    }
    ProxyPrefs::ProxyMode mode;
    ASSERT_TRUE(dict.GetMode(&mode));
    EXPECT_EQ(expected_proxy_mode, mode);
  }
};

TEST_F(ConfigurationPolicyPrefStoreProxyTest, ManualOptions) {
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(kPolicyProxyBypassList,
                     Value::CreateStringValue("http://chromium.org/override"));
  provider.AddPolicy(kPolicyProxyServer,
                     Value::CreateStringValue("chromium.org"));
  provider.AddPolicy(kPolicyProxyServerMode,
                     Value::CreateIntegerValue(
                         kPolicyManuallyConfiguredProxyServerMode));

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));
  VerifyProxyPrefs(
      *store, "chromium.org", "", "http://chromium.org/override",
      ProxyPrefs::MODE_FIXED_SERVERS);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, ManualOptionsReversedApplyOrder) {
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(kPolicyProxyServerMode,
                     Value::CreateIntegerValue(
                         kPolicyManuallyConfiguredProxyServerMode));
  provider.AddPolicy(kPolicyProxyBypassList,
                     Value::CreateStringValue("http://chromium.org/override"));
  provider.AddPolicy(kPolicyProxyServer,
                     Value::CreateStringValue("chromium.org"));
  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));
  VerifyProxyPrefs(
      *store, "chromium.org", "", "http://chromium.org/override",
      ProxyPrefs::MODE_FIXED_SERVERS);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, ManualOptionsInvalid) {
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(kPolicyProxyServerMode,
                     Value::CreateIntegerValue(
                         kPolicyManuallyConfiguredProxyServerMode));

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));
  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_NO_VALUE, store->GetValue(prefs::kProxy, &value));
}


TEST_F(ConfigurationPolicyPrefStoreProxyTest, NoProxyServerMode) {
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(kPolicyProxyServerMode,
                     Value::CreateIntegerValue(kPolicyNoProxyServerMode));

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));
  VerifyProxyPrefs(*store, "", "", "", ProxyPrefs::MODE_DIRECT);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, NoProxyModeName) {
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(
      kPolicyProxyMode,
      Value::CreateStringValue(ProxyPrefs::kDirectProxyModeName));

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));
  VerifyProxyPrefs(*store, "", "", "", ProxyPrefs::MODE_DIRECT);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, AutoDetectProxyServerMode) {
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(
      kPolicyProxyServerMode,
      Value::CreateIntegerValue(kPolicyAutoDetectProxyServerMode));

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));
  VerifyProxyPrefs(*store, "", "", "", ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, AutoDetectProxyModeName) {
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(
      kPolicyProxyMode,
      Value::CreateStringValue(ProxyPrefs::kAutoDetectProxyModeName));

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));
  VerifyProxyPrefs(*store, "", "", "", ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, PacScriptProxyMode) {
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(kPolicyProxyPacUrl,
                     Value::CreateStringValue("http://short.org/proxy.pac"));
  provider.AddPolicy(
      kPolicyProxyMode,
      Value::CreateStringValue(ProxyPrefs::kPacScriptProxyModeName));

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));
  VerifyProxyPrefs(*store, "", "http://short.org/proxy.pac", "",
                   ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, PacScriptProxyModeInvalid) {
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(
      kPolicyProxyMode,
      Value::CreateStringValue(ProxyPrefs::kPacScriptProxyModeName));

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));
  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_NO_VALUE, store->GetValue(prefs::kProxy, &value));
}

// Regression test for http://crbug.com/78016, CPanel returns empty strings
// for unset properties.
TEST_F(ConfigurationPolicyPrefStoreProxyTest, PacScriptProxyModeBug78016) {
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(kPolicyProxyServer,
                     Value::CreateStringValue(""));
  provider.AddPolicy(kPolicyProxyPacUrl,
                     Value::CreateStringValue("http://short.org/proxy.pac"));
  provider.AddPolicy(
      kPolicyProxyMode,
      Value::CreateStringValue(ProxyPrefs::kPacScriptProxyModeName));

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));
  VerifyProxyPrefs(*store, "", "http://short.org/proxy.pac", "",
                   ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, UseSystemProxyServerMode) {
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(
      kPolicyProxyServerMode,
      Value::CreateIntegerValue(kPolicyUseSystemProxyServerMode));

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));
  VerifyProxyPrefs(*store, "", "", "", ProxyPrefs::MODE_SYSTEM);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, UseSystemProxyMode) {
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(
      kPolicyProxyMode,
      Value::CreateStringValue(ProxyPrefs::kSystemProxyModeName));

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));
  VerifyProxyPrefs(*store, "", "", "", ProxyPrefs::MODE_SYSTEM);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest,
       ProxyModeOverridesProxyServerMode) {
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(kPolicyProxyServerMode,
                     Value::CreateIntegerValue(kPolicyNoProxyServerMode));
  provider.AddPolicy(
      kPolicyProxyMode,
      Value::CreateStringValue(ProxyPrefs::kAutoDetectProxyModeName));

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));
  VerifyProxyPrefs(*store, "", "", "", ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, ProxyInvalid) {
  for (int i = 0; i < MODE_COUNT; ++i) {
    MockConfigurationPolicyProvider provider;
    provider.AddPolicy(kPolicyProxyServerMode, Value::CreateIntegerValue(i));
    // No mode expects all three parameters being set.
    provider.AddPolicy(kPolicyProxyPacUrl,
                       Value::CreateStringValue("http://short.org/proxy.pac"));
    provider.AddPolicy(kPolicyProxyBypassList,
                       Value::CreateStringValue(
                           "http://chromium.org/override"));
    provider.AddPolicy(kPolicyProxyServer,
                       Value::CreateStringValue("chromium.org"));

    scoped_refptr<ConfigurationPolicyPrefStore> store(
        new ConfigurationPolicyPrefStore(&provider));
    const Value* value = NULL;
    EXPECT_EQ(PrefStore::READ_NO_VALUE,
              store->GetValue(prefs::kProxy, &value));
  }
}

class ConfigurationPolicyPrefStoreDefaultSearchTest : public testing::Test {
};

// Checks that if the policy for default search is valid, i.e. there's a
// search URL, that all the elements have been given proper defaults.
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, MinimallyDefined) {
  const char* const search_url = "http://test.com/search?t={searchTerms}";
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(kPolicyDefaultSearchProviderEnabled,
                     Value::CreateBooleanValue(true));
  provider.AddPolicy(kPolicyDefaultSearchProviderSearchURL,
                     Value::CreateStringValue(search_url));

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));

  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_OK,
            store->GetValue(prefs::kDefaultSearchProviderSearchURL, &value));
  EXPECT_TRUE(StringValue(search_url).Equals(value));

  EXPECT_EQ(PrefStore::READ_OK,
            store->GetValue(prefs::kDefaultSearchProviderName, &value));
  EXPECT_TRUE(StringValue("test.com").Equals(value));

  EXPECT_EQ(PrefStore::READ_OK,
            store->GetValue(prefs::kDefaultSearchProviderKeyword, &value));
  EXPECT_TRUE(StringValue(std::string()).Equals(value));

  EXPECT_EQ(PrefStore::READ_OK,
            store->GetValue(prefs::kDefaultSearchProviderSuggestURL, &value));
  EXPECT_TRUE(StringValue(std::string()).Equals(value));

  EXPECT_EQ(PrefStore::READ_OK,
            store->GetValue(prefs::kDefaultSearchProviderIconURL, &value));
  EXPECT_TRUE(StringValue(std::string()).Equals(value));

  EXPECT_EQ(PrefStore::READ_OK,
            store->GetValue(prefs::kDefaultSearchProviderEncodings, &value));
  EXPECT_TRUE(StringValue(std::string()).Equals(value));

  EXPECT_EQ(PrefStore::READ_OK,
            store->GetValue(prefs::kDefaultSearchProviderInstantURL, &value));
  EXPECT_TRUE(StringValue(std::string()).Equals(value));
}

// Checks that for a fully defined search policy, all elements have been
// read properly.
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, FullyDefined) {
  const char* const search_url = "http://test.com/search?t={searchTerms}";
  const char* const suggest_url = "http://test.com/sugg?={searchTerms}";
  const char* const icon_url = "http://test.com/icon.jpg";
  const char* const name = "MyName";
  const char* const keyword = "MyKeyword";
  ListValue* encodings = new ListValue();
  encodings->Append(Value::CreateStringValue("UTF-16"));
  encodings->Append(Value::CreateStringValue("UTF-8"));
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(kPolicyDefaultSearchProviderEnabled,
                     Value::CreateBooleanValue(true));
  provider.AddPolicy(kPolicyDefaultSearchProviderSearchURL,
                     Value::CreateStringValue(search_url));
  provider.AddPolicy(kPolicyDefaultSearchProviderName,
                     Value::CreateStringValue(name));
  provider.AddPolicy(kPolicyDefaultSearchProviderKeyword,
                     Value::CreateStringValue(keyword));
  provider.AddPolicy(kPolicyDefaultSearchProviderSuggestURL,
                     Value::CreateStringValue(suggest_url));
  provider.AddPolicy(kPolicyDefaultSearchProviderIconURL,
                     Value::CreateStringValue(icon_url));
  provider.AddPolicy(kPolicyDefaultSearchProviderEncodings, encodings);

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));

  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_OK,
            store->GetValue(prefs::kDefaultSearchProviderSearchURL, &value));
  EXPECT_TRUE(StringValue(search_url).Equals(value));

  EXPECT_EQ(PrefStore::READ_OK,
            store->GetValue(prefs::kDefaultSearchProviderName, &value));
  EXPECT_TRUE(StringValue(name).Equals(value));

  EXPECT_EQ(PrefStore::READ_OK,
            store->GetValue(prefs::kDefaultSearchProviderKeyword, &value));
  EXPECT_TRUE(StringValue(keyword).Equals(value));

  EXPECT_EQ(PrefStore::READ_OK,
            store->GetValue(prefs::kDefaultSearchProviderSuggestURL, &value));
  EXPECT_TRUE(StringValue(suggest_url).Equals(value));

  EXPECT_EQ(PrefStore::READ_OK,
            store->GetValue(prefs::kDefaultSearchProviderIconURL, &value));
  EXPECT_TRUE(StringValue(icon_url).Equals(value));

  EXPECT_EQ(PrefStore::READ_OK,
            store->GetValue(prefs::kDefaultSearchProviderEncodings, &value));
  EXPECT_TRUE(StringValue("UTF-16;UTF-8").Equals(value));
}

// Checks that if the default search policy is missing, that no elements of the
// default search policy will be present.
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, MissingUrl) {
  const char* const suggest_url = "http://test.com/sugg?t={searchTerms}";
  const char* const icon_url = "http://test.com/icon.jpg";
  const char* const name = "MyName";
  const char* const keyword = "MyKeyword";
  ListValue* encodings = new ListValue();
  encodings->Append(Value::CreateStringValue("UTF-16"));
  encodings->Append(Value::CreateStringValue("UTF-8"));
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(kPolicyDefaultSearchProviderEnabled,
                     Value::CreateBooleanValue(true));
  provider.AddPolicy(kPolicyDefaultSearchProviderName,
                     Value::CreateStringValue(name));
  provider.AddPolicy(kPolicyDefaultSearchProviderKeyword,
                     Value::CreateStringValue(keyword));
  provider.AddPolicy(kPolicyDefaultSearchProviderSuggestURL,
                     Value::CreateStringValue(suggest_url));
  provider.AddPolicy(kPolicyDefaultSearchProviderIconURL,
                     Value::CreateStringValue(icon_url));
  provider.AddPolicy(kPolicyDefaultSearchProviderEncodings, encodings);

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));

  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store->GetValue(prefs::kDefaultSearchProviderSearchURL, NULL));
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store->GetValue(prefs::kDefaultSearchProviderName, NULL));
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store->GetValue(prefs::kDefaultSearchProviderKeyword, NULL));
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store->GetValue(prefs::kDefaultSearchProviderSuggestURL, NULL));
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store->GetValue(prefs::kDefaultSearchProviderIconURL, NULL));
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store->GetValue(prefs::kDefaultSearchProviderEncodings, NULL));
}

// Checks that if the default search policy is invalid, that no elements of the
// default search policy will be present.
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, Invalid) {
  const char* const bad_search_url = "http://test.com/noSearchTerms";
  const char* const suggest_url = "http://test.com/sugg?t={searchTerms}";
  const char* const icon_url = "http://test.com/icon.jpg";
  const char* const name = "MyName";
  const char* const keyword = "MyKeyword";
  ListValue* encodings = new ListValue();
  encodings->Append(Value::CreateStringValue("UTF-16"));
  encodings->Append(Value::CreateStringValue("UTF-8"));
  MockConfigurationPolicyProvider provider;
  provider.AddPolicy(kPolicyDefaultSearchProviderEnabled,
                     Value::CreateBooleanValue(true));
  provider.AddPolicy(kPolicyDefaultSearchProviderSearchURL,
                     Value::CreateStringValue(bad_search_url));
  provider.AddPolicy(kPolicyDefaultSearchProviderName,
                     Value::CreateStringValue(name));
  provider.AddPolicy(kPolicyDefaultSearchProviderKeyword,
                     Value::CreateStringValue(keyword));
  provider.AddPolicy(kPolicyDefaultSearchProviderSuggestURL,
                     Value::CreateStringValue(suggest_url));
  provider.AddPolicy(kPolicyDefaultSearchProviderIconURL,
                     Value::CreateStringValue(icon_url));
  provider.AddPolicy(kPolicyDefaultSearchProviderEncodings, encodings);

  scoped_refptr<ConfigurationPolicyPrefStore> store(
      new ConfigurationPolicyPrefStore(&provider));

  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store->GetValue(prefs::kDefaultSearchProviderSearchURL, NULL));
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store->GetValue(prefs::kDefaultSearchProviderName, NULL));
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store->GetValue(prefs::kDefaultSearchProviderKeyword, NULL));
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store->GetValue(prefs::kDefaultSearchProviderSuggestURL, NULL));
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store->GetValue(prefs::kDefaultSearchProviderIconURL, NULL));
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store->GetValue(prefs::kDefaultSearchProviderEncodings, NULL));
}

// Tests Incognito mode availability preference setting.
class ConfigurationPolicyPrefStoreIncognitoModeTest : public testing::Test {
 protected:
  static const int kIncognitoModeAvailabilityNotSet = -1;

  enum ObsoleteIncognitoEnabledValue {
    INCOGNITO_ENABLED_UNKNOWN,
    INCOGNITO_ENABLED_TRUE,
    INCOGNITO_ENABLED_FALSE
  };

  void SetPolicies(ObsoleteIncognitoEnabledValue incognito_enabled,
                   int availability) {
    if (incognito_enabled != INCOGNITO_ENABLED_UNKNOWN) {
      provider_.AddPolicy(kPolicyIncognitoEnabled,
                          Value::CreateBooleanValue(
                              incognito_enabled == INCOGNITO_ENABLED_TRUE));
    }
    if (availability >= 0) {
      provider_.AddPolicy(kPolicyIncognitoModeAvailability,
                          Value::CreateIntegerValue(availability));
    }
    store_ = new ConfigurationPolicyPrefStore(&provider_);
  }

  void VerifyValues(IncognitoModePrefs::Availability availability) {
    const Value* value = NULL;
    EXPECT_EQ(PrefStore::READ_OK,
              store_->GetValue(prefs::kIncognitoModeAvailability, &value));
    EXPECT_TRUE(base::FundamentalValue(availability).Equals(value));
  }

  MockConfigurationPolicyProvider provider_;
  scoped_refptr<ConfigurationPolicyPrefStore> store_;
};

// The following testcases verify that if the obsolete IncognitoEnabled
// policy is not set, the IncognitoModeAvailability values should be copied
// from IncognitoModeAvailability policy to pref "as is".
TEST_F(ConfigurationPolicyPrefStoreIncognitoModeTest,
       NoObsoletePolicyAndIncognitoEnabled) {
  SetPolicies(INCOGNITO_ENABLED_UNKNOWN, IncognitoModePrefs::ENABLED);
  VerifyValues(IncognitoModePrefs::ENABLED);
}

TEST_F(ConfigurationPolicyPrefStoreIncognitoModeTest,
       NoObsoletePolicyAndIncognitoDisabled) {
  SetPolicies(INCOGNITO_ENABLED_UNKNOWN, IncognitoModePrefs::DISABLED);
  VerifyValues(IncognitoModePrefs::DISABLED);
}

TEST_F(ConfigurationPolicyPrefStoreIncognitoModeTest,
       NoObsoletePolicyAndIncognitoForced) {
  SetPolicies(INCOGNITO_ENABLED_UNKNOWN, IncognitoModePrefs::FORCED);
  VerifyValues(IncognitoModePrefs::FORCED);
}

TEST_F(ConfigurationPolicyPrefStoreIncognitoModeTest,
       NoObsoletePolicyAndNoIncognitoAvailability) {
  SetPolicies(INCOGNITO_ENABLED_UNKNOWN, kIncognitoModeAvailabilityNotSet);
  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kIncognitoModeAvailability, &value));
}

// Checks that if the obsolete IncognitoEnabled policy is set, if sets
// the IncognitoModeAvailability preference only in case
// the IncognitoModeAvailability policy is not specified.
TEST_F(ConfigurationPolicyPrefStoreIncognitoModeTest,
       ObsoletePolicyDoesNotAffectAvailabilityEnabled) {
  SetPolicies(INCOGNITO_ENABLED_FALSE, IncognitoModePrefs::ENABLED);
  VerifyValues(IncognitoModePrefs::ENABLED);
}

TEST_F(ConfigurationPolicyPrefStoreIncognitoModeTest,
       ObsoletePolicyDoesNotAffectAvailabilityForced) {
  SetPolicies(INCOGNITO_ENABLED_TRUE, IncognitoModePrefs::FORCED);
  VerifyValues(IncognitoModePrefs::FORCED);
}

TEST_F(ConfigurationPolicyPrefStoreIncognitoModeTest,
       ObsoletePolicySetsPreferenceToEnabled) {
  SetPolicies(INCOGNITO_ENABLED_TRUE, kIncognitoModeAvailabilityNotSet);
  VerifyValues(IncognitoModePrefs::ENABLED);
}

TEST_F(ConfigurationPolicyPrefStoreIncognitoModeTest,
       ObsoletePolicySetsPreferenceToDisabled) {
  SetPolicies(INCOGNITO_ENABLED_FALSE, kIncognitoModeAvailabilityNotSet);
  VerifyValues(IncognitoModePrefs::DISABLED);
}

// Test cases for the Sync policy setting.
class ConfigurationPolicyPrefStoreSyncTest
    : public ConfigurationPolicyPrefStoreTestBase<testing::Test> {
};

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Default) {
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kSyncManaged, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Enabled) {
  provider_.AddPolicy(kPolicySyncDisabled, Value::CreateBooleanValue(false));
  store_->OnUpdatePolicy();
  // Enabling Sync should not set the pref.
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kSyncManaged, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Disabled) {
  provider_.AddPolicy(kPolicySyncDisabled, Value::CreateBooleanValue(true));
  store_->OnUpdatePolicy();
  // Sync should be flagged as managed.
  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_OK, store_->GetValue(prefs::kSyncManaged, &value));
  ASSERT_TRUE(value);
  bool sync_managed = false;
  bool result = value->GetAsBoolean(&sync_managed);
  ASSERT_TRUE(result);
  EXPECT_TRUE(sync_managed);
}

// Test cases for how the DownloadDirectory and AllowFileSelectionDialogs policy
// influence the PromptForDownload preference.
class ConfigurationPolicyPrefStorePromptDownloadTest
    : public ConfigurationPolicyPrefStoreTestBase<testing::Test> {
};

TEST_F(ConfigurationPolicyPrefStorePromptDownloadTest, Default) {
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kPromptForDownload, NULL));
}

#if !defined(OS_CHROMEOS)
TEST_F(ConfigurationPolicyPrefStorePromptDownloadTest, SetDownloadDirectory) {
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kPromptForDownload, NULL));
  provider_.AddPolicy(kPolicyDownloadDirectory, Value::CreateStringValue(""));
  store_->OnUpdatePolicy();

  // Setting a DownloadDirectory should disable the PromptForDownload pref.
  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_OK, store_->GetValue(prefs::kPromptForDownload,
                                                 &value));
  ASSERT_TRUE(value);
  bool prompt_for_download = true;
  bool result = value->GetAsBoolean(&prompt_for_download);
  ASSERT_TRUE(result);
  EXPECT_FALSE(prompt_for_download);
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(ConfigurationPolicyPrefStorePromptDownloadTest,
       EnableFileSelectionDialogs) {
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kPromptForDownload, NULL));
  provider_.AddPolicy(kPolicyAllowFileSelectionDialogs,
                      Value::CreateBooleanValue(true));
  store_->OnUpdatePolicy();

  // Allowing file-selection dialogs should not influence the PromptForDownload
  // pref.
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kPromptForDownload, NULL));
}

TEST_F(ConfigurationPolicyPrefStorePromptDownloadTest,
       DisableFileSelectionDialogs) {
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kPromptForDownload, NULL));
  provider_.AddPolicy(kPolicyAllowFileSelectionDialogs,
                      Value::CreateBooleanValue(false));
  store_->OnUpdatePolicy();

  // Disabling file-selection dialogs should disable the PromptForDownload pref.
  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_OK, store_->GetValue(prefs::kPromptForDownload,
                                                 &value));
  ASSERT_TRUE(value);
  bool prompt_for_download = true;
  bool result = value->GetAsBoolean(&prompt_for_download);
  ASSERT_TRUE(result);
  EXPECT_FALSE(prompt_for_download);
}

// Test cases for the Autofill policy setting.
class ConfigurationPolicyPrefStoreAutofillTest
    : public ConfigurationPolicyPrefStoreTestBase<testing::Test> {
};

TEST_F(ConfigurationPolicyPrefStoreAutofillTest, Default) {
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kAutofillEnabled, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreAutofillTest, Enabled) {
  provider_.AddPolicy(kPolicyAutoFillEnabled, Value::CreateBooleanValue(true));
  store_->OnUpdatePolicy();
  // Enabling Autofill should not set the pref.
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kAutofillEnabled, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreAutofillTest, Disabled) {
  provider_.AddPolicy(kPolicyAutoFillEnabled, Value::CreateBooleanValue(false));
  store_->OnUpdatePolicy();
  // Disabling Autofill should switch the pref to managed.
  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_OK,
            store_->GetValue(prefs::kAutofillEnabled, &value));
  ASSERT_TRUE(value);
  bool autofill_enabled = true;
  bool result = value->GetAsBoolean(&autofill_enabled);
  ASSERT_TRUE(result);
  EXPECT_FALSE(autofill_enabled);
}

// Exercises the policy refresh mechanism.
class ConfigurationPolicyPrefStoreRefreshTest
    : public ConfigurationPolicyPrefStoreTestBase<testing::Test> {
 protected:
  virtual void SetUp() {
    store_->AddObserver(&observer_);
  }

  virtual void TearDown() {
    store_->RemoveObserver(&observer_);
  }

  PrefStoreObserverMock observer_;
};

TEST_F(ConfigurationPolicyPrefStoreRefreshTest, Refresh) {
  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kHomePage, NULL));

  EXPECT_CALL(observer_, OnPrefValueChanged(prefs::kHomePage)).Times(1);
  provider_.AddPolicy(kPolicyHomepageLocation,
                      Value::CreateStringValue("http://www.chromium.org"));
  store_->OnUpdatePolicy();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_EQ(PrefStore::READ_OK,
            store_->GetValue(prefs::kHomePage, &value));
  EXPECT_TRUE(StringValue("http://www.chromium.org").Equals(value));

  EXPECT_CALL(observer_, OnPrefValueChanged(_)).Times(0);
  store_->OnUpdatePolicy();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnPrefValueChanged(prefs::kHomePage)).Times(1);
  provider_.RemovePolicy(kPolicyHomepageLocation);
  store_->OnUpdatePolicy();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kHomePage, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreRefreshTest, Initialization) {
  EXPECT_FALSE(store_->IsInitializationComplete());

  EXPECT_CALL(observer_, OnInitializationCompleted(true)).Times(1);

  provider_.SetInitializationComplete(true);
  EXPECT_FALSE(store_->IsInitializationComplete());

  store_->OnUpdatePolicy();
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(store_->IsInitializationComplete());
}

// Tests for policies that don't quite fit the previous patterns.
class ConfigurationPolicyPrefStoreOthersTest
    : public ConfigurationPolicyPrefStoreTestBase<testing::Test> {
};

TEST_F(ConfigurationPolicyPrefStoreOthersTest, JavascriptEnabled) {
  // This is a boolean policy, but affects an integer preference.
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, NULL));
  provider_.AddPolicy(kPolicyJavascriptEnabled,
                      Value::CreateBooleanValue(true));
  store_->OnUpdatePolicy();
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, NULL));
  provider_.AddPolicy(kPolicyJavascriptEnabled,
                      Value::CreateBooleanValue(false));
  store_->OnUpdatePolicy();
  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_OK,
            store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, &value));
  EXPECT_TRUE(base::FundamentalValue(CONTENT_SETTING_BLOCK).Equals(value));
}

TEST_F(ConfigurationPolicyPrefStoreOthersTest, JavascriptEnabledOverridden) {
  EXPECT_EQ(PrefStore::READ_NO_VALUE,
            store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, NULL));
  provider_.AddPolicy(kPolicyJavascriptEnabled,
                      Value::CreateBooleanValue(false));
  store_->OnUpdatePolicy();
  const Value* value = NULL;
  EXPECT_EQ(PrefStore::READ_OK,
            store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, &value));
  EXPECT_TRUE(base::FundamentalValue(CONTENT_SETTING_BLOCK).Equals(value));
  // DefaultJavaScriptSetting overrides JavascriptEnabled.
  provider_.AddPolicy(kPolicyDefaultJavaScriptSetting,
                      Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
  store_->OnUpdatePolicy();
  EXPECT_EQ(PrefStore::READ_OK,
            store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, &value));
  EXPECT_TRUE(base::FundamentalValue(CONTENT_SETTING_ALLOW).Equals(value));
}

}  // namespace policy
