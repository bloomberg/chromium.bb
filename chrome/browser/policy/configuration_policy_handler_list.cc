// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler_list.h"

#include <limits>

#include "base/prefs/pref_value_map.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_handler.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/onc/onc_constants.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/policy/configuration_policy_handler_chromeos.h"
#endif  // defined(OS_CHROMEOS)

namespace policy {

namespace {

// Maps a policy type to a preference path, and to the expected value type.
// This is the entry type of |kSimplePolicyMap| below.
struct PolicyToPreferenceMapEntry {
  const char* policy_name;
  const char* preference_path;
  base::Value::Type value_type;
};

// List of policy types to preference names. This is used for simple policies
// that directly map to a single preference.
const PolicyToPreferenceMapEntry kSimplePolicyMap[] = {
  { key::kHomepageLocation,
    prefs::kHomePage,
    Value::TYPE_STRING },
  { key::kHomepageIsNewTabPage,
    prefs::kHomePageIsNewTabPage,
    Value::TYPE_BOOLEAN },
  { key::kRestoreOnStartupURLs,
    prefs::kURLsToRestoreOnStartup,
    Value::TYPE_LIST },
  { key::kAlternateErrorPagesEnabled,
    prefs::kAlternateErrorPagesEnabled,
    Value::TYPE_BOOLEAN },
  { key::kSearchSuggestEnabled,
    prefs::kSearchSuggestEnabled,
    Value::TYPE_BOOLEAN },
  { key::kDnsPrefetchingEnabled,
    prefs::kNetworkPredictionEnabled,
    Value::TYPE_BOOLEAN },
  { key::kBuiltInDnsClientEnabled,
    prefs::kBuiltInDnsClientEnabled,
    Value::TYPE_BOOLEAN },
  { key::kDisableSpdy,
    prefs::kDisableSpdy,
    Value::TYPE_BOOLEAN },
  { key::kDisabledSchemes,
    prefs::kDisabledSchemes,
    Value::TYPE_LIST },
  { key::kSafeBrowsingEnabled,
    prefs::kSafeBrowsingEnabled,
    Value::TYPE_BOOLEAN },
  { key::kForceSafeSearch,
    prefs::kForceSafeSearch,
    Value::TYPE_BOOLEAN },
  { key::kPasswordManagerEnabled,
    prefs::kPasswordManagerEnabled,
    Value::TYPE_BOOLEAN },
  { key::kPasswordManagerAllowShowPasswords,
    prefs::kPasswordManagerAllowShowPasswords,
    Value::TYPE_BOOLEAN },
  { key::kPrintingEnabled,
    prefs::kPrintingEnabled,
    Value::TYPE_BOOLEAN },
  { key::kDisablePrintPreview,
    prefs::kPrintPreviewDisabled,
    Value::TYPE_BOOLEAN },
  { key::kMetricsReportingEnabled,
    prefs::kMetricsReportingEnabled,
    Value::TYPE_BOOLEAN },
  { key::kApplicationLocaleValue,
    prefs::kApplicationLocale,
    Value::TYPE_STRING },
  { key::kExtensionInstallForcelist,
    prefs::kExtensionInstallForceList,
    Value::TYPE_LIST },
  { key::kDisabledPlugins,
    prefs::kPluginsDisabledPlugins,
    Value::TYPE_LIST },
  { key::kDisabledPluginsExceptions,
    prefs::kPluginsDisabledPluginsExceptions,
    Value::TYPE_LIST },
  { key::kEnabledPlugins,
    prefs::kPluginsEnabledPlugins,
    Value::TYPE_LIST },
  { key::kShowHomeButton,
    prefs::kShowHomeButton,
    Value::TYPE_BOOLEAN },
  { key::kSavingBrowserHistoryDisabled,
    prefs::kSavingBrowserHistoryDisabled,
    Value::TYPE_BOOLEAN },
  { key::kAllowDeletingBrowserHistory,
    prefs::kAllowDeletingBrowserHistory,
    Value::TYPE_BOOLEAN },
  { key::kDeveloperToolsDisabled,
    prefs::kDevToolsDisabled,
    Value::TYPE_BOOLEAN },
  { key::kBlockThirdPartyCookies,
    prefs::kBlockThirdPartyCookies,
    Value::TYPE_BOOLEAN },
  { key::kDefaultCookiesSetting,
    prefs::kManagedDefaultCookiesSetting,
    Value::TYPE_INTEGER },
  { key::kDefaultImagesSetting,
    prefs::kManagedDefaultImagesSetting,
    Value::TYPE_INTEGER },
  { key::kDefaultPluginsSetting,
    prefs::kManagedDefaultPluginsSetting,
    Value::TYPE_INTEGER },
  { key::kDefaultPopupsSetting,
    prefs::kManagedDefaultPopupsSetting,
    Value::TYPE_INTEGER },
  { key::kAutoSelectCertificateForUrls,
    prefs::kManagedAutoSelectCertificateForUrls,
    Value::TYPE_LIST },
  { key::kCookiesAllowedForUrls,
    prefs::kManagedCookiesAllowedForUrls,
    Value::TYPE_LIST },
  { key::kCookiesBlockedForUrls,
    prefs::kManagedCookiesBlockedForUrls,
    Value::TYPE_LIST },
  { key::kCookiesSessionOnlyForUrls,
    prefs::kManagedCookiesSessionOnlyForUrls,
    Value::TYPE_LIST },
  { key::kImagesAllowedForUrls,
    prefs::kManagedImagesAllowedForUrls,
    Value::TYPE_LIST },
  { key::kImagesBlockedForUrls,
    prefs::kManagedImagesBlockedForUrls,
    Value::TYPE_LIST },
  { key::kJavaScriptAllowedForUrls,
    prefs::kManagedJavaScriptAllowedForUrls,
    Value::TYPE_LIST },
  { key::kJavaScriptBlockedForUrls,
    prefs::kManagedJavaScriptBlockedForUrls,
    Value::TYPE_LIST },
  { key::kPluginsAllowedForUrls,
    prefs::kManagedPluginsAllowedForUrls,
    Value::TYPE_LIST },
  { key::kPluginsBlockedForUrls,
    prefs::kManagedPluginsBlockedForUrls,
    Value::TYPE_LIST },
  { key::kPopupsAllowedForUrls,
    prefs::kManagedPopupsAllowedForUrls,
    Value::TYPE_LIST },
  { key::kPopupsBlockedForUrls,
    prefs::kManagedPopupsBlockedForUrls,
    Value::TYPE_LIST },
  { key::kNotificationsAllowedForUrls,
    prefs::kManagedNotificationsAllowedForUrls,
    Value::TYPE_LIST },
  { key::kNotificationsBlockedForUrls,
    prefs::kManagedNotificationsBlockedForUrls,
    Value::TYPE_LIST },
  { key::kDefaultNotificationsSetting,
    prefs::kManagedDefaultNotificationsSetting,
    Value::TYPE_INTEGER },
  { key::kDefaultGeolocationSetting,
    prefs::kManagedDefaultGeolocationSetting,
    Value::TYPE_INTEGER },
  { key::kSigninAllowed,
    prefs::kSigninAllowed,
    Value::TYPE_BOOLEAN },
  { key::kEnableOriginBoundCerts,
    prefs::kEnableOriginBoundCerts,
    Value::TYPE_BOOLEAN },
  { key::kDisableSSLRecordSplitting,
    prefs::kDisableSSLRecordSplitting,
    Value::TYPE_BOOLEAN },
  { key::kEnableOnlineRevocationChecks,
    prefs::kCertRevocationCheckingEnabled,
    Value::TYPE_BOOLEAN },
  { key::kAuthSchemes,
    prefs::kAuthSchemes,
    Value::TYPE_STRING },
  { key::kDisableAuthNegotiateCnameLookup,
    prefs::kDisableAuthNegotiateCnameLookup,
    Value::TYPE_BOOLEAN },
  { key::kEnableAuthNegotiatePort,
    prefs::kEnableAuthNegotiatePort,
    Value::TYPE_BOOLEAN },
  { key::kAuthServerWhitelist,
    prefs::kAuthServerWhitelist,
    Value::TYPE_STRING },
  { key::kAuthNegotiateDelegateWhitelist,
    prefs::kAuthNegotiateDelegateWhitelist,
    Value::TYPE_STRING },
  { key::kGSSAPILibraryName,
    prefs::kGSSAPILibraryName,
    Value::TYPE_STRING },
  { key::kAllowCrossOriginAuthPrompt,
    prefs::kAllowCrossOriginAuthPrompt,
    Value::TYPE_BOOLEAN },
  { key::kDisable3DAPIs,
    prefs::kDisable3DAPIs,
    Value::TYPE_BOOLEAN },
  { key::kDisablePluginFinder,
    prefs::kDisablePluginFinder,
    Value::TYPE_BOOLEAN },
  { key::kDiskCacheSize,
    prefs::kDiskCacheSize,
    Value::TYPE_INTEGER },
  { key::kMediaCacheSize,
    prefs::kMediaCacheSize,
    Value::TYPE_INTEGER },
  { key::kPolicyRefreshRate,
    prefs::kUserPolicyRefreshRate,
    Value::TYPE_INTEGER },
  { key::kDevicePolicyRefreshRate,
    prefs::kDevicePolicyRefreshRate,
    Value::TYPE_INTEGER },
  { key::kInstantEnabled,
    prefs::kInstantEnabled,
    Value::TYPE_BOOLEAN },
  { key::kDefaultBrowserSettingEnabled,
    prefs::kDefaultBrowserSettingEnabled,
    Value::TYPE_BOOLEAN },
  { key::kRemoteAccessHostFirewallTraversal,
    prefs::kRemoteAccessHostFirewallTraversal,
    Value::TYPE_BOOLEAN },
  { key::kRemoteAccessHostRequireTwoFactor,
    prefs::kRemoteAccessHostRequireTwoFactor,
    Value::TYPE_BOOLEAN },
  { key::kRemoteAccessHostDomain,
    prefs::kRemoteAccessHostDomain,
    Value::TYPE_STRING },
  { key::kRemoteAccessHostTalkGadgetPrefix,
    prefs::kRemoteAccessHostTalkGadgetPrefix,
    Value::TYPE_STRING },
  { key::kRemoteAccessHostRequireCurtain,
    prefs::kRemoteAccessHostRequireCurtain,
    Value::TYPE_BOOLEAN },
  { key::kCloudPrintProxyEnabled,
    prefs::kCloudPrintProxyEnabled,
    Value::TYPE_BOOLEAN },
  { key::kCloudPrintSubmitEnabled,
    prefs::kCloudPrintSubmitEnabled,
    Value::TYPE_BOOLEAN },
  { key::kTranslateEnabled,
    prefs::kEnableTranslate,
    Value::TYPE_BOOLEAN },
  { key::kAllowOutdatedPlugins,
    prefs::kPluginsAllowOutdated,
    Value::TYPE_BOOLEAN },
  { key::kAlwaysAuthorizePlugins,
    prefs::kPluginsAlwaysAuthorize,
    Value::TYPE_BOOLEAN },
  { key::kBookmarkBarEnabled,
    prefs::kShowBookmarkBar,
    Value::TYPE_BOOLEAN },
  { key::kEditBookmarksEnabled,
    prefs::kEditBookmarksEnabled,
    Value::TYPE_BOOLEAN },
  { key::kAllowFileSelectionDialogs,
    prefs::kAllowFileSelectionDialogs,
    Value::TYPE_BOOLEAN },
  { key::kImportBookmarks,
    prefs::kImportBookmarks,
    Value::TYPE_BOOLEAN },
  { key::kImportHistory,
    prefs::kImportHistory,
    Value::TYPE_BOOLEAN },
  { key::kImportHomepage,
    prefs::kImportHomepage,
    Value::TYPE_BOOLEAN },
  { key::kImportSearchEngine,
    prefs::kImportSearchEngine,
    Value::TYPE_BOOLEAN },
  { key::kImportSavedPasswords,
    prefs::kImportSavedPasswords,
    Value::TYPE_BOOLEAN },
  { key::kMaxConnectionsPerProxy,
    prefs::kMaxConnectionsPerProxy,
    Value::TYPE_INTEGER },
  { key::kURLBlacklist,
    prefs::kUrlBlacklist,
    Value::TYPE_LIST },
  { key::kURLWhitelist,
    prefs::kUrlWhitelist,
    Value::TYPE_LIST },
  { key::kEnterpriseWebStoreURL,
    prefs::kEnterpriseWebStoreURL,
    Value::TYPE_STRING },
  { key::kEnterpriseWebStoreName,
    prefs::kEnterpriseWebStoreName,
    Value::TYPE_STRING },
  { key::kEnableMemoryInfo,
    prefs::kEnableMemoryInfo,
    Value::TYPE_BOOLEAN },
  { key::kRestrictSigninToPattern,
    prefs::kGoogleServicesUsernamePattern,
    Value::TYPE_STRING },
  { key::kDefaultMediaStreamSetting,
    prefs::kManagedDefaultMediaStreamSetting,
    Value::TYPE_INTEGER },
  { key::kDisableSafeBrowsingProceedAnyway,
    prefs::kSafeBrowsingProceedAnywayDisabled,
    Value::TYPE_BOOLEAN },
  { key::kSpellCheckServiceEnabled,
    prefs::kSpellCheckUseSpellingService,
    Value::TYPE_BOOLEAN },
  { key::kDisableScreenshots,
    prefs::kDisableScreenshots,
    Value::TYPE_BOOLEAN },
  { key::kAudioCaptureAllowed,
    prefs::kAudioCaptureAllowed,
    Value::TYPE_BOOLEAN },
  { key::kVideoCaptureAllowed,
    prefs::kVideoCaptureAllowed,
    Value::TYPE_BOOLEAN },
  { key::kHideWebStoreIcon,
    prefs::kHideWebStoreIcon,
    Value::TYPE_BOOLEAN },

#if defined(OS_CHROMEOS)
  { key::kChromeOsLockOnIdleSuspend,
    prefs::kEnableScreenLock,
    Value::TYPE_BOOLEAN },
  { key::kChromeOsReleaseChannel,
    prefs::kChromeOsReleaseChannel,
    Value::TYPE_STRING },
  { key::kDriveDisabled,
    prefs::kDisableDrive,
    Value::TYPE_BOOLEAN },
  { key::kDriveDisabledOverCellular,
    prefs::kDisableDriveOverCellular,
    Value::TYPE_BOOLEAN },
  { key::kExternalStorageDisabled,
    prefs::kExternalStorageDisabled,
    Value::TYPE_BOOLEAN },
  { key::kAudioOutputAllowed,
    prefs::kAudioOutputAllowed,
    Value::TYPE_BOOLEAN },
  { key::kShowLogoutButtonInTray,
    prefs::kShowLogoutButtonInTray,
    Value::TYPE_BOOLEAN },
  { key::kShelfAutoHideBehavior,
    prefs::kShelfAutoHideBehaviorLocal,
    Value::TYPE_STRING },
  { key::kSessionLengthLimit,
    prefs::kSessionLengthLimit,
    Value::TYPE_INTEGER },
  { key::kPowerManagementUsesAudioActivity,
    prefs::kPowerUseAudioActivity,
    Value::TYPE_BOOLEAN },
  { key::kPowerManagementUsesVideoActivity,
    prefs::kPowerUseVideoActivity,
    Value::TYPE_BOOLEAN },
  { key::kTermsOfServiceURL,
    prefs::kTermsOfServiceURL,
    Value::TYPE_STRING },
  { key::kShowAccessibilityOptionsInSystemTrayMenu,
    prefs::kShouldAlwaysShowAccessibilityMenu,
    Value::TYPE_BOOLEAN },
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  { key::kBackgroundModeEnabled,
    prefs::kBackgroundModeEnabled,
    Value::TYPE_BOOLEAN },
#endif  // !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
};

// Mapping from extension type names to Manifest::Type.
StringToIntEnumListPolicyHandler::MappingEntry kExtensionAllowedTypesMap[] = {
  { "extension", extensions::Manifest::TYPE_EXTENSION },
  { "theme", extensions::Manifest::TYPE_THEME },
  { "user_script", extensions::Manifest::TYPE_USER_SCRIPT },
  { "hosted_app", extensions::Manifest::TYPE_HOSTED_APP },
  { "legacy_packaged_app", extensions::Manifest::TYPE_LEGACY_PACKAGED_APP },
  { "platform_app", extensions::Manifest::TYPE_PLATFORM_APP },
};

}  // namespace

ConfigurationPolicyHandlerList::ConfigurationPolicyHandlerList() {
  for (size_t i = 0; i < arraysize(kSimplePolicyMap); ++i) {
    handlers_.push_back(
        new SimplePolicyHandler(kSimplePolicyMap[i].policy_name,
                                kSimplePolicyMap[i].preference_path,
                                kSimplePolicyMap[i].value_type));
  }

  handlers_.push_back(new AutofillPolicyHandler());
  handlers_.push_back(new ClearSiteDataOnExitPolicyHandler());
  handlers_.push_back(new DefaultSearchPolicyHandler());
  handlers_.push_back(new DiskCacheDirPolicyHandler());
  handlers_.push_back(new FileSelectionDialogsHandler());
  handlers_.push_back(new IncognitoModePolicyHandler());
  handlers_.push_back(new JavascriptPolicyHandler());
  handlers_.push_back(new ProxyPolicyHandler());
  handlers_.push_back(new RestoreOnStartupPolicyHandler());
  handlers_.push_back(new SyncPolicyHandler());

  handlers_.push_back(
      new ExtensionListPolicyHandler(key::kExtensionInstallWhitelist,
                                     prefs::kExtensionInstallAllowList,
                                     false));
  handlers_.push_back(
      new ExtensionListPolicyHandler(key::kExtensionInstallBlacklist,
                                     prefs::kExtensionInstallDenyList,
                                     true));
  handlers_.push_back(new ExtensionInstallForcelistPolicyHandler());
  handlers_.push_back(
      new ExtensionURLPatternListPolicyHandler(
          key::kExtensionInstallSources,
          prefs::kExtensionAllowedInstallSites));
  handlers_.push_back(
      new StringToIntEnumListPolicyHandler(
          key::kExtensionAllowedTypes, prefs::kExtensionAllowedTypes,
          kExtensionAllowedTypesMap,
          kExtensionAllowedTypesMap + arraysize(kExtensionAllowedTypesMap)));

#if !defined(OS_CHROMEOS)
  handlers_.push_back(new DownloadDirPolicyHandler());
#endif  // !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
  handlers_.push_back(
      new NetworkConfigurationPolicyHandler(
          key::kDeviceOpenNetworkConfiguration,
          chromeos::onc::ONC_SOURCE_DEVICE_POLICY));
  handlers_.push_back(
      new NetworkConfigurationPolicyHandler(
          key::kOpenNetworkConfiguration,
          chromeos::onc::ONC_SOURCE_USER_POLICY));
  handlers_.push_back(new PinnedLauncherAppsPolicyHandler());

  handlers_.push_back(
      new IntRangePolicyHandler(
          key::kScreenDimDelayAC,
          prefs::kPowerAcScreenDimDelayMs,
          0, INT_MAX, true));
  handlers_.push_back(
      new IntRangePolicyHandler(
          key::kScreenOffDelayAC,
          prefs::kPowerAcScreenOffDelayMs,
          0, INT_MAX, true));
  handlers_.push_back(
      new IntRangePolicyHandler(
          key::kScreenLockDelayAC,
          prefs::kPowerAcScreenLockDelayMs,
          0, INT_MAX, true));
  handlers_.push_back(
      new IntRangePolicyHandler(
          key::kIdleDelayAC,
          prefs::kPowerAcIdleDelayMs,
          0, INT_MAX, true));
  handlers_.push_back(
      new IntRangePolicyHandler(
          key::kScreenDimDelayBattery,
          prefs::kPowerBatteryScreenDimDelayMs,
          0, INT_MAX, true));
  handlers_.push_back(
      new IntRangePolicyHandler(
          key::kScreenOffDelayBattery,
          prefs::kPowerBatteryScreenOffDelayMs,
          0, INT_MAX, true));
  handlers_.push_back(
      new IntRangePolicyHandler(
          key::kScreenLockDelayBattery,
          prefs::kPowerBatteryScreenLockDelayMs,
          0, INT_MAX, true));
  handlers_.push_back(
      new IntRangePolicyHandler(
          key::kIdleDelayBattery,
          prefs::kPowerBatteryIdleDelayMs,
          0, INT_MAX, true));
  handlers_.push_back(
      new IntRangePolicyHandler(
          key::kIdleAction,
          prefs::kPowerIdleAction,
          0, 3, false));
  handlers_.push_back(
      new IntRangePolicyHandler(
          key::kLidCloseAction,
          prefs::kPowerLidClosedAction,
          0, 3, false));
  handlers_.push_back(
      new IntPercentageToDoublePolicyHandler(
          key::kPresentationIdleDelayScale,
          prefs::kPowerPresentationIdleDelayFactor,
          100, INT_MAX, true));
#endif  // defined(OS_CHROMEOS)
}

ConfigurationPolicyHandlerList::~ConfigurationPolicyHandlerList() {
  STLDeleteElements(&handlers_);
}

void ConfigurationPolicyHandlerList::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs,
    PolicyErrorMap* errors) const {
  PolicyErrorMap scoped_errors;
  if (!errors)
    errors = &scoped_errors;

  std::vector<ConfigurationPolicyHandler*>::const_iterator handler;
  for (handler = handlers_.begin(); handler != handlers_.end(); ++handler) {
    if ((*handler)->CheckPolicySettings(policies, errors) && prefs)
      (*handler)->ApplyPolicySettings(policies, prefs);
  }

  for (PolicyMap::const_iterator it = policies.begin();
       it != policies.end();
       ++it) {
    if (IsDeprecatedPolicy(it->first))
      errors->AddError(it->first, IDS_POLICY_DEPRECATED);
  }
}

void ConfigurationPolicyHandlerList::PrepareForDisplaying(
    PolicyMap* policies) const {
  std::vector<ConfigurationPolicyHandler*>::const_iterator handler;
  for (handler = handlers_.begin(); handler != handlers_.end(); ++handler)
    (*handler)->PrepareForDisplaying(policies);
}

}  // namespace policy
