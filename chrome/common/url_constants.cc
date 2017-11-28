// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/url_constants.h"

namespace chrome {

#if defined(OS_CHROMEOS)
const char kCrosScheme[] = "cros";
#endif

#if defined(OS_ANDROID)
const char kAndroidAppScheme[] = "android-app";
#endif

#if defined(OS_CHROMEOS)
const char kCupsPrintLearnMoreURL[] =
    "https://support.google.com/chromebook?p=chromebook_printing";
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
const char kEULAPathFormat[] = "/usr/share/chromeos-assets/eula/%s/eula.html";
const char kOemEulaURLPath[] = "oem";
const char kOnlineEulaURLPath[] =
    "https://www.google.com/intl/%s/chrome/eula_text.html";

const char kChromeOSCreditsPath[] =
    "/opt/google/chrome/resources/about_os_credits.html";

const char kChromeOSAssetHost[] = "chromeos-asset";
const char kChromeOSAssetPath[] = "/usr/share/chromeos-assets/";
#endif  // defined(OS_CHROMEOS)

const char kExtensionInvalidRequestURL[] = "chrome-extension://invalid/";

const char kSyncGoogleDashboardURL[] =
    "https://www.google.com/settings/chrome/sync/";

const char kGoogleAccountActivityControlsURL[] =
    "https://myaccount.google.com/activitycontrols/search";

const char kContentSettingsExceptionsLearnMoreURL[] =
    "https://support.google.com/chrome/?p=settings_manage_exceptions";

const char kPasswordManagerLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=settings_password";
#else
    "https://support.google.com/chrome/?p=settings_password";
#endif

const char kUpgradeHelpCenterBaseURL[] =
    "https://support.google.com/installer/?product="
    "{8A69D345-D564-463c-AFF1-A69D9E530F96}&error=";

const char kSmartLockHelpPage[] =
    "https://support.google.com/accounts/answer/6197437";

const char kExtensionControlledSettingLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_settings_api_extension";

const char kChromeHelpViaKeyboardURL[] =
#if defined(OS_CHROMEOS)
#if defined(GOOGLE_CHROME_BUILD)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook/?p=help&ctx=keyboard";
#endif  // defined(GOOGLE_CHROME_BUILD
#else
    "https://support.google.com/chrome/?p=help&ctx=keyboard";
#endif  // defined(OS_CHROMEOS)

const char kChromeHelpViaMenuURL[] =
#if defined(OS_CHROMEOS)
#if defined(GOOGLE_CHROME_BUILD)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook/?p=help&ctx=menu";
#endif  // defined(GOOGLE_CHROME_BUILD
#else
    "https://support.google.com/chrome/?p=help&ctx=menu";
#endif  // defined(OS_CHROMEOS)

const char kChromeHelpViaWebUIURL[] =
#if defined(OS_CHROMEOS)
#if defined(GOOGLE_CHROME_BUILD)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook/?p=help&ctx=settings";
#endif  // defined(GOOGLE_CHROME_BUILD
#else
    "https://support.google.com/chrome/?p=help&ctx=settings";
#endif  // defined(OS_CHROMEOS)

const char kChromeBetaForumURL[] =
    "https://support.google.com/chrome/?p=beta_forum";

#if defined(OS_CHROMEOS)
const char kChromeAccessibilityHelpURL[] =
    "https://support.google.com/chromebook/topic/6323347";
const char kChromeAccessibilitySettingsURL[] =
    "/chromevox/background/options.html";
const char kChromePaletteHelpURL[] =
    "https://support.google.com/chromebook?p=stylus_help";
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)
const char kChromeSyncLearnMoreURL[] =
    "https://support.google.com/chrome/answer/165139";

const char kChromeSyncMergeTroubleshootingURL[] =
    "https://support.google.com/chrome/answer/1181420#merge";
#endif  // BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)

#if defined(OS_MACOSX)
const char kChromeEnterpriseSignInLearnMoreURL[] =
    "https://support.google.com/chromebook/answer/1331549";
#endif

const char kResetProfileSettingsLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_reset_settings";

const char kAutomaticSettingsResetLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_automatic_settings_reset";

const char kLegacySupervisedUserManagementURL[] =
    "https://www.chrome.com/manage";
const char kLegacySupervisedUserManagementDisplayURL[] =
    "www.chrome.com/manage";

const char kSettingsSearchHelpURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=settings_search_help";
#else
    "https://support.google.com/chrome/?p=settings_search_help";
#endif

const char kOmniboxLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=settings_omnibox";
#else
    "https://support.google.com/chrome/?p=settings_omnibox";
#endif

const char kPageInfoHelpCenterURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=ui_security_indicator";
#else
    "https://support.google.com/chrome/?p=ui_security_indicator";
#endif

const char kCrashReasonURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=e_awsnap";
#else
    "https://support.google.com/chrome/?p=e_awsnap";
#endif

const char kCrashReasonFeedbackDisplayedURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=e_awsnap_rl";
#else
    "https://support.google.com/chrome/?p=e_awsnap_rl";
#endif

const char kPrivacyLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=settings_privacy";
#else
    "https://support.google.com/chrome/?p=settings_privacy";
#endif

const char kMyActivityUrlInClearBrowsingData[] =
    "https://myactivity.google.com/myactivity/?utm_source=chrome_cbd";

