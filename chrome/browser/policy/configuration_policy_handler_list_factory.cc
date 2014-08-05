// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "chrome/browser/net/proxy_policy_handler.h"
#include "chrome/browser/policy/managed_bookmarks_policy_handler.h"
#include "chrome/browser/profiles/incognito_mode_policy_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/policy/core/browser/autofill_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/url_blacklist_policy_handler.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/schema.h"
#include "components/search_engines/default_search_policy_handler.h"
#include "components/translate/core/common/translate_pref_names.h"
#include "grit/components_strings.h"
#include "policy/policy_constants.h"

#if !defined(OS_IOS)
#include "chrome/browser/net/disk_cache_dir_policy_handler.h"
#include "chrome/browser/policy/file_selection_dialogs_policy_handler.h"
#include "chrome/browser/policy/javascript_policy_handler.h"
#include "chrome/browser/policy/network_prediction_policy_handler.h"
#include "chrome/browser/sessions/restore_on_startup_policy_handler.h"
#include "chrome/browser/sync/sync_policy_handler.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/magnifier/magnifier_constants.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/policy/configuration_policy_handler_chromeos.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "components/user_manager/user.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "chrome/browser/download/download_dir_policy_handler.h"
#endif

#if !defined(OS_MACOSX) && !defined(OS_IOS)
#include "apps/pref_names.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/messaging/native_messaging_policy_handler.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/manifest.h"
#endif

