// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_store_observer_mock.h"
#include "chrome/browser/policy/configuration_policy_handler.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service_impl.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::Return;
using testing::_;

namespace policy {

// Holds a set of test parameters, consisting of pref name and policy name.
class PolicyAndPref {
 public:
  PolicyAndPref(const char* policy_name, const char* pref_name)
      : policy_name_(policy_name),
        pref_name_(pref_name) {}

  const char* policy_name() const { return policy_name_; }
  const char* pref_name() const { return pref_name_; }

 private:
  const char* policy_name_;
  const char* pref_name_;
};

class ConfigurationPolicyPrefStoreTest : public testing::Test {
 protected:
  ConfigurationPolicyPrefStoreTest() {
    EXPECT_CALL(provider_, IsInitializationComplete())
        .WillRepeatedly(Return(false));
    provider_.Init();
    PolicyServiceImpl::Providers providers;
    providers.push_back(&provider_);
    policy_service_.reset(new PolicyServiceImpl(providers));
    store_ = new ConfigurationPolicyPrefStore(policy_service_.get(),
                                              POLICY_LEVEL_MANDATORY);
  }

  virtual void TearDown() OVERRIDE {
    provider_.Shutdown();
  }

  MockConfigurationPolicyProvider provider_;
  scoped_ptr<PolicyServiceImpl> policy_service_;
  scoped_refptr<ConfigurationPolicyPrefStore> store_;
};

// Test cases for list-valued policy settings.
class ConfigurationPolicyPrefStoreListTest
    : public ConfigurationPolicyPrefStoreTest,
      public testing::WithParamInterface<PolicyAndPref> {};

TEST_P(ConfigurationPolicyPrefStoreListTest, GetDefault) {
  EXPECT_FALSE(store_->GetValue(GetParam().pref_name(), NULL));
}

TEST_P(ConfigurationPolicyPrefStoreListTest, SetValue) {
  base::ListValue* in_value = new base::ListValue();
  in_value->Append(base::Value::CreateStringValue("test1"));
  in_value->Append(base::Value::CreateStringValue("test2,"));
  PolicyMap policy;
  policy.Set(GetParam().policy_name(), POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, in_value);
  provider_.UpdateChromePolicy(policy);
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(GetParam().pref_name(), &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(in_value->Equals(value));
}

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreListTestInstance,
    ConfigurationPolicyPrefStoreListTest,
    testing::Values(
        PolicyAndPref(key::kRestoreOnStartupURLs,
                      prefs::kURLsToRestoreOnStartup),
        PolicyAndPref(key::kDisabledPlugins,
                      prefs::kPluginsDisabledPlugins),
        PolicyAndPref(key::kDisabledPluginsExceptions,
                      prefs::kPluginsDisabledPluginsExceptions),
        PolicyAndPref(key::kEnabledPlugins,
                      prefs::kPluginsEnabledPlugins),
        PolicyAndPref(key::kDisabledSchemes,
                      prefs::kDisabledSchemes),
        PolicyAndPref(key::kAutoSelectCertificateForUrls,
                      prefs::kManagedAutoSelectCertificateForUrls),
        PolicyAndPref(key::kURLBlacklist,
                      prefs::kUrlBlacklist),
        PolicyAndPref(key::kURLWhitelist,
                      prefs::kUrlWhitelist)));

// Test cases for string-valued policy settings.
class ConfigurationPolicyPrefStoreStringTest
    : public ConfigurationPolicyPrefStoreTest,
      public testing::WithParamInterface<PolicyAndPref> {};

TEST_P(ConfigurationPolicyPrefStoreStringTest, GetDefault) {
  EXPECT_FALSE(store_->GetValue(GetParam().pref_name(), NULL));
}

TEST_P(ConfigurationPolicyPrefStoreStringTest, SetValue) {
  PolicyMap policy;
  policy.Set(GetParam().policy_name(), POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             base::Value::CreateStringValue("http://chromium.org"));
  provider_.UpdateChromePolicy(policy);
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(GetParam().pref_name(), &value));
  ASSERT_TRUE(value);
  EXPECT_TRUE(base::StringValue("http://chromium.org").Equals(value));
}

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreStringTestInstance,
    ConfigurationPolicyPrefStoreStringTest,
    testing::Values(
        PolicyAndPref(key::kRestrictSigninToPattern,
                      prefs::kGoogleServicesUsernamePattern),
        PolicyAndPref(key::kHomepageLocation,
                      prefs::kHomePage),
        PolicyAndPref(key::kApplicationLocaleValue,
                      prefs::kApplicationLocale),
        PolicyAndPref(key::kAuthSchemes,
                      prefs::kAuthSchemes),
        PolicyAndPref(key::kAuthServerWhitelist,
                      prefs::kAuthServerWhitelist),
        PolicyAndPref(key::kAuthNegotiateDelegateWhitelist,
                      prefs::kAuthNegotiateDelegateWhitelist),
        PolicyAndPref(key::kGSSAPILibraryName,
                      prefs::kGSSAPILibraryName),
        PolicyAndPref(key::kDiskCacheDir,
                      prefs::kDiskCacheDir)));

