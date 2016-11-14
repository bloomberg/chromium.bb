// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"

#include <limits.h>
#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/net/disk_cache_dir_policy_handler.h"
#include "chrome/browser/net/safe_search_util.h"
#include "chrome/browser/policy/file_selection_dialogs_policy_handler.h"
#include "chrome/browser/policy/javascript_policy_handler.h"
#include "chrome/browser/policy/managed_bookmarks_policy_handler.h"
#include "chrome/browser/policy/network_prediction_policy_handler.h"
#include "chrome/browser/profiles/incognito_mode_policy_handler.h"
#include "chrome/browser/sessions/restore_on_startup_policy_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/policy/core/browser/autofill_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/proxy_policy_handler.h"
#include "components/policy/core/browser/url_blacklist_policy_handler.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/default_search_policy_handler.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "components/ssl_config/ssl_config_prefs.h"
#include "components/sync/driver/sync_policy_handler.h"
#include "components/translate/core/common/translate_pref_names.h"
#include "components/variations/pref_names.h"
#include "extensions/features/features.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/search/contextual_search_policy_handler_android.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/common/accessibility_types.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions_policy_handler.h"
#include "chrome/browser/chromeos/policy/configuration_policy_handler_chromeos.h"
#include "chrome/browser/policy/default_geolocation_policy_handler.h"
#include "chromeos/chromeos_pref_names.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "components/drive/drive_pref_names.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/download/download_dir_policy_handler.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/messaging/native_messaging_policy_handler.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/manifest.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_policy_handler.h"
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/browser/pref_names.h"
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
  { key::kPacHttpsUrlStrippingEnabled,
    prefs::kPacHttpsUrlStrippingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kSafeBrowsingEnabled,
    prefs::kSafeBrowsingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kForceGoogleSafeSearch,
    prefs::kForceGoogleSafeSearch,
    base::Value::TYPE_BOOLEAN },
  { key::kForceYouTubeRestrict,
    prefs::kForceYouTubeRestrict,
    base::Value::TYPE_INTEGER},
  { key::kPasswordManagerEnabled,
    password_manager::prefs::kPasswordManagerSavingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kPrintingEnabled,
    prefs::kPrintingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kDisablePrintPreview,
    prefs::kPrintPreviewDisabled,
    base::Value::TYPE_BOOLEAN },
  { key::kDefaultPrinterSelection,
    prefs::kPrintPreviewDefaultDestinationSelectionRules,
    base::Value::TYPE_STRING },
  { key::kApplicationLocaleValue,
    prefs::kApplicationLocale,
    base::Value::TYPE_STRING },
  { key::kAlwaysOpenPdfExternally,
    prefs::kPluginsAlwaysOpenPdfExternally,
    base::Value::TYPE_BOOLEAN },
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
  { key::kDefaultKeygenSetting,
    prefs::kManagedDefaultKeygenSetting,
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
  { key::kKeygenAllowedForUrls,
    prefs::kManagedKeygenAllowedForUrls,
    base::Value::TYPE_LIST },
  { key::kKeygenBlockedForUrls,
    prefs::kManagedKeygenBlockedForUrls,
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
  { key::kEnableOnlineRevocationChecks,
    ssl_config::prefs::kCertRevocationCheckingEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kRequireOnlineRevocationChecksForLocalAnchors,
    ssl_config::prefs::kCertRevocationCheckingRequiredLocalAnchors,
    base::Value::TYPE_BOOLEAN },
  { key::kEnableSha1ForLocalAnchors,
    ssl_config::prefs::kCertEnableSha1LocalAnchors,
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
    bookmarks::prefs::kShowBookmarkBar,
    base::Value::TYPE_BOOLEAN },
  { key::kEditBookmarksEnabled,
    bookmarks::prefs::kEditBookmarksEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kShowAppsShortcutInBookmarkBar,
    bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
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
  { key::kImportAutofillFormData,
    prefs::kImportAutofillFormData,
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
  { key::kDefaultWebBluetoothGuardSetting,
    prefs::kManagedDefaultWebBluetoothGuardSetting,
    base::Value::TYPE_INTEGER },
  { key::kDefaultMediaStreamSetting,
    prefs::kManagedDefaultMediaStreamSetting,
    base::Value::TYPE_INTEGER },
  { key::kDisableSafeBrowsingProceedAnyway,
    prefs::kSafeBrowsingProceedAnywayDisabled,
    base::Value::TYPE_BOOLEAN },
  { key::kSafeBrowsingExtendedReportingOptInAllowed,
    prefs::kSafeBrowsingExtendedReportingOptInAllowed,
    base::Value::TYPE_BOOLEAN },
  { key::kSSLErrorOverrideAllowed,
    prefs::kSSLErrorOverrideAllowed,
    base::Value::TYPE_BOOLEAN },
  { key::kHardwareAccelerationModeEnabled,
    prefs::kHardwareAccelerationModeEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kAllowDinosaurEasterEgg,
    prefs::kAllowDinosaurEasterEgg,
    base::Value::TYPE_BOOLEAN },
  { key::kAllowedDomainsForApps,
    prefs::kAllowedDomainsForApps,
    base::Value::TYPE_STRING },
  { key::kComponentUpdatesEnabled,
    prefs::kComponentUpdatesEnabled,
    base::Value::TYPE_BOOLEAN },

#if BUILDFLAG(ENABLE_SPELLCHECK)
  { key::kSpellCheckServiceEnabled,
    spellcheck::prefs::kSpellCheckUseSpellingService,
    base::Value::TYPE_BOOLEAN },
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

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
    variations::prefs::kVariationsRestrictParameter,
    base::Value::TYPE_STRING },
  { key::kSupervisedUserCreationEnabled,
    prefs::kSupervisedUserCreationAllowed,
    base::Value::TYPE_BOOLEAN },
  { key::kForceEphemeralProfiles,
    prefs::kForceEphemeralProfiles,
    base::Value::TYPE_BOOLEAN },
  { key::kDHEEnabled,
    ssl_config::prefs::kDHEEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kNTPContentSuggestionsEnabled,
    ntp_snippets::prefs::kEnableSnippets,
    base::Value::TYPE_BOOLEAN },
#if defined(ENABLE_MEDIA_ROUTER)
  { key::kEnableMediaRouter,
    prefs::kEnableMediaRouter,
    base::Value::TYPE_BOOLEAN },
#endif  // defined(ENABLE_MEDIA_ROUTER)
#if defined(ENABLE_WEBRTC)
  { key::kWebRtcUdpPortRange,
    prefs::kWebRTCUDPPortRange,
    base::Value::TYPE_STRING },
#endif  // defined(ENABLE_WEBRTC)
#if !defined(OS_MACOSX)
  { key::kFullscreenAllowed,
    prefs::kFullscreenAllowed,
    base::Value::TYPE_BOOLEAN },
#if BUILDFLAG(ENABLE_EXTENSIONS)
  { key::kFullscreenAllowed,
    extensions::pref_names::kAppFullscreenAllowed,
    base::Value::TYPE_BOOLEAN },
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // !defined(OS_MACOSX)

#if defined(OS_CHROMEOS)
  { key::kChromeOsLockOnIdleSuspend,
    prefs::kEnableAutoScreenLock,
    base::Value::TYPE_BOOLEAN },
  { key::kChromeOsReleaseChannel,
    prefs::kChromeOsReleaseChannel,
    base::Value::TYPE_STRING },
  { key::kDriveDisabled,
    drive::prefs::kDisableDrive,
    base::Value::TYPE_BOOLEAN },
  { key::kDriveDisabledOverCellular,
    drive::prefs::kDisableDriveOverCellular,
    base::Value::TYPE_BOOLEAN },
  { key::kExternalStorageDisabled,
    prefs::kExternalStorageDisabled,
    base::Value::TYPE_BOOLEAN },
  { key::kExternalStorageReadOnly,
    prefs::kExternalStorageReadOnly,
    base::Value::TYPE_BOOLEAN },
  { key::kAudioOutputAllowed,
    chromeos::prefs::kAudioOutputAllowed,
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
  { key::kCaptivePortalAuthenticationIgnoresProxy,
    prefs::kCaptivePortalAuthenticationIgnoresProxy,
    base::Value::TYPE_BOOLEAN },
  { key::kForceMaximizeOnFirstRun,
    prefs::kForceMaximizeOnFirstRun,
    base::Value::TYPE_BOOLEAN },
  { key::kUnifiedDesktopEnabledByDefault,
    prefs::kUnifiedDesktopEnabledByDefault,
    base::Value::TYPE_BOOLEAN },
  { key::kArcEnabled,
    prefs::kArcEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kArcBackupRestoreEnabled,
    prefs::kArcBackupRestoreEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kReportArcStatusEnabled,
    prefs::kReportArcStatusEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kNativePrinters,
    prefs::kRecommendedNativePrinters,
    base::Value::TYPE_LIST },
#endif  // defined(OS_CHROMEOS)

// Metrics reporting is controlled by a platform specific policy for ChromeOS
#if defined(OS_CHROMEOS)
  { key::kDeviceMetricsReportingEnabled,
    metrics::prefs::kMetricsReportingEnabled,
    base::Value::TYPE_BOOLEAN },
#else
  { key::kMetricsReportingEnabled,
    metrics::prefs::kMetricsReportingEnabled,
    base::Value::TYPE_BOOLEAN },
#endif

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  { key::kBackgroundModeEnabled,
    prefs::kBackgroundModeEnabled,
    base::Value::TYPE_BOOLEAN },
#endif  // !defined(OS_MACOSX) && !defined(OS_CHROMEOS)

#if BUILDFLAG(ANDROID_JAVA_UI)
  { key::kDataCompressionProxyEnabled,
    prefs::kDataSaverEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kAuthAndroidNegotiateAccountType,
    prefs::kAuthAndroidNegotiateAccountType,
    base::Value::TYPE_STRING },
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  { key::kNativeMessagingUserLevelHosts,
    extensions::pref_names::kNativeMessagingUserLevelHosts,
    base::Value::TYPE_BOOLEAN },
  { key::kBrowserGuestModeEnabled,
    prefs::kBrowserGuestModeEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kBrowserAddPersonEnabled,
    prefs::kBrowserAddPersonEnabled,
    base::Value::TYPE_BOOLEAN },
  { key::kForceBrowserSignin,
    prefs::kForceBrowserSignin,
    base::Value::TYPE_BOOLEAN },
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

#if defined(OS_WIN)
  { key::kWelcomePageOnOSUpgradeEnabled,
    prefs::kWelcomePageOnOSUpgradeEnabled,
    base::Value::TYPE_BOOLEAN },
#endif  // OS_WIN

#if !defined(OS_ANDROID)
  { key::kSuppressUnsupportedOSWarning,
    prefs::kSuppressUnsupportedOSWarning,
    base::Value::TYPE_BOOLEAN },
#endif  // !OS_ANDROID

#if defined(OS_CHROMEOS)
  { key::kSystemTimezoneAutomaticDetection,
    prefs::kSystemTimezoneAutomaticDetectionPolicy,
    base::Value::TYPE_INTEGER },
#endif

  { key::kTaskManagerEndProcessEnabled,
    prefs::kTaskManagerEndProcessEnabled,
    base::Value::TYPE_BOOLEAN },

#if defined(OS_CHROMEOS)
  { key::kNetworkThrottlingEnabled,
    prefs::kNetworkThrottlingEnabled,
    base::Value::TYPE_DICTIONARY },

  { key::kAllowScreenLock, prefs::kAllowScreenLock, base::Value::TYPE_BOOLEAN },

  { key::kQuickUnlockModeWhitelist, prefs::kQuickUnlockModeWhitelist,
    base::Value::TYPE_LIST },
  { key::kQuickUnlockTimeout, prefs::kQuickUnlockTimeout,
    base::Value::TYPE_INTEGER },
#endif
};

class ForceSafeSearchPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  ForceSafeSearchPolicyHandler()
      : TypeCheckingPolicyHandler(key::kForceSafeSearch,
                                  base::Value::TYPE_BOOLEAN) {}
  ~ForceSafeSearchPolicyHandler() override {}

  // ConfigurationPolicyHandler implementation:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override {
    // If either of the new ForceGoogleSafeSearch, ForceYouTubeSafetyMode or
    // ForceYouTubeRestrict policies are defined, then this one should be
    // ignored. crbug.com/476908, crbug.com/590478
    // Note: Those policies are declared in kSimplePolicyMap above, except
    // ForceYouTubeSafetyMode, which has been replaced by ForceYouTubeRestrict.
    if (policies.GetValue(key::kForceGoogleSafeSearch) ||
        policies.GetValue(key::kForceYouTubeSafetyMode) ||
        policies.GetValue(key::kForceYouTubeRestrict)) {
      return;
    }
    const base::Value* value = policies.GetValue(policy_name());
    if (value) {
      bool enabled;
      prefs->SetValue(prefs::kForceGoogleSafeSearch, value->CreateDeepCopy());

      // Note that ForceYouTubeRestrict is an int policy, we cannot simply deep
      // copy value, which is a boolean.
      if (value->GetAsBoolean(&enabled)) {
        prefs->SetValue(
            prefs::kForceYouTubeRestrict,
            base::MakeUnique<base::FundamentalValue>(
                enabled ? safe_search_util::YOUTUBE_RESTRICT_MODERATE
                        : safe_search_util::YOUTUBE_RESTRICT_OFF));
      }
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ForceSafeSearchPolicyHandler);
};

class ForceYouTubeSafetyModePolicyHandler : public TypeCheckingPolicyHandler {
 public:
  ForceYouTubeSafetyModePolicyHandler()
      : TypeCheckingPolicyHandler(key::kForceYouTubeSafetyMode,
                                  base::Value::TYPE_BOOLEAN) {}
  ~ForceYouTubeSafetyModePolicyHandler() override {}

  // ConfigurationPolicyHandler implementation:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override {
    // If only the deprecated ForceYouTubeSafetyMode policy is set,
    // but not ForceYouTubeRestrict, set ForceYouTubeRestrict to Moderate.
    if (policies.GetValue(key::kForceYouTubeRestrict))
      return;

    const base::Value* value = policies.GetValue(policy_name());
    bool enabled;
    if (value && value->GetAsBoolean(&enabled)) {
      prefs->SetValue(
          prefs::kForceYouTubeRestrict,
          base::MakeUnique<base::FundamentalValue>(
              enabled ? safe_search_util::YOUTUBE_RESTRICT_MODERATE
                      : safe_search_util::YOUTUBE_RESTRICT_OFF));
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ForceYouTubeSafetyModePolicyHandler);
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
void GetExtensionAllowedTypesMap(
    std::vector<std::unique_ptr<StringMappingListPolicyHandler::MappingEntry>>*
        result) {
  // Mapping from extension type names to Manifest::Type.
  for (size_t index = 0;
       index < extensions::schema_constants::kAllowedTypesMapSize;
       ++index) {
    const extensions::schema_constants::AllowedTypesMapEntry& entry =
        extensions::schema_constants::kAllowedTypesMap[index];
    result->push_back(
        base::MakeUnique<StringMappingListPolicyHandler::MappingEntry>(
            entry.name, std::unique_ptr<base::Value>(
                            new base::FundamentalValue(entry.manifest_type))));
  }
}
#endif

void GetDeprecatedFeaturesMap(
    std::vector<std::unique_ptr<StringMappingListPolicyHandler::MappingEntry>>*
        result) {
  // Maps feature tags as specified in policy to the corresponding switch to
  // re-enable them.
}

}  // namespace

void PopulatePolicyHandlerParameters(PolicyHandlerParameters* parameters) {
#if defined(OS_CHROMEOS)
  if (user_manager::UserManager::IsInitialized()) {
    const user_manager::User* user =
        user_manager::UserManager::Get()->GetActiveUser();
    if (user)
      parameters->user_id_hash = user->username_hash();
  }
#endif
}

std::unique_ptr<ConfigurationPolicyHandlerList> BuildHandlerList(
    const Schema& chrome_schema) {
  std::unique_ptr<ConfigurationPolicyHandlerList> handlers(
      new ConfigurationPolicyHandlerList(
          base::Bind(&PopulatePolicyHandlerParameters),
          base::Bind(&GetChromePolicyDetails)));
  for (size_t i = 0; i < arraysize(kSimplePolicyMap); ++i) {
    handlers->AddHandler(base::MakeUnique<SimplePolicyHandler>(
        kSimplePolicyMap[i].policy_name, kSimplePolicyMap[i].preference_path,
        kSimplePolicyMap[i].value_type));
  }

  handlers->AddHandler(base::MakeUnique<AutofillPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<DefaultSearchPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<ForceSafeSearchPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<ForceYouTubeSafetyModePolicyHandler>());
  handlers->AddHandler(base::MakeUnique<IncognitoModePolicyHandler>());
  handlers->AddHandler(
      base::MakeUnique<ManagedBookmarksPolicyHandler>(chrome_schema));
  handlers->AddHandler(base::MakeUnique<ProxyPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<URLBlacklistPolicyHandler>());

  handlers->AddHandler(base::MakeUnique<SimpleSchemaValidatingPolicyHandler>(
      key::kCertificateTransparencyEnforcementDisabledForUrls,
      certificate_transparency::prefs::kCTExcludedHosts, chrome_schema,
      SCHEMA_STRICT, SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));

#if BUILDFLAG(ANDROID_JAVA_UI)
  handlers->AddHandler(
      base::MakeUnique<ContextualSearchPolicyHandlerAndroid>());
#endif

  handlers->AddHandler(base::MakeUnique<FileSelectionDialogsPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<JavascriptPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<NetworkPredictionPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<RestoreOnStartupPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<syncer::SyncPolicyHandler>());

  handlers->AddHandler(base::MakeUnique<StringMappingListPolicyHandler>(
      key::kEnableDeprecatedWebPlatformFeatures,
      prefs::kEnableDeprecatedWebPlatformFeatures,
      base::Bind(GetDeprecatedFeaturesMap)));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  handlers->AddHandler(base::MakeUnique<extensions::ExtensionListPolicyHandler>(
      key::kExtensionInstallWhitelist,
      extensions::pref_names::kInstallAllowList, false));
  handlers->AddHandler(base::MakeUnique<extensions::ExtensionListPolicyHandler>(
      key::kExtensionInstallBlacklist, extensions::pref_names::kInstallDenyList,
      true));
  handlers->AddHandler(
      base::MakeUnique<extensions::ExtensionInstallForcelistPolicyHandler>());
  handlers->AddHandler(
      base::MakeUnique<extensions::ExtensionURLPatternListPolicyHandler>(
          key::kExtensionInstallSources,
          extensions::pref_names::kAllowedInstallSites));
  handlers->AddHandler(base::MakeUnique<StringMappingListPolicyHandler>(
      key::kExtensionAllowedTypes, extensions::pref_names::kAllowedTypes,
      base::Bind(GetExtensionAllowedTypesMap)));
  handlers->AddHandler(
      base::MakeUnique<extensions::ExtensionSettingsPolicyHandler>(
          chrome_schema));
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  handlers->AddHandler(base::MakeUnique<DiskCacheDirPolicyHandler>());

  handlers->AddHandler(
      base::MakeUnique<extensions::NativeMessagingHostListPolicyHandler>(
          key::kNativeMessagingWhitelist,
          extensions::pref_names::kNativeMessagingWhitelist, false));
  handlers->AddHandler(
      base::MakeUnique<extensions::NativeMessagingHostListPolicyHandler>(
          key::kNativeMessagingBlacklist,
          extensions::pref_names::kNativeMessagingBlacklist, true));
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
  handlers->AddHandler(base::WrapUnique(new DownloadDirPolicyHandler));

  handlers->AddHandler(base::MakeUnique<SimpleSchemaValidatingPolicyHandler>(
      key::kRegisteredProtocolHandlers,
      prefs::kPolicyRegisteredProtocolHandlers, chrome_schema, SCHEMA_STRICT,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_PROHIBITED));
#endif

#if defined(OS_CHROMEOS)
  handlers->AddHandler(base::MakeUnique<extensions::ExtensionListPolicyHandler>(
      key::kAttestationExtensionWhitelist,
      prefs::kAttestationExtensionWhitelist, false));
  handlers->AddHandler(base::WrapUnique(
      NetworkConfigurationPolicyHandler::CreateForDevicePolicy()));
  handlers->AddHandler(base::WrapUnique(
      NetworkConfigurationPolicyHandler::CreateForUserPolicy()));
  handlers->AddHandler(base::MakeUnique<PinnedLauncherAppsPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<ScreenMagnifierPolicyHandler>());
  handlers->AddHandler(
      base::MakeUnique<LoginScreenPowerManagementPolicyHandler>(chrome_schema));

  std::vector<std::unique_ptr<ConfigurationPolicyHandler>>
      power_management_idle_legacy_policies;
  power_management_idle_legacy_policies.push_back(
      base::MakeUnique<IntRangePolicyHandler>(key::kScreenDimDelayAC,
                                              prefs::kPowerAcScreenDimDelayMs,
                                              0, INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      base::MakeUnique<IntRangePolicyHandler>(key::kScreenOffDelayAC,
                                              prefs::kPowerAcScreenOffDelayMs,
                                              0, INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      base::MakeUnique<IntRangePolicyHandler>(key::kIdleWarningDelayAC,
                                              prefs::kPowerAcIdleWarningDelayMs,
                                              0, INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      base::MakeUnique<IntRangePolicyHandler>(
          key::kIdleDelayAC, prefs::kPowerAcIdleDelayMs, 0, INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      base::MakeUnique<IntRangePolicyHandler>(
          key::kScreenDimDelayBattery, prefs::kPowerBatteryScreenDimDelayMs, 0,
          INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      base::MakeUnique<IntRangePolicyHandler>(
          key::kScreenOffDelayBattery, prefs::kPowerBatteryScreenOffDelayMs, 0,
          INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      base::MakeUnique<IntRangePolicyHandler>(
          key::kIdleWarningDelayBattery, prefs::kPowerBatteryIdleWarningDelayMs,
          0, INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      base::MakeUnique<IntRangePolicyHandler>(key::kIdleDelayBattery,
                                              prefs::kPowerBatteryIdleDelayMs,
                                              0, INT_MAX, true));
  power_management_idle_legacy_policies.push_back(
      base::MakeUnique<IntRangePolicyHandler>(
          key::kIdleActionAC, prefs::kPowerAcIdleAction,
          chromeos::PowerPolicyController::ACTION_SUSPEND,
          chromeos::PowerPolicyController::ACTION_DO_NOTHING, false));
  power_management_idle_legacy_policies.push_back(
      base::MakeUnique<IntRangePolicyHandler>(
          key::kIdleActionBattery, prefs::kPowerBatteryIdleAction,
          chromeos::PowerPolicyController::ACTION_SUSPEND,
          chromeos::PowerPolicyController::ACTION_DO_NOTHING, false));
  power_management_idle_legacy_policies.push_back(
      base::MakeUnique<DeprecatedIdleActionHandler>());

  std::vector<std::unique_ptr<ConfigurationPolicyHandler>>
      screen_lock_legacy_policies;
  screen_lock_legacy_policies.push_back(base::MakeUnique<IntRangePolicyHandler>(
      key::kScreenLockDelayAC, prefs::kPowerAcScreenLockDelayMs, 0, INT_MAX,
      true));
  screen_lock_legacy_policies.push_back(base::MakeUnique<IntRangePolicyHandler>(
      key::kScreenLockDelayBattery, prefs::kPowerBatteryScreenLockDelayMs, 0,
      INT_MAX, true));

  handlers->AddHandler(base::MakeUnique<IntRangePolicyHandler>(
      key::kSAMLOfflineSigninTimeLimit, prefs::kSAMLOfflineSigninTimeLimit, -1,
      INT_MAX, true));
  handlers->AddHandler(base::MakeUnique<IntRangePolicyHandler>(
      key::kLidCloseAction, prefs::kPowerLidClosedAction,
      chromeos::PowerPolicyController::ACTION_SUSPEND,
      chromeos::PowerPolicyController::ACTION_DO_NOTHING, false));
  handlers->AddHandler(base::MakeUnique<IntPercentageToDoublePolicyHandler>(
      key::kPresentationScreenDimDelayScale,
      prefs::kPowerPresentationScreenDimDelayFactor, 100, INT_MAX, true));
  handlers->AddHandler(base::MakeUnique<IntPercentageToDoublePolicyHandler>(
      key::kUserActivityScreenDimDelayScale,
      prefs::kPowerUserActivityScreenDimDelayFactor, 100, INT_MAX, true));
  handlers->AddHandler(base::MakeUnique<IntRangePolicyHandler>(
      key::kUptimeLimit, prefs::kUptimeLimit, 3600, INT_MAX, true));
  handlers->AddHandler(base::WrapUnique(new IntRangePolicyHandler(
      key::kDeviceLoginScreenDefaultScreenMagnifierType, NULL, 0,
      ash::MAGNIFIER_FULL, false)));
  // TODO(binjin): Remove LegacyPoliciesDeprecatingPolicyHandler for these two
  // policies once deprecation of legacy power management policies is done.
  // http://crbug.com/346229
  handlers->AddHandler(base::MakeUnique<LegacyPoliciesDeprecatingPolicyHandler>(
      std::move(power_management_idle_legacy_policies),
      base::WrapUnique(
          new PowerManagementIdleSettingsPolicyHandler(chrome_schema))));
  handlers->AddHandler(base::MakeUnique<LegacyPoliciesDeprecatingPolicyHandler>(
      std::move(screen_lock_legacy_policies),
      base::WrapUnique(new ScreenLockDelayPolicyHandler(chrome_schema))));
  handlers->AddHandler(
      base::MakeUnique<ExternalDataPolicyHandler>(key::kUserAvatarImage));
  handlers->AddHandler(
      base::MakeUnique<ExternalDataPolicyHandler>(key::kWallpaperImage));
  handlers->AddHandler(base::WrapUnique(new SimpleSchemaValidatingPolicyHandler(
      key::kSessionLocales, NULL, chrome_schema, SCHEMA_STRICT,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_PROHIBITED)));
  handlers->AddHandler(
      base::MakeUnique<chromeos::KeyPermissionsPolicyHandler>(chrome_schema));
  handlers->AddHandler(base::WrapUnique(new DefaultGeolocationPolicyHandler()));
#endif  // defined(OS_CHROMEOS)

#if defined(ENABLE_PLUGINS)
  handlers->AddHandler(base::MakeUnique<PluginPolicyHandler>());
#endif  // defined(ENABLE_PLUGINS)

  return handlers;
}

}  // namespace policy