namespace policy {

namespace {

// List of policy types to preference names. This is used for simple policies
// that directly map to a single preference.
const PolicyToPreferenceMapEntry kSimplePolicyMap[] = {
  { key::kHomepageLocation,
    prefs::kHomePage,
    base::Value::TYPE_STRING },
  { key::kHomepageIsNewTabPage,
    prefs::kHomePageIsNewTabPage,
    base::Value::TYPE_BOOLEAN },
  { key::kRestoreOnStartupURLs,
    prefs::kURLsToRestoreOnStartup,
    base::Value::TYPE_LIST },
  { key::kAlternateErrorPagesEnabled,
    prefs::kAlternateErrorPagesEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kSearchSuggestEnabled,
    prefs::kSearchSuggestEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kBuiltInDnsClientEnabled,
    prefs::kBuiltInDnsClientEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kWPADQuickCheckEnabled,
    prefs::kQuickCheckEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kDisableSpdy,
    prefs::kDisableSpdy,
    base::Value::TYPE_BOOLEAN },
  { key::kSafeBrowsingEnabled,
    prefs::kSafeBrowsingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kForceSafeSearch,
    prefs::kForceSafeSearch,
    base::Value::TYPE_BOOLEAN },
  { key::kPasswordManagerEnabled,
    password_manager::prefs::kPasswordManagerSavingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kPasswordManagerAllowShowPasswords,
    password_manager::prefs::kPasswordManagerAllowShowPasswords,
    base::Value::TYPE_BOOLEAN },
  { key::kPrintingEnabled,
    prefs::kPrintingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kDisablePrintPreview,
    prefs::kPrintPreviewDisabled,
    base::Value::TYPE_BOOLEAN },
  { key::kMetricsReportingEnabled,
    prefs::kMetricsReportingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kApplicationLocaleValue,
    prefs::kApplicationLocale,
    base::Value::TYPE_STRING },
  { key::kDisabledPlugins,
    prefs::kPluginsDisabledPlugins,
    base::Value::TYPE_LIST },
  { key::kDisabledPluginsExceptions,
    prefs::kPluginsDisabledPluginsExceptions,
    base::Value::TYPE_LIST },
  { key::kEnabledPlugins,
    prefs::kPluginsEnabledPlugins,
    base::Value::TYPE_LIST },
  { key::kShowHomeButton,
    prefs::kShowHomeButton,
    base::Value::TYPE_BOOLEAN },
  { key::kSavingBrowserHistoryDisabled,
    prefs::kSavingBrowserHistoryDisabled,
    base::Value::TYPE_BOOLEAN },
  { key::kAllowDeletingBrowserHistory,
    prefs::kAllowDeletingBrowserHistory,
    base::Value::TYPE_BOOLEAN },
  { key::kDeveloperToolsDisabled,
    prefs::kDevToolsDisabled,
    base::Value::TYPE_BOOLEAN },
  { key::kBlockThirdPartyCookies,
    prefs::kBlockThirdPartyCookies,
    base::Value::TYPE_BOOLEAN },
  { key::kDefaultCookiesSetting,
    prefs::kManagedDefaultCookiesSetting,
    base::Value::TYPE_INTEGER },
  { key::kDefaultImagesSetting,
    prefs::kManagedDefaultImagesSetting,
    base::Value::TYPE_INTEGER },
  { key::kDefaultPluginsSetting,
    prefs::kManagedDefaultPluginsSetting,
    base::Value::TYPE_INTEGER },
  { key::kDefaultPopupsSetting,
    prefs::kManagedDefaultPopupsSetting,
    base::Value::TYPE_INTEGER },
  { key::kAutoSelectCertificateForUrls,
    prefs::kManagedAutoSelectCertificateForUrls,
    base::Value::TYPE_LIST },
  { key::kCookiesAllowedForUrls,
    prefs::kManagedCookiesAllowedForUrls,
    base::Value::TYPE_LIST },
  { key::kCookiesBlockedForUrls,
    prefs::kManagedCookiesBlockedForUrls,
    base::Value::TYPE_LIST },
  { key::kCookiesSessionOnlyForUrls,
    prefs::kManagedCookiesSessionOnlyForUrls,
    base::Value::TYPE_LIST },
  { key::kImagesAllowedForUrls,
    prefs::kManagedImagesAllowedForUrls,
    base::Value::TYPE_LIST },
  { key::kImagesBlockedForUrls,
    prefs::kManagedImagesBlockedForUrls,
    base::Value::TYPE_LIST },
  { key::kJavaScriptAllowedForUrls,
    prefs::kManagedJavaScriptAllowedForUrls,
    base::Value::TYPE_LIST },
  { key::kJavaScriptBlockedForUrls,
    prefs::kManagedJavaScriptBlockedForUrls,
    base::Value::TYPE_LIST },
  { key::kPluginsAllowedForUrls,
    prefs::kManagedPluginsAllowedForUrls,
    base::Value::TYPE_LIST },
  { key::kPluginsBlockedForUrls,
    prefs::kManagedPluginsBlockedForUrls,
    base::Value::TYPE_LIST },
  { key::kPopupsAllowedForUrls,
    prefs::kManagedPopupsAllowedForUrls,
    base::Value::TYPE_LIST },
  { key::kPopupsBlockedForUrls,
    prefs::kManagedPopupsBlockedForUrls,
    base::Value::TYPE_LIST },
  { key::kNotificationsAllowedForUrls,
    prefs::kManagedNotificationsAllowedForUrls,
    base::Value::TYPE_LIST },
  { key::kNotificationsBlockedForUrls,
    prefs::kManagedNotificationsBlockedForUrls,
    base::Value::TYPE_LIST },
  { key::kDefaultNotificationsSetting,
    prefs::kManagedDefaultNotificationsSetting,
    base::Value::TYPE_INTEGER },
  { key::kDefaultGeolocationSetting,
    prefs::kManagedDefaultGeolocationSetting,
    base::Value::TYPE_INTEGER },
  { key::kSigninAllowed,
    prefs::kSigninAllowed,
    base::Value::TYPE_BOOLEAN },
  { key::kDisableSSLRecordSplitting,
    prefs::kDisableSSLRecordSplitting,
    base::Value::TYPE_BOOLEAN },
  { key::kEnableOnlineRevocationChecks,
    prefs::kCertRevocationCheckingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kRequireOnlineRevocationChecksForLocalAnchors,
    prefs::kCertRevocationCheckingRequiredLocalAnchors,
    base::Value::TYPE_BOOLEAN },
  { key::kAuthSchemes,
    prefs::kAuthSchemes,
    base::Value::TYPE_STRING },
  { key::kDisableAuthNegotiateCnameLookup,
    prefs::kDisableAuthNegotiateCnameLookup,
    base::Value::TYPE_BOOLEAN },
  { key::kEnableAuthNegotiatePort,
    prefs::kEnableAuthNegotiatePort,
    base::Value::TYPE_BOOLEAN },
  { key::kAuthServerWhitelist,
    prefs::kAuthServerWhitelist,
    base::Value::TYPE_STRING },
  { key::kAuthNegotiateDelegateWhitelist,
    prefs::kAuthNegotiateDelegateWhitelist,
    base::Value::TYPE_STRING },
  { key::kGSSAPILibraryName,
    prefs::kGSSAPILibraryName,
    base::Value::TYPE_STRING },
  { key::kAllowCrossOriginAuthPrompt,
    prefs::kAllowCrossOriginAuthPrompt,
    base::Value::TYPE_BOOLEAN },
  { key::kDisable3DAPIs,
    prefs::kDisable3DAPIs,
    base::Value::TYPE_BOOLEAN },
  { key::kDisablePluginFinder,
    prefs::kDisablePluginFinder,
    base::Value::TYPE_BOOLEAN },
  { key::kDiskCacheSize,
    prefs::kDiskCacheSize,
    base::Value::TYPE_INTEGER },
  { key::kMediaCacheSize,
    prefs::kMediaCacheSize,
    base::Value::TYPE_INTEGER },
  { key::kPolicyRefreshRate,
    policy_prefs::kUserPolicyRefreshRate,
    base::Value::TYPE_INTEGER },
  { key::kDevicePolicyRefreshRate,
    prefs::kDevicePolicyRefreshRate,
    base::Value::TYPE_INTEGER },
  { key::kDefaultBrowserSettingEnabled,
    prefs::kDefaultBrowserSettingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kRemoteAccessHostFirewallTraversal,
    prefs::kRemoteAccessHostFirewallTraversal,
    base::Value::TYPE_BOOLEAN },
  { key::kRemoteAccessHostRequireTwoFactor,
    prefs::kRemoteAccessHostRequireTwoFactor,
    base::Value::TYPE_BOOLEAN },
  { key::kRemoteAccessHostDomain,
    prefs::kRemoteAccessHostDomain,
    base::Value::TYPE_STRING },
  { key::kRemoteAccessHostTalkGadgetPrefix,
    prefs::kRemoteAccessHostTalkGadgetPrefix,
    base::Value::TYPE_STRING },
  { key::kRemoteAccessHostRequireCurtain,
    prefs::kRemoteAccessHostRequireCurtain,
    base::Value::TYPE_BOOLEAN },
  { key::kRemoteAccessHostAllowClientPairing,
    prefs::kRemoteAccessHostAllowClientPairing,
    base::Value::TYPE_BOOLEAN },
  { key::kRemoteAccessHostAllowGnubbyAuth,
    prefs::kRemoteAccessHostAllowGnubbyAuth,
    base::Value::TYPE_BOOLEAN },
  { key::kRemoteAccessHostAllowRelayedConnection,
    prefs::kRemoteAccessHostAllowRelayedConnection,
    base::Value::TYPE_BOOLEAN },
  { key::kRemoteAccessHostUdpPortRange,
    prefs::kRemoteAccessHostUdpPortRange,
    base::Value::TYPE_STRING },
  { key::kCloudPrintProxyEnabled,
    prefs::kCloudPrintProxyEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kCloudPrintSubmitEnabled,
    prefs::kCloudPrintSubmitEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kTranslateEnabled,
    prefs::kEnableTranslate,
    base::Value::TYPE_BOOLEAN },
  { key::kAllowOutdatedPlugins,
    prefs::kPluginsAllowOutdated,
    base::Value::TYPE_BOOLEAN },
  { key::kAlwaysAuthorizePlugins,
    prefs::kPluginsAlwaysAuthorize,
    base::Value::TYPE_BOOLEAN },
  { key::kBookmarkBarEnabled,
    prefs::kShowBookmarkBar,
    base::Value::TYPE_BOOLEAN },
  { key::kEditBookmarksEnabled,
    prefs::kEditBookmarksEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kShowAppsShortcutInBookmarkBar,
    prefs::kShowAppsShortcutInBookmarkBar,
    base::Value::TYPE_BOOLEAN },
  { key::kAllowFileSelectionDialogs,
    prefs::kAllowFileSelectionDialogs,
    base::Value::TYPE_BOOLEAN },
  { key::kImportBookmarks,
    prefs::kImportBookmarks,
    base::Value::TYPE_BOOLEAN },
  { key::kImportHistory,
    prefs::kImportHistory,
    base::Value::TYPE_BOOLEAN },
  { key::kImportHomepage,
    prefs::kImportHomepage,
    base::Value::TYPE_BOOLEAN },
  { key::kImportSearchEngine,
    prefs::kImportSearchEngine,
    base::Value::TYPE_BOOLEAN },
  { key::kImportSavedPasswords,
    prefs::kImportSavedPasswords,
    base::Value::TYPE_BOOLEAN },
  { key::kMaxConnectionsPerProxy,
    prefs::kMaxConnectionsPerProxy,
    base::Value::TYPE_INTEGER },
  { key::kURLWhitelist,
    policy_prefs::kUrlWhitelist,
    base::Value::TYPE_LIST },
  { key::kRestrictSigninToPattern,
    prefs::kGoogleServicesUsernamePattern,
    base::Value::TYPE_STRING },
  { key::kDefaultMediaStreamSetting,
    prefs::kManagedDefaultMediaStreamSetting,
    base::Value::TYPE_INTEGER },
  { key::kDisableSafeBrowsingProceedAnyway,
    prefs::kSafeBrowsingProceedAnywayDisabled,
    base::Value::TYPE_BOOLEAN },
  { key::kSpellCheckServiceEnabled,
    prefs::kSpellCheckUseSpellingService,
    base::Value::TYPE_BOOLEAN },
  { key::kDisableScreenshots,
    prefs::kDisableScreenshots,
    base::Value::TYPE_BOOLEAN },
  { key::kAudioCaptureAllowed,
    prefs::kAudioCaptureAllowed,
    base::Value::TYPE_BOOLEAN },
  { key::kVideoCaptureAllowed,
    prefs::kVideoCaptureAllowed,
    base::Value::TYPE_BOOLEAN },
  { key::kAudioCaptureAllowedUrls,
    prefs::kAudioCaptureAllowedUrls,
    base::Value::TYPE_LIST },
  { key::kVideoCaptureAllowedUrls,
    prefs::kVideoCaptureAllowedUrls,
    base::Value::TYPE_LIST },
  { key::kHideWebStoreIcon,
    prefs::kHideWebStoreIcon,
    base::Value::TYPE_BOOLEAN },
  { key::kVariationsRestrictParameter,
    prefs::kVariationsRestrictParameter,
    base::Value::TYPE_STRING },
  { key::kSupervisedUserCreationEnabled,
    prefs::kSupervisedUserCreationAllowed,
    base::Value::TYPE_BOOLEAN },
  { key::kForceEphemeralProfiles,
    prefs::kForceEphemeralProfiles,
    base::Value::TYPE_BOOLEAN },

#if !defined(OS_MACOSX) && !defined(OS_IOS)
  { key::kFullscreenAllowed,
    prefs::kFullscreenAllowed,
    base::Value::TYPE_BOOLEAN },
#if defined(ENABLE_EXTENSIONS)
  { key::kFullscreenAllowed,
    apps::prefs::kAppFullscreenAllowed,
    base::Value::TYPE_BOOLEAN },
#endif  // defined(ENABLE_EXTENSIONS)
#endif  // !defined(OS_MACOSX) && !defined(OS_IOS)

#if defined(OS_CHROMEOS)
  { key::kChromeOsLockOnIdleSuspend,
    prefs::kEnableAutoScreenLock,
    base::Value::TYPE_BOOLEAN },
  { key::kChromeOsReleaseChannel,
    prefs::kChromeOsReleaseChannel,
    base::Value::TYPE_STRING },
  { key::kDriveDisabled,
    prefs::kDisableDrive,
    base::Value::TYPE_BOOLEAN },
  { key::kDriveDisabledOverCellular,
    prefs::kDisableDriveOverCellular,
    base::Value::TYPE_BOOLEAN },
  { key::kExternalStorageDisabled,
    prefs::kExternalStorageDisabled,
    base::Value::TYPE_BOOLEAN },
  { key::kAudioOutputAllowed,
    prefs::kAudioOutputAllowed,
    base::Value::TYPE_BOOLEAN },
  { key::kShowLogoutButtonInTray,
    prefs::kShowLogoutButtonInTray,
    base::Value::TYPE_BOOLEAN },
  { key::kShelfAutoHideBehavior,
    prefs::kShelfAutoHideBehaviorLocal,
    base::Value::TYPE_STRING },
  { key::kSessionLengthLimit,
    prefs::kSessionLengthLimit,
    base::Value::TYPE_INTEGER },
  { key::kWaitForInitialUserActivity,
    prefs::kSessionWaitForInitialUserActivity,
    base::Value::TYPE_BOOLEAN },
  { key::kPowerManagementUsesAudioActivity,
    prefs::kPowerUseAudioActivity,
    base::Value::TYPE_BOOLEAN },
  { key::kPowerManagementUsesVideoActivity,
    prefs::kPowerUseVideoActivity,
    base::Value::TYPE_BOOLEAN },
  { key::kAllowScreenWakeLocks,
    prefs::kPowerAllowScreenWakeLocks,
    base::Value::TYPE_BOOLEAN },
  { key::kWaitForInitialUserActivity,
    prefs::kPowerWaitForInitialUserActivity,
    base::Value::TYPE_BOOLEAN },
  { key::kTermsOfServiceURL,
    prefs::kTermsOfServiceURL,
    base::Value::TYPE_STRING },
  { key::kShowAccessibilityOptionsInSystemTrayMenu,
    prefs::kShouldAlwaysShowAccessibilityMenu,
    base::Value::TYPE_BOOLEAN },
  { key::kLargeCursorEnabled,
    prefs::kAccessibilityLargeCursorEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kSpokenFeedbackEnabled,
    prefs::kAccessibilitySpokenFeedbackEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kHighContrastEnabled,
    prefs::kAccessibilityHighContrastEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kVirtualKeyboardEnabled,
    prefs::kAccessibilityVirtualKeyboardEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kDeviceLoginScreenDefaultLargeCursorEnabled,
    NULL,
    base::Value::TYPE_BOOLEAN },
  { key::kDeviceLoginScreenDefaultSpokenFeedbackEnabled,
    NULL,
    base::Value::TYPE_BOOLEAN },
  { key::kDeviceLoginScreenDefaultHighContrastEnabled,
    NULL,
    base::Value::TYPE_BOOLEAN },
  { key::kDeviceLoginScreenDefaultVirtualKeyboardEnabled,
    NULL,
    base::Value::TYPE_BOOLEAN },
  { key::kRebootAfterUpdate,
    prefs::kRebootAfterUpdate,
    base::Value::TYPE_BOOLEAN },
  { key::kAttestationEnabledForUser,
    prefs::kAttestationEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kChromeOsMultiProfileUserBehavior,
    prefs::kMultiProfileUserBehavior,
    base::Value::TYPE_STRING },
  { key::kKeyboardDefaultToFunctionKeys,
    prefs::kLanguageSendFunctionKeys,
    base::Value::TYPE_BOOLEAN },
  { key::kTouchVirtualKeyboardEnabled,
    prefs::kTouchVirtualKeyboardEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kEasyUnlockAllowed,
    prefs::kEasyUnlockAllowed,
    base::Value::TYPE_BOOLEAN },
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  { key::kBackgroundModeEnabled,
    prefs::kBackgroundModeEnabled,
    base::Value::TYPE_BOOLEAN },
#endif  // !defined(OS_MACOSX) && !defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
  { key::kDataCompressionProxyEnabled,
    data_reduction_proxy::prefs::kDataReductionProxyEnabled,
    base::Value::TYPE_BOOLEAN },
#endif  // defined(OS_ANDROID)

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
  { key::kNativeMessagingUserLevelHosts,
    extensions::pref_names::kNativeMessagingUserLevelHosts,
    base::Value::TYPE_BOOLEAN },
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
};

#if defined(ENABLE_EXTENSIONS)
void GetExtensionAllowedTypesMap(
    ScopedVector<StringMappingListPolicyHandler::MappingEntry>* result) {
  // Mapping from extension type names to Manifest::Type.
  result->push_back(new StringMappingListPolicyHandler::MappingEntry(
      "extension", scoped_ptr<base::Value>(new base::FundamentalValue(
          extensions::Manifest::TYPE_EXTENSION))));
  result->push_back(new StringMappingListPolicyHandler::MappingEntry(
      "theme", scoped_ptr<base::Value>(new base::FundamentalValue(
          extensions::Manifest::TYPE_THEME))));
  result->push_back(new StringMappingListPolicyHandler::MappingEntry(
      "user_script", scoped_ptr<base::Value>(new base::FundamentalValue(
          extensions::Manifest::TYPE_USER_SCRIPT))));
  result->push_back(new StringMappingListPolicyHandler::MappingEntry(
      "hosted_app", scoped_ptr<base::Value>(new base::FundamentalValue(
          extensions::Manifest::TYPE_HOSTED_APP))));
  result->push_back(new StringMappingListPolicyHandler::MappingEntry(
      "legacy_packaged_app", scoped_ptr<base::Value>(new base::FundamentalValue(
          extensions::Manifest::TYPE_LEGACY_PACKAGED_APP))));
  result->push_back(new StringMappingListPolicyHandler::MappingEntry(
      "platform_app", scoped_ptr<base::Value>(new base::FundamentalValue(
          extensions::Manifest::TYPE_PLATFORM_APP))));
}
#endif

#if !defined(OS_IOS)
void GetDeprecatedFeaturesMap(
    ScopedVector<StringMappingListPolicyHandler::MappingEntry>* result) {
  // Maps feature tags as specified in policy to the corresponding switch to
  // re-enable them.
  // TODO: Remove after 2015-04-30 per http://crbug.com/374782.
  result->push_back(new StringMappingListPolicyHandler::MappingEntry(
      "ShowModalDialog_EffectiveUntil20150430",
      scoped_ptr<base::Value>(new base::StringValue(
          switches::kEnableShowModalDialog))));
}
#endif  // !defined(OS_IOS)

}  // namespace

void PopulatePolicyHandlerParameters(PolicyHandlerParameters* parameters) {
#if defined(OS_CHROMEOS)
  if (chromeos::UserManager::IsInitialized()) {
    const user_manager::User* user =
        chromeos::UserManager::Get()->GetActiveUser();
    if (user)
      parameters->user_id_hash = user->username_hash();
  }
#endif
}

scoped_ptr<ConfigurationPolicyHandlerList> BuildHandlerList(
    const Schema& chrome_schema) {
  scoped_ptr<ConfigurationPolicyHandlerList> handlers(
      new ConfigurationPolicyHandlerList(
          base::Bind(&PopulatePolicyHandlerParameters),
          base::Bind(&GetChromePolicyDetails)));
  for (size_t i = 0; i < arraysize(kSimplePolicyMap); ++i) {
    handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
        new SimplePolicyHandler(kSimplePolicyMap[i].policy_name,
                                kSimplePolicyMap[i].preference_path,
                                kSimplePolicyMap[i].value_type)));
  }

  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new AutofillPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new DefaultSearchPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IncognitoModePolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new ManagedBookmarksPolicyHandler(chrome_schema)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new ProxyPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new URLBlacklistPolicyHandler()));