#if !defined(OS_CHROMEOS)
INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreDownloadDirectoryInstance,
    ConfigurationPolicyPrefStoreStringTest,
    testing::Values(PolicyAndPref(key::kDownloadDirectory,
                                  prefs::kDownloadDefaultDirectory)));
#endif  // !defined(OS_CHROMEOS)

// Test cases for boolean-valued policy settings.
class ConfigurationPolicyPrefStoreBooleanTest
    : public ConfigurationPolicyPrefStoreTest,
      public testing::WithParamInterface<PolicyAndPref> {};

TEST_P(ConfigurationPolicyPrefStoreBooleanTest, GetDefault) {
  EXPECT_FALSE(store_->GetValue(GetParam().pref_name(), NULL));
}

TEST_P(ConfigurationPolicyPrefStoreBooleanTest, SetValue) {
  PolicyMap policy;
  policy.Set(GetParam().policy_name(), POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policy);
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(GetParam().pref_name(), &value));
  ASSERT_TRUE(value);
  bool boolean_value = true;
  bool result = value->GetAsBoolean(&boolean_value);
  ASSERT_TRUE(result);
  EXPECT_FALSE(boolean_value);

  policy.Set(GetParam().policy_name(), POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policy);
  value = NULL;
  EXPECT_TRUE(store_->GetValue(GetParam().pref_name(), &value));
  boolean_value = false;
  result = value->GetAsBoolean(&boolean_value);
  ASSERT_TRUE(result);
  EXPECT_TRUE(boolean_value);
}

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreBooleanTestInstance,
    ConfigurationPolicyPrefStoreBooleanTest,
    testing::Values(
        PolicyAndPref(key::kHomepageIsNewTabPage,
                      prefs::kHomePageIsNewTabPage),
        PolicyAndPref(key::kAlternateErrorPagesEnabled,
                      prefs::kAlternateErrorPagesEnabled),
        PolicyAndPref(key::kSearchSuggestEnabled,
                      prefs::kSearchSuggestEnabled),
        PolicyAndPref(key::kDnsPrefetchingEnabled,
                      prefs::kNetworkPredictionEnabled),
        PolicyAndPref(key::kBuiltInDnsClientEnabled,
                      prefs::kBuiltInDnsClientEnabled),
        PolicyAndPref(key::kDisableSpdy,
                      prefs::kDisableSpdy),
        PolicyAndPref(key::kSafeBrowsingEnabled,
                      prefs::kSafeBrowsingEnabled),
        PolicyAndPref(key::kForceSafeSearch,
                      prefs::kForceSafeSearch),
        PolicyAndPref(key::kMetricsReportingEnabled,
                      prefs::kMetricsReportingEnabled),
        PolicyAndPref(key::kPasswordManagerEnabled,
                      prefs::kPasswordManagerEnabled),
        PolicyAndPref(key::kPasswordManagerAllowShowPasswords,
                      prefs::kPasswordManagerAllowShowPasswords),
        PolicyAndPref(key::kShowHomeButton,
                      prefs::kShowHomeButton),
        PolicyAndPref(key::kPrintingEnabled,
                      prefs::kPrintingEnabled),
        PolicyAndPref(key::kRemoteAccessHostFirewallTraversal,
                      prefs::kRemoteAccessHostFirewallTraversal),
        PolicyAndPref(key::kCloudPrintProxyEnabled,
                      prefs::kCloudPrintProxyEnabled),
        PolicyAndPref(key::kCloudPrintSubmitEnabled,
                      prefs::kCloudPrintSubmitEnabled),
        PolicyAndPref(key::kSavingBrowserHistoryDisabled,
                      prefs::kSavingBrowserHistoryDisabled),
        PolicyAndPref(key::kEnableOriginBoundCerts,
                      prefs::kEnableOriginBoundCerts),
        PolicyAndPref(key::kDisableSSLRecordSplitting,
                      prefs::kDisableSSLRecordSplitting),
        PolicyAndPref(key::kEnableOnlineRevocationChecks,
                      prefs::kCertRevocationCheckingEnabled),
        PolicyAndPref(key::kDisableAuthNegotiateCnameLookup,
                      prefs::kDisableAuthNegotiateCnameLookup),
        PolicyAndPref(key::kEnableAuthNegotiatePort,
                      prefs::kEnableAuthNegotiatePort),
        PolicyAndPref(key::kInstantEnabled,
                      prefs::kInstantEnabled),
        PolicyAndPref(key::kDisablePluginFinder,
                      prefs::kDisablePluginFinder),
        PolicyAndPref(key::kDefaultBrowserSettingEnabled,
                      prefs::kDefaultBrowserSettingEnabled),
        PolicyAndPref(key::kDisable3DAPIs,
                      prefs::kDisable3DAPIs),
        PolicyAndPref(key::kTranslateEnabled,
                      prefs::kEnableTranslate),
        PolicyAndPref(key::kAllowOutdatedPlugins,
                      prefs::kPluginsAllowOutdated),
        PolicyAndPref(key::kAlwaysAuthorizePlugins,
                      prefs::kPluginsAlwaysAuthorize),
        PolicyAndPref(key::kBookmarkBarEnabled,
                      prefs::kShowBookmarkBar),
        PolicyAndPref(key::kEditBookmarksEnabled,
                      prefs::kEditBookmarksEnabled),
        PolicyAndPref(key::kAllowFileSelectionDialogs,
                      prefs::kAllowFileSelectionDialogs),
        PolicyAndPref(key::kAllowCrossOriginAuthPrompt,
                      prefs::kAllowCrossOriginAuthPrompt),
        PolicyAndPref(key::kImportBookmarks,
                      prefs::kImportBookmarks),
        PolicyAndPref(key::kImportHistory,
                      prefs::kImportHistory),
        PolicyAndPref(key::kImportHomepage,
                      prefs::kImportHomepage),
        PolicyAndPref(key::kImportSearchEngine,
                      prefs::kImportSearchEngine),
        PolicyAndPref(key::kImportSavedPasswords,
                      prefs::kImportSavedPasswords),
        PolicyAndPref(key::kEnableMemoryInfo,
                      prefs::kEnableMemoryInfo),
        PolicyAndPref(key::kDisablePrintPreview,
                      prefs::kPrintPreviewDisabled),
        PolicyAndPref(key::kDeveloperToolsDisabled,
                      prefs::kDevToolsDisabled)));

