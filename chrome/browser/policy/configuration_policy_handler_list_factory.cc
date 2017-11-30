// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler_list_factory.h"

#include <limits.h>
#include <stddef.h>

#include <utility>
#include <vector>

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
#include "chrome/browser/profiles/guest_mode_policy_handler.h"
#include "chrome/browser/profiles/incognito_mode_policy_handler.h"
#include "chrome/browser/sessions/restore_on_startup_policy_handler.h"
#include "chrome/browser/supervised_user/supervised_user_creation_policy_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/autofill_credit_card_policy_handler.h"
#include "components/autofill/core/browser/autofill_policy_handler.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/network_time/network_time_pref_names.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
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
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/search_engines/default_search_policy_handler.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "components/ssl_config/ssl_config_prefs.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_policy_handler.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/variations/pref_names.h"
#include "extensions/features/features.h"
#include "media/media_features.h"
#include "ppapi/features/features.h"

#if defined(OS_ANDROID)
#include "chrome/browser/search/contextual_search_policy_handler_android.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/accessibility_types.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions_policy_handler.h"
#include "chrome/browser/chromeos/policy/configuration_policy_handler_chromeos.h"
#include "chrome/browser/policy/default_geolocation_policy_handler.h"
#include "chromeos/chromeos_pref_names.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "components/arc/arc_prefs.h"
#include "components/drive/drive_pref_names.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/download/default_download_dir_policy_handler.h"
#include "chrome/browser/download/download_dir_policy_handler.h"
#include "chrome/browser/policy/local_sync_policy_handler.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/messaging/native_messaging_policy_handler.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/manifest.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_policy_handler.h"
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/browser/pref_names.h"
#endif