#if !defined(OS_IOS)
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new FileSelectionDialogsPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new JavascriptPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new NetworkPredictionPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new RestoreOnStartupPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new browser_sync::SyncPolicyHandler()));

  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new StringMappingListPolicyHandler(
          key::kEnableDeprecatedWebPlatformFeatures,
          prefs::kEnableDeprecatedWebPlatformFeatures,
          base::Bind(GetDeprecatedFeaturesMap))));
#endif  // !defined(OS_IOS)

#if defined(ENABLE_EXTENSIONS)
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new extensions::ExtensionListPolicyHandler(
          key::kExtensionInstallWhitelist,
          extensions::pref_names::kInstallAllowList,
          false)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new extensions::ExtensionListPolicyHandler(
          key::kExtensionInstallBlacklist,
          extensions::pref_names::kInstallDenyList,
          true)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new extensions::ExtensionInstallForcelistPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new extensions::ExtensionURLPatternListPolicyHandler(
          key::kExtensionInstallSources,
          extensions::pref_names::kAllowedInstallSites)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new StringMappingListPolicyHandler(
          key::kExtensionAllowedTypes,
          extensions::pref_names::kAllowedTypes,
          base::Bind(GetExtensionAllowedTypesMap))));
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new DiskCacheDirPolicyHandler()));

  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new extensions::NativeMessagingHostListPolicyHandler(
          key::kNativeMessagingWhitelist,
          extensions::pref_names::kNativeMessagingWhitelist,
          false)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new extensions::NativeMessagingHostListPolicyHandler(
          key::kNativeMessagingBlacklist,
          extensions::pref_names::kNativeMessagingBlacklist,
          true)));
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new DownloadDirPolicyHandler));

  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new SimpleSchemaValidatingPolicyHandler(
          key::kRegisteredProtocolHandlers,
          prefs::kPolicyRegisteredProtocolHandlers,
          chrome_schema,
          SCHEMA_STRICT,
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_PROHIBITED)));
#endif