#if defined(OS_CHROMEOS)
INSTANTIATE_TEST_CASE_P(
    CrosConfigurationPolicyPrefStoreBooleanTestInstance,
    ConfigurationPolicyPrefStoreBooleanTest,
    testing::Values(
        PolicyAndPref(key::kChromeOsLockOnIdleSuspend,
                      prefs::kEnableScreenLock),
        PolicyAndPref(key::kDriveDisabled,
                      prefs::kDisableDrive),
        PolicyAndPref(key::kDriveDisabledOverCellular,
                      prefs::kDisableDriveOverCellular),
        PolicyAndPref(key::kExternalStorageDisabled,
                      prefs::kExternalStorageDisabled),
        PolicyAndPref(key::kAudioOutputAllowed,
                      prefs::kAudioOutputAllowed),
        PolicyAndPref(key::kAudioCaptureAllowed,
                      prefs::kAudioCaptureAllowed)));
#endif  // defined(OS_CHROMEOS)

// Test cases for integer-valued policy settings.
class ConfigurationPolicyPrefStoreIntegerTest
    : public ConfigurationPolicyPrefStoreTest,
      public testing::WithParamInterface<PolicyAndPref> {};

TEST_P(ConfigurationPolicyPrefStoreIntegerTest, GetDefault) {
  EXPECT_FALSE(store_->GetValue(GetParam().pref_name(), NULL));
}

TEST_P(ConfigurationPolicyPrefStoreIntegerTest, SetValue) {
  PolicyMap policy;
  policy.Set(GetParam().policy_name(), POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateIntegerValue(2));
  provider_.UpdateChromePolicy(policy);
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(GetParam().pref_name(), &value));
  EXPECT_TRUE(base::FundamentalValue(2).Equals(value));
}

INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreIntegerTestInstance,
    ConfigurationPolicyPrefStoreIntegerTest,
    testing::Values(
        PolicyAndPref(key::kDefaultCookiesSetting,
                      prefs::kManagedDefaultCookiesSetting),
        PolicyAndPref(key::kDefaultImagesSetting,
                      prefs::kManagedDefaultImagesSetting),
        PolicyAndPref(key::kDefaultPluginsSetting,
                      prefs::kManagedDefaultPluginsSetting),
        PolicyAndPref(key::kDefaultPopupsSetting,
                      prefs::kManagedDefaultPopupsSetting),
        PolicyAndPref(key::kDefaultNotificationsSetting,
                      prefs::kManagedDefaultNotificationsSetting),
        PolicyAndPref(key::kDefaultGeolocationSetting,
                      prefs::kManagedDefaultGeolocationSetting),
        PolicyAndPref(key::kRestoreOnStartup,
                      prefs::kRestoreOnStartup),
        PolicyAndPref(key::kDiskCacheSize,
                      prefs::kDiskCacheSize),
        PolicyAndPref(key::kMediaCacheSize,
                      prefs::kMediaCacheSize),
        PolicyAndPref(key::kPolicyRefreshRate,
                      prefs::kUserPolicyRefreshRate),
        PolicyAndPref(key::kMaxConnectionsPerProxy,
                      prefs::kMaxConnectionsPerProxy)));

// Test cases for the proxy policy settings.
class ConfigurationPolicyPrefStoreProxyTest
    : public ConfigurationPolicyPrefStoreTest {
 protected:
  // Verify that all the proxy prefs are set to the specified expected values.
  void VerifyProxyPrefs(
      const std::string& expected_proxy_server,
      const std::string& expected_proxy_pac_url,
      const std::string& expected_proxy_bypass_list,
      const ProxyPrefs::ProxyMode& expected_proxy_mode) {
    const base::Value* value = NULL;
    ASSERT_TRUE(store_->GetValue(prefs::kProxy, &value));
    ASSERT_EQ(base::Value::TYPE_DICTIONARY, value->GetType());
    ProxyConfigDictionary dict(
        static_cast<const base::DictionaryValue*>(value));
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
  PolicyMap policy;
  policy.Set(key::kProxyBypassList, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue("http://chromium.org/override"));
  policy.Set(key::kProxyServer, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue("chromium.org"));
  policy.Set(
      key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      base::Value::CreateIntegerValue(
          ProxyPolicyHandler::PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE));
  provider_.UpdateChromePolicy(policy);

  VerifyProxyPrefs(
      "chromium.org", "", "http://chromium.org/override",
      ProxyPrefs::MODE_FIXED_SERVERS);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, ManualOptionsReversedApplyOrder) {
  PolicyMap policy;
  policy.Set(
      key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      base::Value::CreateIntegerValue(
          ProxyPolicyHandler::PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE));
  policy.Set(key::kProxyBypassList, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue("http://chromium.org/override"));
  policy.Set(key::kProxyServer, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue("chromium.org"));
  provider_.UpdateChromePolicy(policy);

  VerifyProxyPrefs(
      "chromium.org", "", "http://chromium.org/override",
      ProxyPrefs::MODE_FIXED_SERVERS);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, ManualOptionsInvalid) {
  PolicyMap policy;
  policy.Set(
      key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      base::Value::CreateIntegerValue(
          ProxyPolicyHandler::PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE));
  provider_.UpdateChromePolicy(policy);

  const base::Value* value = NULL;
  EXPECT_FALSE(store_->GetValue(prefs::kProxy, &value));
}


TEST_F(ConfigurationPolicyPrefStoreProxyTest, NoProxyServerMode) {
  PolicyMap policy;
  policy.Set(key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateIntegerValue(
                 ProxyPolicyHandler::PROXY_SERVER_MODE));
  provider_.UpdateChromePolicy(policy);
  VerifyProxyPrefs("", "", "", ProxyPrefs::MODE_DIRECT);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, NoProxyModeName) {
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue(ProxyPrefs::kDirectProxyModeName));
  provider_.UpdateChromePolicy(policy);
  VerifyProxyPrefs("", "", "", ProxyPrefs::MODE_DIRECT);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, AutoDetectProxyServerMode) {
  PolicyMap policy;
  policy.Set(
      key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      base::Value::CreateIntegerValue(
          ProxyPolicyHandler::PROXY_AUTO_DETECT_PROXY_SERVER_MODE));
  provider_.UpdateChromePolicy(policy);
  VerifyProxyPrefs("", "", "", ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, AutoDetectProxyModeName) {
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue(
                 ProxyPrefs::kAutoDetectProxyModeName));
  provider_.UpdateChromePolicy(policy);
  VerifyProxyPrefs("", "", "", ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, PacScriptProxyMode) {
  PolicyMap policy;
  policy.Set(key::kProxyPacUrl, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue("http://short.org/proxy.pac"));
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue(
                 ProxyPrefs::kPacScriptProxyModeName));
  provider_.UpdateChromePolicy(policy);
  VerifyProxyPrefs("", "http://short.org/proxy.pac", "",
                   ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, PacScriptProxyModeInvalid) {
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue(
                 ProxyPrefs::kPacScriptProxyModeName));
  provider_.UpdateChromePolicy(policy);
  const base::Value* value = NULL;
  EXPECT_FALSE(store_->GetValue(prefs::kProxy, &value));
}

