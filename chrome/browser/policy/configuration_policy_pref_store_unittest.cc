// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_pref_store_unittest.h"

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/prefs/pref_store_observer_mock.h"
#include "base/run_loop.h"
#include "chrome/browser/policy/configuration_policy_handler.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/external_data_fetcher.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service_impl.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

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

ConfigurationPolicyPrefStoreTest::ConfigurationPolicyPrefStoreTest() {
  EXPECT_CALL(provider_, IsInitializationComplete(_))
      .WillRepeatedly(Return(false));
  provider_.Init();
  PolicyServiceImpl::Providers providers;
  providers.push_back(&provider_);
  policy_service_.reset(new PolicyServiceImpl(providers));
  store_ = new ConfigurationPolicyPrefStore(
      policy_service_.get(), &handler_list_, POLICY_LEVEL_MANDATORY);
}

ConfigurationPolicyPrefStoreTest::~ConfigurationPolicyPrefStoreTest() {}

void ConfigurationPolicyPrefStoreTest::TearDown() {
  provider_.Shutdown();
}

void ConfigurationPolicyPrefStoreTest::UpdateProviderPolicy(
    const PolicyMap& policy) {
  provider_.UpdateChromePolicy(policy);
  base::RunLoop loop;
  loop.RunUntilIdle();
}

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
             POLICY_SCOPE_USER, in_value, NULL);
  UpdateProviderPolicy(policy);
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
             base::Value::CreateStringValue("http://chromium.org"), NULL);
  UpdateProviderPolicy(policy);
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
        PolicyAndPref(key::kVariationsRestrictParameter,
                      prefs::kVariationsRestrictParameter)));

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyPrefStoreDownloadDirectoryInstance,
    ConfigurationPolicyPrefStoreStringTest,
    testing::Values(
        PolicyAndPref(key::kDiskCacheDir,
                      prefs::kDiskCacheDir),
        PolicyAndPref(key::kDownloadDirectory,
                      prefs::kDownloadDefaultDirectory)));
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

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
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false), NULL);
  UpdateProviderPolicy(policy);
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(GetParam().pref_name(), &value));
  ASSERT_TRUE(value);
  bool boolean_value = true;
  bool result = value->GetAsBoolean(&boolean_value);
  ASSERT_TRUE(result);
  EXPECT_FALSE(boolean_value);

  policy.Set(GetParam().policy_name(), POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true), NULL);
  UpdateProviderPolicy(policy);
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
        PolicyAndPref(key::kRequireOnlineRevocationChecksForLocalAnchors,
                      prefs::kCertRevocationCheckingRequiredLocalAnchors),
        PolicyAndPref(key::kDisableAuthNegotiateCnameLookup,
                      prefs::kDisableAuthNegotiateCnameLookup),
        PolicyAndPref(key::kEnableAuthNegotiatePort,
                      prefs::kEnableAuthNegotiatePort),
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
                      prefs::kDevToolsDisabled),
        PolicyAndPref(key::kHideWebStoreIcon,
                      prefs::kHideWebStoreIcon)));

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
        PolicyAndPref(key::kShowAccessibilityOptionsInSystemTrayMenu,
                      prefs::kShouldAlwaysShowAccessibilityMenu),
        PolicyAndPref(key::kLargeCursorEnabled,
                      prefs::kLargeCursorEnabled),
        PolicyAndPref(key::kSpokenFeedbackEnabled,
                      prefs::kSpokenFeedbackEnabled),
        PolicyAndPref(key::kHighContrastEnabled,
                      prefs::kHighContrastEnabled),
        PolicyAndPref(key::kAudioOutputAllowed,
                      prefs::kAudioOutputAllowed),
        PolicyAndPref(key::kAudioCaptureAllowed,
                      prefs::kAudioCaptureAllowed),
        PolicyAndPref(key::kAttestationEnabledForUser,
                      prefs::kAttestationEnabled)));
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
             POLICY_SCOPE_USER, base::Value::CreateIntegerValue(2), NULL);
  UpdateProviderPolicy(policy);
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
                      policy_prefs::kUserPolicyRefreshRate),
        PolicyAndPref(key::kMaxConnectionsPerProxy,
                      prefs::kMaxConnectionsPerProxy)));