const char kDoNotTrackLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=settings_do_not_track";
#else
    "https://support.google.com/chrome/?p=settings_do_not_track";
#endif

#if defined(OS_CHROMEOS)
const char kAttestationForContentProtectionLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=verified_access";
#endif

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
const char kEnhancedPlaybackNotificationLearnMoreURL[] =
#endif
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=enhanced_playback";
#elif defined(OS_ANDROID)
// Keep in sync with chrome/android/java/strings/android_chrome_strings.grd
    "https://support.google.com/chrome/?p=mobile_protected_content";
#endif

const char kChromiumProjectURL[] = "https://www.chromium.org/";

const char kLearnMoreReportingURL[] =
    "https://support.google.com/chrome/?p=ui_usagestat";

#if BUILDFLAG(ENABLE_PLUGINS)
const char kOutdatedPluginLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_outdated_plugin";

const char kBlockedPluginLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_blocked_plugin";
#endif

const char kLearnMoreRegisterProtocolHandlerURL[] =
    "https://support.google.com/chrome/?p=ib_protocol_handler";

const char kSyncLearnMoreURL[] =
    "https://support.google.com/chrome/?p=settings_sign_in";

const char kDownloadScanningLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_download_blocked";

const char kDownloadInterruptedLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_download_errors";

const char kSyncEverythingLearnMoreURL[] =
    "https://support.google.com/chrome/?p=settings_sync_all";

#if defined(OS_CHROMEOS)
const char kCrosPrintingLearnMoreURL[] =
    "https://support.google.com/chromebook?p=chromebook_printing";
#endif

const char kCloudPrintLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=settings_cloud_print";
#else
    "https://support.google.com/chrome/?p=settings_cloud_print";
#endif

const char kCloudPrintNoDestinationsLearnMoreURL[] =
    "https://www.google.com/cloudprint/learn/";

const char kAppLauncherHelpURL[] =
    "https://support.google.com/chrome_webstore/?p=cws_app_launcher";

const char kSyncEncryptionHelpURL[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=settings_encryption";
#else
    "https://support.google.com/chrome/?p=settings_encryption";
#endif

const char kSyncErrorsHelpURL[] =
    "https://support.google.com/chrome/?p=settings_sync_error";

#if defined(OS_CHROMEOS)
const char kNaturalScrollHelpURL[] =
    "https://support.google.com/chromebook/?p=simple_scrolling";
const char kLearnMoreEnterpriseURL[] =
    "https://support.google.com/chromebook/?p=managed";
const char kAndroidAppsLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=playapps";
const char kInstantTetheringLearnMoreURL[] =
    "https://support.google.com/chromebook?p=instant_tethering";
const char kTPMFirmwareUpdateLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=tpm_update";
#endif

const char kRemoveNonCWSExtensionURL[] =
    "https://support.google.com/chrome/?p=ui_remove_non_cws_extensions";

#if defined(OS_WIN)
const char kNotificationsHelpURL[] =
    "https://support.google.com/chrome/?p=ui_notifications";

const char kChromeCleanerLearnMoreURL[] =
    "https://support.google.com/chrome/?p=chrome_cleanup_tool";
#endif

const char kChromeNativeScheme[] = "chrome-native";

const char kChromeSearchScheme[] = "chrome-search";
const char kChromeSearchLocalNtpHost[] = "local-ntp";
const char kChromeSearchLocalNtpUrl[] =
    "chrome-search://local-ntp/local-ntp.html";
const char kChromeSearchRemoteNtpHost[] = "remote-ntp";

const char kChromeSearchMostVisitedHost[] = "most-visited";
const char kChromeSearchMostVisitedUrl[] = "chrome-search://most-visited/";

const char kEasyUnlockLearnMoreUrl[] =
    "https://support.google.com/chromebook/?p=smart_lock";

// Google SafeSearch query parameters.
const char kSafeSearchSafeParameter[] = "safe=active";
const char kSafeSearchSsuiParameter[] = "ssui=on";

const char kMediaAccessLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=ib_access_cam_mic";

const char kLanguageSettingsLearnMoreUrl[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/answer/1059490";
#else
    "https://support.google.com/chrome/topic/1678461";
#endif

#if defined(OS_MACOSX)
// TODO(mark): Change to a Help Center URL when one is available.
// https://crbug.com/555044
const char kMac10_678_DeprecationURL[] =
    "https://chrome.blogspot.com/2015/11/updates-to-chrome-platform-support.html";
#endif

#if defined(OS_WIN)
const char kWindowsXPVistaDeprecationURL[] =
    "https://chrome.blogspot.com/2015/11/updates-to-chrome-platform-support.html";
#endif

const char kChooserBluetoothOverviewURL[] =
    "https://support.google.com/chrome?p=bluetooth";

const char kBluetoothAdapterOffHelpURL[] =
#if defined(OS_CHROMEOS)
    "chrome://settings/?search=bluetooth";
#else
    "https://support.google.com/chrome?p=bluetooth";
#endif

const char kChooserUsbOverviewURL[] =
    "https://support.google.com/chrome?p=webusb";

#if defined(OS_CHROMEOS)
const char kEolNotificationURL[] = "https://www.google.com/chromebook/older/";

const char kGoogleNameserversLearnMoreURL[] =
    "https://developers.google.com/speed/public-dns";
#endif

}  // namespace chrome