// Regression test for http://crbug.com/78016, CPanel returns empty strings
// for unset properties.
TEST_F(ConfigurationPolicyPrefStoreProxyTest, PacScriptProxyModeBug78016) {
  PolicyMap policy;
  policy.Set(key::kProxyServer, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue(""));
  policy.Set(key::kProxyPacUrl, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue("http://short.org/proxy.pac"));
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue(
                 ProxyPrefs::kPacScriptProxyModeName));
  provider_.UpdateChromePolicy(policy);
  VerifyProxyPrefs("", "http://short.org/proxy.pac", "",
                   ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, UseSystemProxyServerMode) {
  PolicyMap policy;
  policy.Set(key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      base::Value::CreateIntegerValue(
          ProxyPolicyHandler::PROXY_USE_SYSTEM_PROXY_SERVER_MODE));
  provider_.UpdateChromePolicy(policy);
  VerifyProxyPrefs("", "", "", ProxyPrefs::MODE_SYSTEM);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, UseSystemProxyMode) {
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue(ProxyPrefs::kSystemProxyModeName));
  provider_.UpdateChromePolicy(policy);
  VerifyProxyPrefs("", "", "", ProxyPrefs::MODE_SYSTEM);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest,
       ProxyModeOverridesProxyServerMode) {
  PolicyMap policy;
  policy.Set(key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateIntegerValue(
                 ProxyPolicyHandler::PROXY_SERVER_MODE));
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue(
                 ProxyPrefs::kAutoDetectProxyModeName));
  provider_.UpdateChromePolicy(policy);
  VerifyProxyPrefs("", "", "", ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ConfigurationPolicyPrefStoreProxyTest, ProxyInvalid) {
  // No mode expects all three parameters being set.
  PolicyMap policy;
  policy.Set(key::kProxyPacUrl, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue("http://short.org/proxy.pac"));
  policy.Set(key::kProxyBypassList, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue("http://chromium.org/override"));
  policy.Set(key::kProxyServer, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue("chromium.org"));
  for (int i = 0; i < ProxyPolicyHandler::MODE_COUNT; ++i) {
    policy.Set(key::kProxyServerMode, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateIntegerValue(i));
    provider_.UpdateChromePolicy(policy);
    const base::Value* value = NULL;
    EXPECT_FALSE(store_->GetValue(prefs::kProxy, &value));
  }
}

class ConfigurationPolicyPrefStoreDefaultSearchTest
    : public ConfigurationPolicyPrefStoreTest {
 public:
  ConfigurationPolicyPrefStoreDefaultSearchTest() {
    default_alternate_urls_.AppendString(
        "http://www.google.com/#q={searchTerms}");
    default_alternate_urls_.AppendString(
        "http://www.google.com/search#q={searchTerms}");
  }

 protected:
  static const char* const kSearchURL;
  static const char* const kSuggestURL;
  static const char* const kIconURL;
  static const char* const kName;
  static const char* const kKeyword;
  static const char* const kReplacementKey;

  // Build a default search policy by setting search-related keys in |policy| to
  // reasonable values. You can update any of the keys after calling this
  // method.
  void BuildDefaultSearchPolicy(PolicyMap* policy);

  base::ListValue default_alternate_urls_;
};

const char* const ConfigurationPolicyPrefStoreDefaultSearchTest::kSearchURL =
    "http://test.com/search?t={searchTerms}";
const char* const ConfigurationPolicyPrefStoreDefaultSearchTest::kSuggestURL =
    "http://test.com/sugg?={searchTerms}";
const char* const ConfigurationPolicyPrefStoreDefaultSearchTest::kIconURL =
    "http://test.com/icon.jpg";
const char* const ConfigurationPolicyPrefStoreDefaultSearchTest::kName =
    "MyName";
const char* const ConfigurationPolicyPrefStoreDefaultSearchTest::kKeyword =
    "MyKeyword";
const char* const
    ConfigurationPolicyPrefStoreDefaultSearchTest::kReplacementKey = "espv";