namespace policy {

namespace {

// List of policy types to preference names. This is used for simple policies
// that directly map to a single preference.
// clang-format off
const PolicyToPreferenceMapEntry kSimplePolicyMap[] = {
  { key::kHomepageLocation,
    prefs::kHomePage,
    base::Value::Type::STRING },
  { key::kHomepageIsNewTabPage,
    prefs::kHomePageIsNewTabPage,
    base::Value::Type::BOOLEAN },
  { key::kNewTabPageLocation,
    prefs::kNewTabPageLocationOverride,
    base::Value::Type::STRING },
  { key::kRestoreOnStartupURLs,
    prefs::kURLsToRestoreOnStartup,
    base::Value::Type::LIST },
  { key::kAlternateErrorPagesEnabled,
    prefs::kAlternateErrorPagesEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSearchSuggestEnabled,
    prefs::kSearchSuggestEnabled,
    base::Value::Type::BOOLEAN },
  { key::kBuiltInDnsClientEnabled,
    prefs::kBuiltInDnsClientEnabled,
    base::Value::Type::BOOLEAN },
  { key::kWPADQuickCheckEnabled,
    prefs::kQuickCheckEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPacHttpsUrlStrippingEnabled,
    prefs::kPacHttpsUrlStrippingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kQuicAllowed,
    prefs::kQuicAllowed,
    base::Value::Type::BOOLEAN },
  { key::kSafeBrowsingEnabled,
    prefs::kSafeBrowsingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSafeBrowsingForTrustedSourcesEnabled,
    prefs::kSafeBrowsingForTrustedSourcesEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDownloadRestrictions,
    prefs::kDownloadRestrictions,
    base::Value::Type::INTEGER },
  { key::kForceGoogleSafeSearch,
    prefs::kForceGoogleSafeSearch,
    base::Value::Type::BOOLEAN },
  { key::kForceYouTubeRestrict,
    prefs::kForceYouTubeRestrict,
    base::Value::Type::INTEGER},
  { key::kPasswordManagerEnabled,
    password_manager::prefs::kCredentialsEnableService,
    base::Value::Type::BOOLEAN },
  { key::kPrintingEnabled,
    prefs::kPrintingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDisablePrintPreview,
    prefs::kPrintPreviewDisabled,
    base::Value::Type::BOOLEAN },
  { key::kDefaultPrinterSelection,
    prefs::kPrintPreviewDefaultDestinationSelectionRules,
    base::Value::Type::STRING },
  { key::kApplicationLocaleValue,
    prefs::kApplicationLocale,
    base::Value::Type::STRING },
  { key::kAlwaysOpenPdfExternally,
    prefs::kPluginsAlwaysOpenPdfExternally,
    base::Value::Type::BOOLEAN },
  { key::kShowHomeButton,
    prefs::kShowHomeButton,
    base::Value::Type::BOOLEAN },
  { key::kSavingBrowserHistoryDisabled,
    prefs::kSavingBrowserHistoryDisabled,
    base::Value::Type::BOOLEAN },
  { key::kAllowDeletingBrowserHistory,
    prefs::kAllowDeletingBrowserHistory,
    base::Value::Type::BOOLEAN },
  { key::kDeveloperToolsDisabled,
    prefs::kDevToolsDisabled,
    base::Value::Type::BOOLEAN },
  { key::kBlockThirdPartyCookies,
    prefs::kBlockThirdPartyCookies,
    base::Value::Type::BOOLEAN },
  { key::kDefaultCookiesSetting,
    prefs::kManagedDefaultCookiesSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultImagesSetting,
    prefs::kManagedDefaultImagesSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultPluginsSetting,
    prefs::kManagedDefaultPluginsSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultPopupsSetting,
    prefs::kManagedDefaultPopupsSetting,
    base::Value::Type::INTEGER },
  { key::kAutoSelectCertificateForUrls,
    prefs::kManagedAutoSelectCertificateForUrls,
    base::Value::Type::LIST },
  { key::kCookiesAllowedForUrls,
    prefs::kManagedCookiesAllowedForUrls,
    base::Value::Type::LIST },
  { key::kCookiesBlockedForUrls,
    prefs::kManagedCookiesBlockedForUrls,
    base::Value::Type::LIST },
  { key::kCookiesSessionOnlyForUrls,
    prefs::kManagedCookiesSessionOnlyForUrls,
    base::Value::Type::LIST },
  { key::kImagesAllowedForUrls,
    prefs::kManagedImagesAllowedForUrls,
    base::Value::Type::LIST },
  { key::kImagesBlockedForUrls,
    prefs::kManagedImagesBlockedForUrls,
    base::Value::Type::LIST },
  { key::kJavaScriptAllowedForUrls,
    prefs::kManagedJavaScriptAllowedForUrls,
    base::Value::Type::LIST },
  { key::kJavaScriptBlockedForUrls,
    prefs::kManagedJavaScriptBlockedForUrls,
    base::Value::Type::LIST },
  { key::kPluginsAllowedForUrls,
    prefs::kManagedPluginsAllowedForUrls,
    base::Value::Type::LIST },
  { key::kPluginsBlockedForUrls,
    prefs::kManagedPluginsBlockedForUrls,
    base::Value::Type::LIST },
  { key::kPopupsAllowedForUrls,
    prefs::kManagedPopupsAllowedForUrls,
    base::Value::Type::LIST },
  { key::kPopupsBlockedForUrls,
    prefs::kManagedPopupsBlockedForUrls,
    base::Value::Type::LIST },
  { key::kNotificationsAllowedForUrls,
    prefs::kManagedNotificationsAllowedForUrls,
    base::Value::Type::LIST },
  { key::kNotificationsBlockedForUrls,
    prefs::kManagedNotificationsBlockedForUrls,
    base::Value::Type::LIST },
  { key::kDefaultNotificationsSetting,
    prefs::kManagedDefaultNotificationsSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultGeolocationSetting,
    prefs::kManagedDefaultGeolocationSetting,
    base::Value::Type::INTEGER },
  { key::kSigninAllowed,
    prefs::kSigninAllowed,
    base::Value::Type::BOOLEAN },
  { key::kEnableOnlineRevocationChecks,
    ssl_config::prefs::kCertRevocationCheckingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kRequireOnlineRevocationChecksForLocalAnchors,
    ssl_config::prefs::kCertRevocationCheckingRequiredLocalAnchors,
    base::Value::Type::BOOLEAN },
  { key::kEnableSha1ForLocalAnchors,
    ssl_config::prefs::kCertEnableSha1LocalAnchors,
    base::Value::Type::BOOLEAN },
  { key::kEnableCommonNameFallbackForLocalAnchors,
    ssl_config::prefs::kCertEnableCommonNameFallbackLocalAnchors,
    base::Value::Type::BOOLEAN },
  { key::kAuthSchemes,
    prefs::kAuthSchemes,
    base::Value::Type::STRING },
  { key::kDisableAuthNegotiateCnameLookup,
    prefs::kDisableAuthNegotiateCnameLookup,
    base::Value::Type::BOOLEAN },
  { key::kEnableAuthNegotiatePort,
    prefs::kEnableAuthNegotiatePort,
    base::Value::Type::BOOLEAN },
  { key::kAuthServerWhitelist,
    prefs::kAuthServerWhitelist,
    base::Value::Type::STRING },
  { key::kAuthNegotiateDelegateWhitelist,
    prefs::kAuthNegotiateDelegateWhitelist,
    base::Value::Type::STRING },
  { key::kGSSAPILibraryName,
    prefs::kGSSAPILibraryName,
    base::Value::Type::STRING },
  { key::kAllowCrossOriginAuthPrompt,
    prefs::kAllowCrossOriginAuthPrompt,
    base::Value::Type::BOOLEAN },
#if defined(OS_POSIX)
  { key::kNtlmV2Enabled,
    prefs::kNtlmV2Enabled,
    base::Value::Type::BOOLEAN },
#endif  // defined(OS_POSIX)
  { key::kDisable3DAPIs,
    prefs::kDisable3DAPIs,
    base::Value::Type::BOOLEAN },
  { key::kDiskCacheSize,
    prefs::kDiskCacheSize,
    base::Value::Type::INTEGER },
  { key::kMediaCacheSize,
    prefs::kMediaCacheSize,
    base::Value::Type::INTEGER },
  { key::kPolicyRefreshRate,
    policy_prefs::kUserPolicyRefreshRate,
    base::Value::Type::INTEGER },
  { key::kDevicePolicyRefreshRate,
    prefs::kDevicePolicyRefreshRate,
    base::Value::Type::INTEGER },
  { key::kDefaultBrowserSettingEnabled,
    prefs::kDefaultBrowserSettingEnabled,
    base::Value::Type::BOOLEAN },
  { key::kCloudPrintProxyEnabled,
    prefs::kCloudPrintProxyEnabled,
    base::Value::Type::BOOLEAN },
  { key::kCloudPrintSubmitEnabled,
    prefs::kCloudPrintSubmitEnabled,
    base::Value::Type::BOOLEAN },
  { key::kTranslateEnabled,
    prefs::kOfferTranslateEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAllowOutdatedPlugins,
    prefs::kPluginsAllowOutdated,
    base::Value::Type::BOOLEAN },
  { key::kRunAllFlashInAllowMode,
    prefs::kRunAllFlashInAllowMode,
    base::Value::Type::BOOLEAN },
  { key::kAlwaysAuthorizePlugins,
    prefs::kPluginsAlwaysAuthorize,
    base::Value::Type::BOOLEAN },
  { key::kBookmarkBarEnabled,
    bookmarks::prefs::kShowBookmarkBar,
    base::Value::Type::BOOLEAN },
  { key::kEditBookmarksEnabled,
    bookmarks::prefs::kEditBookmarksEnabled,
    base::Value::Type::BOOLEAN },
  { key::kShowAppsShortcutInBookmarkBar,
    bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
    base::Value::Type::BOOLEAN },
  { key::kAllowFileSelectionDialogs,
    prefs::kAllowFileSelectionDialogs,
    base::Value::Type::BOOLEAN },
  { key::kPromptForDownloadLocation,
    prefs::kPromptForDownload,
    base::Value::Type::BOOLEAN },

  // First run import.
  { key::kImportBookmarks,
    prefs::kImportBookmarks,
    base::Value::Type::BOOLEAN },
  { key::kImportHistory,
    prefs::kImportHistory,
    base::Value::Type::BOOLEAN },
  { key::kImportHomepage,
    prefs::kImportHomepage,
    base::Value::Type::BOOLEAN },
  { key::kImportSearchEngine,
    prefs::kImportSearchEngine,
    base::Value::Type::BOOLEAN },
  { key::kImportSavedPasswords,
    prefs::kImportSavedPasswords,
    base::Value::Type::BOOLEAN },
  { key::kImportAutofillFormData,
    prefs::kImportAutofillFormData,
    base::Value::Type::BOOLEAN },

  // Import data dialog: controlled by same policies as first run import, but
  // uses different prefs.
  { key::kImportBookmarks,
    prefs::kImportDialogBookmarks,
    base::Value::Type::BOOLEAN },
  { key::kImportHistory,
    prefs::kImportDialogHistory,
    base::Value::Type::BOOLEAN },
  { key::kImportSearchEngine,
    prefs::kImportDialogSearchEngine,
    base::Value::Type::BOOLEAN },
  { key::kImportSavedPasswords,
    prefs::kImportDialogSavedPasswords,
    base::Value::Type::BOOLEAN },
  { key::kImportAutofillFormData,
    prefs::kImportDialogAutofillFormData,
    base::Value::Type::BOOLEAN },

  { key::kMaxConnectionsPerProxy,
    prefs::kMaxConnectionsPerProxy,
    base::Value::Type::INTEGER },
  { key::kURLWhitelist,
    policy_prefs::kUrlWhitelist,
    base::Value::Type::LIST },
  { key::kRestrictSigninToPattern,
    prefs::kGoogleServicesUsernamePattern,
    base::Value::Type::STRING },
  { key::kDefaultWebBluetoothGuardSetting,
    prefs::kManagedDefaultWebBluetoothGuardSetting,
    base::Value::Type::INTEGER },
  { key::kDefaultMediaStreamSetting,
    prefs::kManagedDefaultMediaStreamSetting,
    base::Value::Type::INTEGER },
  { key::kDisableSafeBrowsingProceedAnyway,
    prefs::kSafeBrowsingProceedAnywayDisabled,
    base::Value::Type::BOOLEAN },
  { key::kSafeBrowsingExtendedReportingOptInAllowed,
    prefs::kSafeBrowsingExtendedReportingOptInAllowed,
    base::Value::Type::BOOLEAN },
  { key::kSSLErrorOverrideAllowed,
    prefs::kSSLErrorOverrideAllowed,
    base::Value::Type::BOOLEAN },
  { key::kHardwareAccelerationModeEnabled,
    prefs::kHardwareAccelerationModeEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAllowDinosaurEasterEgg,
    prefs::kAllowDinosaurEasterEgg,
    base::Value::Type::BOOLEAN },
  { key::kAllowedDomainsForApps,
    prefs::kAllowedDomainsForApps,
    base::Value::Type::STRING },
  { key::kComponentUpdatesEnabled,
    prefs::kComponentUpdatesEnabled,
    base::Value::Type::BOOLEAN },

#if BUILDFLAG(ENABLE_SPELLCHECK)
  { key::kSpellCheckServiceEnabled,
    spellcheck::prefs::kSpellCheckUseSpellingService,
    base::Value::Type::BOOLEAN },
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

  { key::kDisableScreenshots,
    prefs::kDisableScreenshots,
    base::Value::Type::BOOLEAN },
  { key::kAudioCaptureAllowed,
    prefs::kAudioCaptureAllowed,
    base::Value::Type::BOOLEAN },
  { key::kVideoCaptureAllowed,
    prefs::kVideoCaptureAllowed,
    base::Value::Type::BOOLEAN },
  { key::kAudioCaptureAllowedUrls,
    prefs::kAudioCaptureAllowedUrls,
    base::Value::Type::LIST },
  { key::kVideoCaptureAllowedUrls,
    prefs::kVideoCaptureAllowedUrls,
    base::Value::Type::LIST },
  { key::kHideWebStoreIcon,
    prefs::kHideWebStoreIcon,
    base::Value::Type::BOOLEAN },
  { key::kVariationsRestrictParameter,
    variations::prefs::kVariationsRestrictParameter,
    base::Value::Type::STRING },
  { key::kForceEphemeralProfiles,
    prefs::kForceEphemeralProfiles,
    base::Value::Type::BOOLEAN },
  { key::kSSLVersionMax,
    ssl_config::prefs::kSSLVersionMax,
    base::Value::Type::STRING },
  { key::kNTPContentSuggestionsEnabled,
    ntp_snippets::prefs::kEnableSnippets,
    base::Value::Type::BOOLEAN },
  { key::kEnableMediaRouter,
    prefs::kEnableMediaRouter,
    base::Value::Type::BOOLEAN },
#if !defined(OS_ANDROID)
  { key::kShowCastIconInToolbar,
    prefs::kShowCastIconInToolbar,
    base::Value::Type::BOOLEAN },
#endif  // !defined(OS_ANDROID)
#if BUILDFLAG(ENABLE_WEBRTC)
  { key::kWebRtcUdpPortRange,
    prefs::kWebRTCUDPPortRange,
    base::Value::Type::STRING },
#endif  // BUILDFLAG(ENABLE_WEBRTC)
#if !defined(OS_MACOSX)
  { key::kFullscreenAllowed,
    prefs::kFullscreenAllowed,
    base::Value::Type::BOOLEAN },
#if BUILDFLAG(ENABLE_EXTENSIONS)
  { key::kFullscreenAllowed,
    extensions::pref_names::kAppFullscreenAllowed,
    base::Value::Type::BOOLEAN },
  { key::kSecurityKeyPermitAttestation,
    prefs::kSecurityKeyPermitAttestation,
    base::Value::Type::LIST },
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // !defined(OS_MACOSX)

#if defined(OS_CHROMEOS)
  { key::kChromeOsLockOnIdleSuspend,
    prefs::kEnableAutoScreenLock,
    base::Value::Type::BOOLEAN },
  { key::kChromeOsReleaseChannel,
    prefs::kChromeOsReleaseChannel,
    base::Value::Type::STRING },
  { key::kDriveDisabled,
    drive::prefs::kDisableDrive,
    base::Value::Type::BOOLEAN },
  { key::kDriveDisabledOverCellular,
    drive::prefs::kDisableDriveOverCellular,
    base::Value::Type::BOOLEAN },
  { key::kExternalStorageDisabled,
    prefs::kExternalStorageDisabled,
    base::Value::Type::BOOLEAN },
  { key::kExternalStorageReadOnly,
    prefs::kExternalStorageReadOnly,
    base::Value::Type::BOOLEAN },
  { key::kAudioOutputAllowed,
    chromeos::prefs::kAudioOutputAllowed,
    base::Value::Type::BOOLEAN },
  { key::kShowLogoutButtonInTray,
    ash::prefs::kShowLogoutButtonInTray,
    base::Value::Type::BOOLEAN },
  { key::kShelfAutoHideBehavior,
    ash::prefs::kShelfAutoHideBehaviorLocal,
    base::Value::Type::STRING },
  { key::kSessionLengthLimit,
    prefs::kSessionLengthLimit,
    base::Value::Type::INTEGER },
  { key::kWaitForInitialUserActivity,
    prefs::kSessionWaitForInitialUserActivity,
    base::Value::Type::BOOLEAN },
  { key::kPowerManagementUsesAudioActivity,
    prefs::kPowerUseAudioActivity,
    base::Value::Type::BOOLEAN },
  { key::kPowerManagementUsesVideoActivity,
    prefs::kPowerUseVideoActivity,
    base::Value::Type::BOOLEAN },
  { key::kAllowScreenWakeLocks,
    prefs::kPowerAllowScreenWakeLocks,
    base::Value::Type::BOOLEAN },
  { key::kWaitForInitialUserActivity,
    prefs::kPowerWaitForInitialUserActivity,
    base::Value::Type::BOOLEAN },
  { key::kTermsOfServiceURL,
    prefs::kTermsOfServiceURL,
    base::Value::Type::STRING },
  { key::kShowAccessibilityOptionsInSystemTrayMenu,
    ash::prefs::kShouldAlwaysShowAccessibilityMenu,
    base::Value::Type::BOOLEAN },
  { key::kLargeCursorEnabled,
    ash::prefs::kAccessibilityLargeCursorEnabled,
    base::Value::Type::BOOLEAN },
  { key::kSpokenFeedbackEnabled,
    ash::prefs::kAccessibilitySpokenFeedbackEnabled,
    base::Value::Type::BOOLEAN },
  { key::kHighContrastEnabled,
    ash::prefs::kAccessibilityHighContrastEnabled,
    base::Value::Type::BOOLEAN },
  { key::kVirtualKeyboardEnabled,
    ash::prefs::kAccessibilityVirtualKeyboardEnabled,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenDefaultLargeCursorEnabled,
    NULL,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenDefaultSpokenFeedbackEnabled,
    NULL,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenDefaultHighContrastEnabled,
    NULL,
    base::Value::Type::BOOLEAN },
  { key::kDeviceLoginScreenDefaultVirtualKeyboardEnabled,
    NULL,
    base::Value::Type::BOOLEAN },
  { key::kRebootAfterUpdate,
    prefs::kRebootAfterUpdate,
    base::Value::Type::BOOLEAN },
  { key::kAttestationEnabledForUser,
    prefs::kAttestationEnabled,
    base::Value::Type::BOOLEAN },
  { key::kChromeOsMultiProfileUserBehavior,
    prefs::kMultiProfileUserBehavior,
    base::Value::Type::STRING },
  { key::kKeyboardDefaultToFunctionKeys,
    prefs::kLanguageSendFunctionKeys,
    base::Value::Type::BOOLEAN },
  { key::kTouchVirtualKeyboardEnabled,
    prefs::kTouchVirtualKeyboardEnabled,
    base::Value::Type::BOOLEAN },
  { key::kEasyUnlockAllowed,
    prefs::kEasyUnlockAllowed,
    base::Value::Type::BOOLEAN },
  { key::kInstantTetheringAllowed,
    prefs::kInstantTetheringAllowed,
    base::Value::Type::BOOLEAN },
  { key::kCaptivePortalAuthenticationIgnoresProxy,
    prefs::kCaptivePortalAuthenticationIgnoresProxy,
    base::Value::Type::BOOLEAN },
  { key::kForceMaximizeOnFirstRun,
    prefs::kForceMaximizeOnFirstRun,
    base::Value::Type::BOOLEAN },
  { key::kUnifiedDesktopEnabledByDefault,
    prefs::kUnifiedDesktopEnabledByDefault,
    base::Value::Type::BOOLEAN },
  { key::kArcEnabled,
    arc::prefs::kArcEnabled,
    base::Value::Type::BOOLEAN },
  { key::kArcBackupRestoreEnabled,
    arc::prefs::kArcBackupRestoreEnabled,
    base::Value::Type::BOOLEAN },
  { key::kArcLocationServiceEnabled,
    arc::prefs::kArcLocationServiceEnabled,
    base::Value::Type::BOOLEAN },
  { key::kReportArcStatusEnabled,
    prefs::kReportArcStatusEnabled,
    base::Value::Type::BOOLEAN },
  { key::kNativePrinters,
    prefs::kRecommendedNativePrinters,
    base::Value::Type::LIST },
  { key::kEcryptfsMigrationStrategy,
    arc::prefs::kEcryptfsMigrationStrategy,
    base::Value::Type::INTEGER },
  { key::kNativePrintersBulkAccessMode,
    prefs::kRecommendedNativePrintersAccessMode,
    base::Value::Type::INTEGER },
  { key::kNativePrintersBulkBlacklist,
    prefs::kRecommendedNativePrintersBlacklist,
    base::Value::Type::LIST },
  { key::kNativePrintersBulkWhitelist,
    prefs::kRecommendedNativePrintersWhitelist,
    base::Value::Type::LIST },
#endif  // defined(OS_CHROMEOS)

// Metrics reporting is controlled by a platform specific policy for ChromeOS
#if defined(OS_CHROMEOS)
  { key::kDeviceMetricsReportingEnabled,
    metrics::prefs::kMetricsReportingEnabled,
    base::Value::Type::BOOLEAN },
#else
  { key::kMetricsReportingEnabled,
    metrics::prefs::kMetricsReportingEnabled,
    base::Value::Type::BOOLEAN },
#endif

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  { key::kBackgroundModeEnabled,
    prefs::kBackgroundModeEnabled,
    base::Value::Type::BOOLEAN },
#endif  // !defined(OS_MACOSX) && !defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
  { key::kDataCompressionProxyEnabled,
    prefs::kDataSaverEnabled,
    base::Value::Type::BOOLEAN },
  { key::kAuthAndroidNegotiateAccountType,
    prefs::kAuthAndroidNegotiateAccountType,
    base::Value::Type::STRING },
#endif  // defined(OS_ANDROID)

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  { key::kNativeMessagingUserLevelHosts,
    extensions::pref_names::kNativeMessagingUserLevelHosts,
    base::Value::Type::BOOLEAN },
  { key::kBrowserAddPersonEnabled,
    prefs::kBrowserAddPersonEnabled,
    base::Value::Type::BOOLEAN },
  { key::kPrintPreviewUseSystemDefaultPrinter,
    prefs::kPrintPreviewUseSystemDefaultPrinter,
    base::Value::Type::BOOLEAN },
  { key::kCloudPolicyOverridesMachinePolicy,
    prefs::kCloudPolicyOverridesMachinePolicy,
    base::Value::Type::BOOLEAN },
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

  { key::kForceBrowserSignin,
    prefs::kForceBrowserSignin,
    base::Value::Type::BOOLEAN },

#if defined(OS_WIN)
  { key::kWelcomePageOnOSUpgradeEnabled,
    prefs::kWelcomePageOnOSUpgradeEnabled,
    base::Value::Type::BOOLEAN },
#endif  // OS_WIN

#if !defined(OS_ANDROID)
  { key::kSuppressUnsupportedOSWarning,
    prefs::kSuppressUnsupportedOSWarning,
    base::Value::Type::BOOLEAN },
#endif  // !OS_ANDROID

#if defined(OS_CHROMEOS)
  { key::kSystemTimezoneAutomaticDetection,
    prefs::kSystemTimezoneAutomaticDetectionPolicy,
    base::Value::Type::INTEGER },
#endif

  { key::kTaskManagerEndProcessEnabled,
    prefs::kTaskManagerEndProcessEnabled,
    base::Value::Type::BOOLEAN },

#if defined(OS_CHROMEOS)
  { key::kNetworkThrottlingEnabled,
    prefs::kNetworkThrottlingEnabled,
    base::Value::Type::DICTIONARY },

  { key::kAllowScreenLock,
    prefs::kAllowScreenLock,
    base::Value::Type::BOOLEAN },

  { key::kQuickUnlockModeWhitelist,
    prefs::kQuickUnlockModeWhitelist,
    base::Value::Type::LIST },
  { key::kQuickUnlockTimeout,
    prefs::kQuickUnlockTimeout,
    base::Value::Type::INTEGER },
  { key::kPinUnlockMinimumLength,
    prefs::kPinUnlockMinimumLength,
    base::Value::Type::INTEGER },
  { key::kPinUnlockMaximumLength,
    prefs::kPinUnlockMaximumLength,
    base::Value::Type::INTEGER },
  { key::kPinUnlockWeakPinsAllowed,
    prefs::kPinUnlockWeakPinsAllowed,
    base::Value::Type::BOOLEAN },

  { key::kCastReceiverEnabled,
    prefs::kCastReceiverEnabled,
    base::Value::Type::BOOLEAN },
#endif

  { key::kRoamingProfileSupportEnabled,
    syncer::prefs::kEnableLocalSyncBackend,
    base::Value::Type::BOOLEAN },

  { key::kBrowserNetworkTimeQueriesEnabled,
    network_time::prefs::kNetworkTimeQueriesEnabled,
    base::Value::Type::BOOLEAN },

  { key::kIsolateOrigins,
    prefs::kIsolateOrigins,
    base::Value::Type::STRING },
  { key::kSitePerProcess,
    prefs::kSitePerProcess,
    base::Value::Type::BOOLEAN },
};
// clang-format on

class ForceSafeSearchPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  ForceSafeSearchPolicyHandler()
      : TypeCheckingPolicyHandler(key::kForceSafeSearch,
                                  base::Value::Type::BOOLEAN) {}
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
            base::MakeUnique<base::Value>(
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
                                  base::Value::Type::BOOLEAN) {}
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
      prefs->SetValue(prefs::kForceYouTubeRestrict,
                      base::MakeUnique<base::Value>(
                          enabled ? safe_search_util::YOUTUBE_RESTRICT_MODERATE
                                  : safe_search_util::YOUTUBE_RESTRICT_OFF));
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ForceYouTubeSafetyModePolicyHandler);
};

class BrowsingHistoryPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  BrowsingHistoryPolicyHandler()
      : TypeCheckingPolicyHandler(key::kAllowDeletingBrowserHistory,
                                  base::Value::Type::BOOLEAN) {}
  ~BrowsingHistoryPolicyHandler() override {}

  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override {
    const base::Value* value = policies.GetValue(policy_name());
    bool deleting_history_allowed;
    if (value && value->GetAsBoolean(&deleting_history_allowed) &&
        !deleting_history_allowed) {
      prefs->SetBoolean(
          browsing_data::prefs::kDeleteBrowsingHistory, false);
      prefs->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistoryBasic,
                        false);
      prefs->SetBoolean(browsing_data::prefs::kDeleteDownloadHistory, false);
    }
  }
};

class SecureOriginPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  SecureOriginPolicyHandler()
      : TypeCheckingPolicyHandler(key::kUnsafelyTreatInsecureOriginAsSecure,
                                  base::Value::Type::LIST) {}
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override {
    const base::Value* value = policies.GetValue(policy_name());
    if (!value)
      return;

    std::string pref_string;
    for (const auto& list_entry : value->GetList()) {
      if (!pref_string.empty())
        pref_string.append(",");
      pref_string.append(list_entry.GetString());
    }
    prefs->SetString(prefs::kUnsafelyTreatInsecureOriginAsSecure, pref_string);
  }
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
                            new base::Value(entry.manifest_type))));
  }
}