#if defined(OS_CHROMEOS)
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new extensions::ExtensionListPolicyHandler(
          key::kAttestationExtensionWhitelist,
          prefs::kAttestationExtensionWhitelist,
          false)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      NetworkConfigurationPolicyHandler::CreateForDevicePolicy()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new PinnedLauncherAppsPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new ScreenMagnifierPolicyHandler()));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new LoginScreenPowerManagementPolicyHandler(chrome_schema)));

  ScopedVector<ConfigurationPolicyHandler>
      power_management_idle_legacy_policies;
  power_management_idle_legacy_policies.push_back(
      new IntRangePolicyHandler(key::kScreenDimDelayAC,
                                prefs::kPowerAcScreenDimDelayMs,
                                0,
                                INT_MAX,
                                true));
  power_management_idle_legacy_policies.push_back(
      new IntRangePolicyHandler(key::kScreenOffDelayAC,
                                prefs::kPowerAcScreenOffDelayMs,
                                0,
                                INT_MAX,
                                true));
  power_management_idle_legacy_policies.push_back(
      new IntRangePolicyHandler(key::kIdleWarningDelayAC,
                                prefs::kPowerAcIdleWarningDelayMs,
                                0,
                                INT_MAX,
                                true));
  power_management_idle_legacy_policies.push_back(new IntRangePolicyHandler(
      key::kIdleDelayAC, prefs::kPowerAcIdleDelayMs, 0, INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      new IntRangePolicyHandler(key::kScreenDimDelayBattery,
                                prefs::kPowerBatteryScreenDimDelayMs,
                                0,
                                INT_MAX,
                                true));
  power_management_idle_legacy_policies.push_back(
      new IntRangePolicyHandler(key::kScreenOffDelayBattery,
                                prefs::kPowerBatteryScreenOffDelayMs,
                                0,
                                INT_MAX,
                                true));
  power_management_idle_legacy_policies.push_back(
      new IntRangePolicyHandler(key::kIdleWarningDelayBattery,
                                prefs::kPowerBatteryIdleWarningDelayMs,
                                0,
                                INT_MAX,
                                true));
  power_management_idle_legacy_policies.push_back(
      new IntRangePolicyHandler(key::kIdleDelayBattery,
                                prefs::kPowerBatteryIdleDelayMs,
                                0,
                                INT_MAX,
                                true));
  power_management_idle_legacy_policies.push_back(new IntRangePolicyHandler(
      key::kIdleActionAC,
      prefs::kPowerAcIdleAction,
      chromeos::PowerPolicyController::ACTION_SUSPEND,
      chromeos::PowerPolicyController::ACTION_DO_NOTHING,
      false));
  power_management_idle_legacy_policies.push_back(new IntRangePolicyHandler(
      key::kIdleActionBattery,
      prefs::kPowerBatteryIdleAction,
      chromeos::PowerPolicyController::ACTION_SUSPEND,
      chromeos::PowerPolicyController::ACTION_DO_NOTHING,
      false));
  power_management_idle_legacy_policies.push_back(
      new DeprecatedIdleActionHandler());

  ScopedVector<ConfigurationPolicyHandler> screen_lock_legacy_policies;
  screen_lock_legacy_policies.push_back(
      new IntRangePolicyHandler(key::kScreenLockDelayAC,
                                prefs::kPowerAcScreenLockDelayMs,
                                0,
                                INT_MAX,
                                true));
  screen_lock_legacy_policies.push_back(
      new IntRangePolicyHandler(key::kScreenLockDelayBattery,
                                prefs::kPowerBatteryScreenLockDelayMs,
                                0,
                                INT_MAX,
                                true));

  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntRangePolicyHandler(key::kSAMLOfflineSigninTimeLimit,
                                prefs::kSAMLOfflineSigninTimeLimit,
                                -1,
                                INT_MAX,
                                true)));
  handlers->AddHandler(
      make_scoped_ptr<ConfigurationPolicyHandler>(new IntRangePolicyHandler(
          key::kLidCloseAction,
          prefs::kPowerLidClosedAction,
          chromeos::PowerPolicyController::ACTION_SUSPEND,
          chromeos::PowerPolicyController::ACTION_DO_NOTHING,
          false)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntPercentageToDoublePolicyHandler(
          key::kPresentationScreenDimDelayScale,
          prefs::kPowerPresentationScreenDimDelayFactor,
          100,
          INT_MAX,
          true)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new IntPercentageToDoublePolicyHandler(
          key::kUserActivityScreenDimDelayScale,
          prefs::kPowerUserActivityScreenDimDelayFactor,
          100,
          INT_MAX,
          true)));
  handlers->AddHandler(
      make_scoped_ptr<ConfigurationPolicyHandler>(new IntRangePolicyHandler(
          key::kUptimeLimit, prefs::kUptimeLimit, 3600, INT_MAX, true)));
  handlers->AddHandler(
      make_scoped_ptr<ConfigurationPolicyHandler>(new IntRangePolicyHandler(
          key::kDeviceLoginScreenDefaultScreenMagnifierType,
          NULL,
          0,
          ash::MAGNIFIER_FULL,
          false)));
  // TODO(binjin): Remove LegacyPoliciesDeprecatingPolicyHandler for these two
  // policies once deprecation of legacy power management policies is done.
  // http://crbug.com/346229
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new LegacyPoliciesDeprecatingPolicyHandler(
          power_management_idle_legacy_policies.Pass(),
          make_scoped_ptr<SchemaValidatingPolicyHandler>(
              new PowerManagementIdleSettingsPolicyHandler(chrome_schema)))));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new LegacyPoliciesDeprecatingPolicyHandler(
          screen_lock_legacy_policies.Pass(),
          make_scoped_ptr<SchemaValidatingPolicyHandler>(
              new ScreenLockDelayPolicyHandler(chrome_schema)))));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new ExternalDataPolicyHandler(key::kUserAvatarImage)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new ExternalDataPolicyHandler(key::kWallpaperImage)));
  handlers->AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
      new SimpleSchemaValidatingPolicyHandler(
          key::kSessionLocales,
          NULL,
          chrome_schema,
          SCHEMA_STRICT,
          SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
          SimpleSchemaValidatingPolicyHandler::MANDATORY_PROHIBITED)));
#endif  // defined(OS_CHROMEOS)

  return handlers.Pass();
}

}  // namespace policy