void ConfigurationPolicyPrefStoreDefaultSearchTest::
    BuildDefaultSearchPolicy(PolicyMap* policy) {
  base::ListValue* encodings = new base::ListValue();
  encodings->AppendString("UTF-16");
  encodings->AppendString("UTF-8");
  policy->Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  policy->Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, base::Value::CreateStringValue(kSearchURL));
  policy->Set(key::kDefaultSearchProviderName, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, base::Value::CreateStringValue(kName));
  policy->Set(key::kDefaultSearchProviderKeyword, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, base::Value::CreateStringValue(kKeyword));
  policy->Set(key::kDefaultSearchProviderSuggestURL, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, base::Value::CreateStringValue(kSuggestURL));
  policy->Set(key::kDefaultSearchProviderIconURL, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, base::Value::CreateStringValue(kIconURL));
  policy->Set(key::kDefaultSearchProviderEncodings, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, encodings);
  policy->Set(key::kDefaultSearchProviderAlternateURLs, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, default_alternate_urls_.DeepCopy());
  policy->Set(key::kDefaultSearchProviderSearchTermsReplacementKey,
              POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
              base::Value::CreateStringValue(kReplacementKey));
}

// Checks that if the policy for default search is valid, i.e. there's a
// search URL, that all the elements have been given proper defaults.
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, MinimallyDefined) {
  PolicyMap policy;
  policy.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  policy.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateStringValue(kSearchURL));
  provider_.UpdateChromePolicy(policy);

  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderSearchURL, &value));
  EXPECT_TRUE(base::StringValue(kSearchURL).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderName, &value));
  EXPECT_TRUE(base::StringValue("test.com").Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderKeyword, &value));
  EXPECT_TRUE(base::StringValue("test.com").Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderSuggestURL,
                               &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderIconURL, &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderEncodings, &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderInstantURL,
                               &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderAlternateURLs,
                             &value));
  EXPECT_TRUE(base::ListValue().Equals(value));

  EXPECT_TRUE(
      store_->GetValue(prefs::kDefaultSearchProviderSearchTermsReplacementKey,
      &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));
}

// Checks that for a fully defined search policy, all elements have been
// read properly.
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, FullyDefined) {
  PolicyMap policy;
  BuildDefaultSearchPolicy(&policy);
  provider_.UpdateChromePolicy(policy);

  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderSearchURL, &value));
  EXPECT_TRUE(base::StringValue(kSearchURL).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderName, &value));
  EXPECT_TRUE(base::StringValue(kName).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderKeyword, &value));
  EXPECT_TRUE(base::StringValue(kKeyword).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderSuggestURL,
                               &value));
  EXPECT_TRUE(base::StringValue(kSuggestURL).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderIconURL, &value));
  EXPECT_TRUE(base::StringValue(kIconURL).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderEncodings, &value));
  EXPECT_TRUE(base::StringValue("UTF-16;UTF-8").Equals(value));

  EXPECT_TRUE(store_->GetValue(
      prefs::kDefaultSearchProviderAlternateURLs, &value));
  EXPECT_TRUE(default_alternate_urls_.Equals(value));

  EXPECT_TRUE(
      store_->GetValue(prefs::kDefaultSearchProviderSearchTermsReplacementKey,
      &value));
  EXPECT_TRUE(base::StringValue(kReplacementKey).Equals(value));
}

// Checks that if the default search policy is missing, that no elements of the
// default search policy will be present.
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, MissingUrl) {
  PolicyMap policy;
  BuildDefaultSearchPolicy(&policy);
  policy.Erase(key::kDefaultSearchProviderSearchURL);
  provider_.UpdateChromePolicy(policy);

  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderSearchURL, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderName, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderKeyword, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderSuggestURL, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderIconURL, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderEncodings, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderAlternateURLs,
                                NULL));
  EXPECT_FALSE(store_->GetValue(
      prefs::kDefaultSearchProviderSearchTermsReplacementKey, NULL));
}

// Checks that if the default search policy is invalid, that no elements of the
// default search policy will be present.
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, Invalid) {
  PolicyMap policy;
  BuildDefaultSearchPolicy(&policy);
  const char* const bad_search_url = "http://test.com/noSearchTerms";
  policy.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             base::Value::CreateStringValue(bad_search_url));
  provider_.UpdateChromePolicy(policy);

  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderSearchURL, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderName, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderKeyword, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderSuggestURL, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderIconURL, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderEncodings, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderAlternateURLs,
                                NULL));
  EXPECT_FALSE(store_->GetValue(
      prefs::kDefaultSearchProviderSearchTermsReplacementKey, NULL));
}