// Piggy-back kDeveloperToolsDisabled set to true to also force-disable
// kExtensionsUIDeveloperMode.
class DevToolsExtensionsUIPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  DevToolsExtensionsUIPolicyHandler()
      : TypeCheckingPolicyHandler(key::kDeveloperToolsDisabled,
                                  base::Value::Type::BOOLEAN) {}
  ~DevToolsExtensionsUIPolicyHandler() override {}

  // ConfigurationPolicyHandler implementation:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override {
    const base::Value* value = policies.GetValue(policy_name());
    bool developerToolsDisabled;
    if (value && value->GetAsBoolean(&developerToolsDisabled) &&
        developerToolsDisabled) {
      prefs->SetValue(prefs::kExtensionsUIDeveloperMode,
                      base::MakeUnique<base::Value>(false));
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsExtensionsUIPolicyHandler);
};
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

  handlers->AddHandler(
      base::MakeUnique<autofill::AutofillCreditCardPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<autofill::AutofillPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<DefaultSearchPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<ForceSafeSearchPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<ForceYouTubeSafetyModePolicyHandler>());
  handlers->AddHandler(base::MakeUnique<IncognitoModePolicyHandler>());
  handlers->AddHandler(base::MakeUnique<GuestModePolicyHandler>());
  handlers->AddHandler(
      base::MakeUnique<ManagedBookmarksPolicyHandler>(chrome_schema));
  handlers->AddHandler(base::MakeUnique<ProxyPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<URLBlacklistPolicyHandler>());

  handlers->AddHandler(base::MakeUnique<SimpleSchemaValidatingPolicyHandler>(
      key::kCertificateTransparencyEnforcementDisabledForUrls,
      certificate_transparency::prefs::kCTExcludedHosts, chrome_schema,
      SCHEMA_STRICT, SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED));
  handlers->AddHandler(base::MakeUnique<SecureOriginPolicyHandler>());