// Test cases for the Sync policy setting.
class ConfigurationPolicyPrefStoreSyncTest
    : public ConfigurationPolicyPrefStoreTest {};

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Default) {
  EXPECT_FALSE(store_->GetValue(prefs::kSyncManaged, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Enabled) {
  PolicyMap policy;
  policy.Set(key::kSyncDisabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false), NULL);
  UpdateProviderPolicy(policy);
  // Enabling Sync should not set the pref.
  EXPECT_FALSE(store_->GetValue(prefs::kSyncManaged, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreSyncTest, Disabled) {
  PolicyMap policy;
  policy.Set(key::kSyncDisabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(true), NULL);
  UpdateProviderPolicy(policy);
  // Sync should be flagged as managed.
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kSyncManaged, &value));
  ASSERT_TRUE(value);
  bool sync_managed = false;
  bool result = value->GetAsBoolean(&sync_managed);
  ASSERT_TRUE(result);
  EXPECT_TRUE(sync_managed);
}

// Test cases for how the AllowFileSelectionDialogs policy influences the
// PromptForDownload preference.
class ConfigurationPolicyPrefStorePromptDownloadTest
    : public ConfigurationPolicyPrefStoreTest {};

TEST_F(ConfigurationPolicyPrefStorePromptDownloadTest, Default) {
  EXPECT_FALSE(store_->GetValue(prefs::kPromptForDownload, NULL));
}

TEST_F(ConfigurationPolicyPrefStorePromptDownloadTest,
       EnableFileSelectionDialogs) {
  PolicyMap policy;
  EXPECT_FALSE(store_->GetValue(prefs::kPromptForDownload, NULL));
  policy.Set(key::kAllowFileSelectionDialogs, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true), NULL);
  UpdateProviderPolicy(policy);

  // Allowing file-selection dialogs should not influence the PromptForDownload
  // pref.
  EXPECT_FALSE(store_->GetValue(prefs::kPromptForDownload, NULL));
}

TEST_F(ConfigurationPolicyPrefStorePromptDownloadTest,
       DisableFileSelectionDialogs) {
  PolicyMap policy;
  EXPECT_FALSE(store_->GetValue(prefs::kPromptForDownload, NULL));
  policy.Set(key::kAllowFileSelectionDialogs, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false), NULL);
  UpdateProviderPolicy(policy);

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
  EXPECT_FALSE(store_->GetValue(autofill::prefs::kAutofillEnabled, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreAutofillTest, Enabled) {
  PolicyMap policy;
  policy.Set(key::kAutoFillEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(true), NULL);
  UpdateProviderPolicy(policy);
  // Enabling Autofill should not set the pref.
  EXPECT_FALSE(store_->GetValue(autofill::prefs::kAutofillEnabled, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreAutofillTest, Disabled) {
  PolicyMap policy;
  policy.Set(key::kAutoFillEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(false), NULL);
  UpdateProviderPolicy(policy);
  // Disabling Autofill should switch the pref to managed.
  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(autofill::prefs::kAutofillEnabled, &value));
  ASSERT_TRUE(value);
  bool autofill_enabled = true;
  bool result = value->GetAsBoolean(&autofill_enabled);
  ASSERT_TRUE(result);
  EXPECT_FALSE(autofill_enabled);
}

#if defined(OS_CHROMEOS)
// Test cases for the screen magnifier type policy setting.
class ConfigurationPolicyPrefStoreScreenMagnifierTypeTest
    : public ConfigurationPolicyPrefStoreTest {};

TEST_F(ConfigurationPolicyPrefStoreScreenMagnifierTypeTest, Default) {
  EXPECT_FALSE(store_->GetValue(prefs::kScreenMagnifierEnabled, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kScreenMagnifierType, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreScreenMagnifierTypeTest, Disabled) {
  PolicyMap policy;
  policy.Set(key::kScreenMagnifierType, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateIntegerValue(0), NULL);
  UpdateProviderPolicy(policy);
  const base::Value* enabled = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kScreenMagnifierEnabled, &enabled));
  ASSERT_TRUE(enabled);
  EXPECT_TRUE(base::FundamentalValue(false).Equals(enabled));
  const base::Value* type = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kScreenMagnifierType, &type));
  ASSERT_TRUE(type);
  EXPECT_TRUE(base::FundamentalValue(0).Equals(type));
}

TEST_F(ConfigurationPolicyPrefStoreScreenMagnifierTypeTest, Enabled) {
  PolicyMap policy;
  policy.Set(key::kScreenMagnifierType, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, base::Value::CreateIntegerValue(1), NULL);
  UpdateProviderPolicy(policy);
  const base::Value* enabled = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kScreenMagnifierEnabled, &enabled));
  ASSERT_TRUE(enabled);
  EXPECT_TRUE(base::FundamentalValue(true).Equals(enabled));
  const base::Value* type = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kScreenMagnifierType, &type));
  ASSERT_TRUE(type);
  EXPECT_TRUE(base::FundamentalValue(1).Equals(type));
}
#endif  // defined(OS_CHROMEOS)

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
             base::Value::CreateStringValue("http://www.chromium.org"), NULL);
  UpdateProviderPolicy(policy);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(store_->GetValue(prefs::kHomePage, &value));
  EXPECT_TRUE(base::StringValue("http://www.chromium.org").Equals(value));

  EXPECT_CALL(observer_, OnPrefValueChanged(_)).Times(0);
  UpdateProviderPolicy(policy);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnPrefValueChanged(prefs::kHomePage)).Times(1);
  policy.Erase(key::kHomepageLocation);
  UpdateProviderPolicy(policy);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_FALSE(store_->GetValue(prefs::kHomePage, NULL));
}

TEST_F(ConfigurationPolicyPrefStoreRefreshTest, Initialization) {
  EXPECT_FALSE(store_->IsInitializationComplete());
  EXPECT_CALL(provider_, IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(observer_, OnInitializationCompleted(true)).Times(1);
  PolicyMap policy;
  UpdateProviderPolicy(policy);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(store_->IsInitializationComplete());
}

}  // namespace policy