// Checks that if the default search policy is invalid, that no elements of the
// default search policy will be present.
TEST_F(ConfigurationPolicyPrefStoreDefaultSearchTest, Disabled) {
  PolicyMap policy;
  policy.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policy);

  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderEnabled, &value));
  base::FundamentalValue expected_enabled(false);
  EXPECT_TRUE(base::Value::Equals(&expected_enabled, value));
  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderSearchURL, &value));
  base::StringValue expected_search_url("");
  EXPECT_TRUE(base::Value::Equals(&expected_search_url, value));
}

// Tests Incognito mode availability preference setting.
class ConfigurationPolicyPrefStoreIncognitoModeTest
    : public ConfigurationPolicyPrefStoreTest {
 protected:
  static const int kIncognitoModeAvailabilityNotSet = -1;

  enum ObsoleteIncognitoEnabledValue {
    INCOGNITO_ENABLED_UNKNOWN,
    INCOGNITO_ENABLED_TRUE,
    INCOGNITO_ENABLED_FALSE
  };

  void SetPolicies(ObsoleteIncognitoEnabledValue incognito_enabled,
                   int availability) {
    PolicyMap policy;
    if (incognito_enabled != INCOGNITO_ENABLED_UNKNOWN) {
      policy.Set(key::kIncognitoEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 base::Value::CreateBooleanValue(
                     incognito_enabled == INCOGNITO_ENABLED_TRUE));
    }
    if (availability >= 0) {
      policy.Set(key::kIncognitoModeAvailability, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 base::Value::CreateIntegerValue(availability));
    }
    provider_.UpdateChromePolicy(policy);
  }

  void VerifyValues(IncognitoModePrefs::Availability availability) {
    const base::Value* value = NULL;
    EXPECT_TRUE(store_->GetValue(prefs::kIncognitoModeAvailability, &value));
    EXPECT_TRUE(base::FundamentalValue(availability).Equals(value));
  }
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
  const base::Value* value = NULL;
  EXPECT_FALSE(store_->GetValue(prefs::kIncognitoModeAvailability, &value));
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
    : public ConfigurationPolicyPrefStoreTest {};

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Default) {
  EXPECT_FALSE(store_->GetValue(prefs::kSyncManaged, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Enabled) {
  PolicyMap policy;
  policy.Set(key::kSyncDisabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policy);
  // Enabling Sync should not set the pref.
  EXPECT_FALSE(store_->GetValue(prefs::kSyncManaged, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Disabled) {
  PolicyMap policy;
  policy.Set(key::kSyncDisabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policy);
  // Sync should be flagged as managed.
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kSyncManaged, &value));
  ASSERT_TRUE(value);
  bool sync_managed = false;
  bool result = value->GetAsBoolean(&sync_managed);
  ASSERT_TRUE(result);
  EXPECT_TRUE(sync_managed);
}

// Test cases for how the DownloadDirectory and AllowFileSelectionDialogs policy
// influence the PromptForDownload preference.
class ConfigurationPolicyPrefStorePromptDownloadTest
    : public ConfigurationPolicyPrefStoreTest {};

TEST_F(ConfigurationPolicyPrefStorePromptDownloadTest, Default) {
  EXPECT_FALSE(store_->GetValue(prefs::kPromptForDownload, NULL));
}

#if !defined(OS_CHROMEOS)
TEST_F(ConfigurationPolicyPrefStorePromptDownloadTest, SetDownloadDirectory) {
  PolicyMap policy;
  EXPECT_FALSE(store_->GetValue(prefs::kPromptForDownload, NULL));
  policy.Set(key::kDownloadDirectory, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateStringValue(""));
  provider_.UpdateChromePolicy(policy);

  // Setting a DownloadDirectory should disable the PromptForDownload pref.
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kPromptForDownload,
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
  PolicyMap policy;
  EXPECT_FALSE(store_->GetValue(prefs::kPromptForDownload, NULL));
  policy.Set(key::kAllowFileSelectionDialogs, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policy);

  // Allowing file-selection dialogs should not influence the PromptForDownload
  // pref.
  EXPECT_FALSE(store_->GetValue(prefs::kPromptForDownload, NULL));
}

TEST_F(ConfigurationPolicyPrefStorePromptDownloadTest,
       DisableFileSelectionDialogs) {
  PolicyMap policy;
  EXPECT_FALSE(store_->GetValue(prefs::kPromptForDownload, NULL));
  policy.Set(key::kAllowFileSelectionDialogs, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policy);

  // Disabling file-selection dialogs should disable the PromptForDownload pref.
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kPromptForDownload,
                                                 &value));
  ASSERT_TRUE(value);
  bool prompt_for_download = true;
  bool result = value->GetAsBoolean(&prompt_for_download);
  ASSERT_TRUE(result);
  EXPECT_FALSE(prompt_for_download);
}