#if defined(OS_ANDROID)
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

  handlers->AddHandler(base::MakeUnique<BrowsingHistoryPolicyHandler>());

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
      base::MakeUnique<
          extensions::ExtensionInstallLoginScreenAppListPolicyHandler>());
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
  handlers->AddHandler(base::MakeUnique<DevToolsExtensionsUIPolicyHandler>());
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
  handlers->AddHandler(base::MakeUnique<SupervisedUserCreationPolicyHandler>());
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
  handlers->AddHandler(base::MakeUnique<DefaultDownloadDirPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<DownloadDirPolicyHandler>());
  handlers->AddHandler(base::MakeUnique<LocalSyncPolicyHandler>());

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
      key::kDeviceLoginScreenDefaultScreenMagnifierType, nullptr,
      ash::MAGNIFIER_DISABLED, ash::MAGNIFIER_FULL, false)));
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
      base::MakeUnique<ExternalDataPolicyHandler>(key::kDeviceWallpaperImage));
  handlers->AddHandler(
      base::MakeUnique<ExternalDataPolicyHandler>(key::kWallpaperImage));
  handlers->AddHandler(base::MakeUnique<ExternalDataPolicyHandler>(
      key::kNativePrintersBulkConfiguration));
  handlers->AddHandler(base::WrapUnique(new SimpleSchemaValidatingPolicyHandler(
      key::kSessionLocales, NULL, chrome_schema, SCHEMA_STRICT,
      SimpleSchemaValidatingPolicyHandler::RECOMMENDED_ALLOWED,
      SimpleSchemaValidatingPolicyHandler::MANDATORY_PROHIBITED)));
  handlers->AddHandler(
      base::MakeUnique<chromeos::KeyPermissionsPolicyHandler>(chrome_schema));
  handlers->AddHandler(base::WrapUnique(new DefaultGeolocationPolicyHandler()));
  handlers->AddHandler(base::MakeUnique<extensions::ExtensionListPolicyHandler>(
      key::kNoteTakingAppsLockScreenWhitelist,
      prefs::kNoteTakingAppsLockScreenWhitelist, false /*allow_wildcards*/));
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_PLUGINS)
  handlers->AddHandler(base::MakeUnique<PluginPolicyHandler>());
#endif  // BUILDFLAG(ENABLE_PLUGINS)

  return handlers;
}

}  // namespace policy