// Test cases for the Autofill policy setting.
class ConfigurationPolicyPrefStoreAutofillTest
    : public ConfigurationPolicyPrefStoreTest {};

TEST_F(ConfigurationPolicyPrefStoreAutofillTest, Default) {
  EXPECT_FALSE(store_->GetValue(prefs::kAutofillEnabled, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreAutofillTest, Enabled) {
  PolicyMap policy;
  policy.Set(key::kAutoFillEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policy);
  // Enabling Autofill should not set the pref.
  EXPECT_FALSE(store_->GetValue(prefs::kAutofillEnabled, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreAutofillTest, Disabled) {
  PolicyMap policy;
  policy.Set(key::kAutoFillEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policy);
  // Disabling Autofill should switch the pref to managed.
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kAutofillEnabled, &value));
  ASSERT_TRUE(value);
  bool autofill_enabled = true;
  bool result = value->GetAsBoolean(&autofill_enabled);
  ASSERT_TRUE(result);
  EXPECT_FALSE(autofill_enabled);
}

// Exercises the policy refresh mechanism.
class ConfigurationPolicyPrefStoreRefreshTest
    : public ConfigurationPolicyPrefStoreTest {
 protected:
  virtual void SetUp() {
    ConfigurationPolicyPrefStoreTest::SetUp();
    store_->AddObserver(&observer_);
  }

  virtual void TearDown() {
    store_->RemoveObserver(&observer_);
    ConfigurationPolicyPrefStoreTest::TearDown();
  }

  PrefStoreObserverMock observer_;
};

TEST_F(ConfigurationPolicyPrefStoreRefreshTest, Refresh) {
  const base::Value* value = NULL;
  EXPECT_FALSE(store_->GetValue(prefs::kHomePage, NULL));

  EXPECT_CALL(observer_, OnPrefValueChanged(prefs::kHomePage)).Times(1);
  PolicyMap policy;
  policy.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateStringValue("http://www.chromium.org"));
  provider_.UpdateChromePolicy(policy);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(store_->GetValue(prefs::kHomePage, &value));
  EXPECT_TRUE(base::StringValue("http://www.chromium.org").Equals(value));

  EXPECT_CALL(observer_, OnPrefValueChanged(_)).Times(0);
  provider_.UpdateChromePolicy(policy);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnPrefValueChanged(prefs::kHomePage)).Times(1);
  policy.Erase(key::kHomepageLocation);
  provider_.UpdateChromePolicy(policy);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_FALSE(store_->GetValue(prefs::kHomePage, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreRefreshTest, Initialization) {
  EXPECT_FALSE(store_->IsInitializationComplete());
  EXPECT_CALL(provider_, IsInitializationComplete())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(observer_, OnInitializationCompleted(true)).Times(1);
  PolicyMap policy;
  provider_.UpdateChromePolicy(policy);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(store_->IsInitializationComplete());
}

// Tests for policies that don't quite fit the previous patterns.
class ConfigurationPolicyPrefStoreOthersTest
    : public ConfigurationPolicyPrefStoreTest {};

TEST_F(ConfigurationPolicyPrefStoreOthersTest, JavascriptEnabled) {
  // This is a boolean policy, but affects an integer preference.
  EXPECT_FALSE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, NULL));
  PolicyMap policy;
  policy.Set(key::kJavascriptEnabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policy);
  EXPECT_FALSE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, NULL));
  policy.Set(key::kJavascriptEnabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policy);
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting,
                               &value));
  EXPECT_TRUE(base::FundamentalValue(CONTENT_SETTING_BLOCK).Equals(value));
}

TEST_F(ConfigurationPolicyPrefStoreOthersTest, JavascriptEnabledOverridden) {
  EXPECT_FALSE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, NULL));
  PolicyMap policy;
  policy.Set(key::kJavascriptEnabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policy);
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting,
                               &value));
  EXPECT_TRUE(base::FundamentalValue(CONTENT_SETTING_BLOCK).Equals(value));
  // DefaultJavaScriptSetting overrides JavascriptEnabled.
  policy.Set(key::kDefaultJavaScriptSetting, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             base::Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
  provider_.UpdateChromePolicy(policy);
  EXPECT_TRUE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting,
                               &value));
  EXPECT_TRUE(base::FundamentalValue(CONTENT_SETTING_ALLOW).Equals(value));
}

}  // namespace policy
