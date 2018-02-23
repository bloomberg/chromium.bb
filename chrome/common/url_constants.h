// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for known URLs and portions thereof.
// Except for WebUI UI/Host/SubPage constants. Those go in
// chrome/common/webui_url_constants.h.
//
// - Use the same order in this header and url_constants.cc.
// - Keep the constants sorted by name.
// - Put platform/feature specific constants towards the end in the appropriate
//   section.

#ifndef CHROME_COMMON_URL_CONSTANTS_H_
#define CHROME_COMMON_URL_CONSTANTS_H_

#include <stddef.h>

#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "ppapi/features/features.h"

namespace chrome {

// "Learn more" URL for when profile settings are automatically reset.
extern const char kAutomaticSettingsResetLearnMoreURL[];

// The URL for providing help when the Bluetooth adapter is off.
extern const char kBluetoothAdapterOffHelpURL[];

// The URL for the Bluetooth Overview help center article in the Web Bluetooth
// Chooser.
extern const char kChooserBluetoothOverviewURL[];

// The URL for the WebUsb help center article.
extern const char kChooserUsbOverviewURL[];

// Link to the forum for Chrome Beta.
extern const char kChromeBetaForumURL[];

// General help links for Chrome, opened using various actions.
extern const char kChromeHelpViaKeyboardURL[];
extern const char kChromeHelpViaMenuURL[];
extern const char kChromeHelpViaWebUIURL[];

// The chrome-native: scheme is used show pages rendered with platform specific
// widgets instead of using HTML.
extern const char kChromeNativeScheme[];

// Pages under chrome-search.
extern const char kChromeSearchLocalNtpHost[];
extern const char kChromeSearchLocalNtpUrl[];

// Host and URL for most visited iframes used on the Instant Extended NTP.
extern const char kChromeSearchMostVisitedHost[];
extern const char kChromeSearchMostVisitedUrl[];

// Page under chrome-search.
extern const char kChromeSearchRemoteNtpHost[];

// The chrome-search: scheme is served by the same backend as chrome:.  However,
// only specific URLDataSources are enabled to serve requests via the
// chrome-search: scheme.  See |InstantIOContext::ShouldServiceRequest| and its
// callers for details.  Note that WebUIBindings should never be granted to
// chrome-search: pages.  chrome-search: pages are displayable but not readable
// by external search providers (that are rendered by Instant renderer
// processes), and neither displayable nor readable by normal (non-Instant) web
// pages.  To summarize, a non-Instant process, when trying to access
// 'chrome-search://something', will bump up against the following:
//
//  1. Renderer: The display-isolated check in WebKit will deny the request,
//  2. Browser: Assuming they got by #1, the scheme checks in
//     URLDataSource::ShouldServiceRequest will deny the request,
//  3. Browser: for specific sub-classes of URLDataSource, like ThemeSource
//     there are additional Instant-PID checks that make sure the request is
//     coming from a blessed Instant process, and deny the request.
extern const char kChromeSearchScheme[];

// The URL for the Chromium project used in the About dialog.
extern const char kChromiumProjectURL[];

// "Learn more" URL for the Cloud Print section under Options.
extern const char kCloudPrintLearnMoreURL[];

// "Learn more" URL for the Cloud Print Preview certificate error.
extern const char kCloudPrintCertificateErrorLearnMoreURL[];

// "Learn more" URL for the Cloud Print Preview No Destinations Promotion.
extern const char kCloudPrintNoDestinationsLearnMoreURL[];

extern const char kContentSettingsExceptionsLearnMoreURL[];

// "Learn more" URL for "Aw snap" page when showing "Reload" button.
extern const char kCrashReasonURL[];

// "Learn more" URL for "Aw snap" page when showing "Send feedback" button.
extern const char kCrashReasonFeedbackDisplayedURL[];

// "Learn more" URL for the "Do not track" setting in the privacy section.
extern const char kDoNotTrackLearnMoreURL[];

// The URL for the "Learn more" page for interrupted downloads.
extern const char kDownloadInterruptedLearnMoreURL[];

// The URL for the "Learn more" page for download scanning.
extern const char kDownloadScanningLearnMoreURL[];

// The URL for the "Learn more" link the the Easy Unlock settings.
// TODO(thestig): Move into OS_CHROMEOS section.
extern const char kEasyUnlockLearnMoreUrl[];

// "Learn more" URL for the Settings API, NTP bubble and other settings bubbles
// showing which extension is controlling them.
extern const char kExtensionControlledSettingLearnMoreURL[];

// URL used to indicate that an extension resource load request was invalid.
extern const char kExtensionInvalidRequestURL[];

// URL of the 'Activity controls' section of the privacy settings page.
extern const char kGoogleAccountActivityControlsURL[];

// The URL for the "Learn more" link in the language settings.
// TODO(michaelpg): Compile on Chrome OS only when Options is removed.
extern const char kLanguageSettingsLearnMoreUrl[];

// The URL for the "Learn more" page for the usage/crash reporting option in the
// first run dialog.
extern const char kLearnMoreReportingURL[];

// Management URL for Chrome Supervised Users - version without scheme, used
// for display.
extern const char kLegacySupervisedUserManagementDisplayURL[];

// Management URL for Chrome Supervised Users.
extern const char kLegacySupervisedUserManagementURL[];

// "myactivity.google.com" URL for the history checkbox in ClearBrowsingData.
extern const char kMyActivityUrlInClearBrowsingData[];

// Help URL for the Omnibox setting.
extern const char kOmniboxLearnMoreURL[];

// "What do these mean?" URL for the Page Info bubble.
extern const char kPageInfoHelpCenterURL[];

extern const char kPasswordManagerLearnMoreURL[];

// "Learn more" URL for the Privacy section under Options.
extern const char kPrivacyLearnMoreURL[];

// The URL for the Learn More link of the non-CWS bubble.
extern const char kRemoveNonCWSExtensionURL[];

// "Learn more" URL for resetting profile preferences.
extern const char kResetProfileSettingsLearnMoreURL[];

// Parameters that get appended to force SafeSearch.
extern const char kSafeSearchSafeParameter[];
extern const char kSafeSearchSsuiParameter[];

// Help URL for the settings page's search feature.
extern const char kSettingsSearchHelpURL[];

extern const char kSmartLockHelpPage[];

// The URL for the "Learn more" page on sync encryption.
extern const char kSyncEncryptionHelpURL[];

// The URL for the "Learn more" link when there is a sync error.
extern const char kSyncErrorsHelpURL[];

extern const char kSyncGoogleDashboardURL[];

// The URL for the "Learn more" page for sync setup on the personal stuff page.
extern const char kSyncLearnMoreURL[];

extern const char kUpgradeHelpCenterBaseURL[];

#if defined(OS_ANDROID)
extern const char kAndroidAppScheme[];
#endif

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
// "Learn more" URL for the enhanced playback notification dialog.
extern const char kEnhancedPlaybackNotificationLearnMoreURL[];
#endif

#if defined(OS_CHROMEOS)
// The URL for the "learn more" link for Google Play Store (ARC) settings.
extern const char kAndroidAppsLearnMoreURL[];

// Accessibility help link for Chrome.
extern const char kChromeAccessibilityHelpURL[];

extern const char kChromeOSAssetHost[];
extern const char kChromeOSAssetPath[];

extern const char kChromeOSCreditsPath[];

// Palette help link for Chrome.
extern const char kChromePaletteHelpURL[];

extern const char kCrosScheme[];

extern const char kCupsPrintLearnMoreURL[];

extern const char kEULAPathFormat[];

// The URL for EOL notification
extern const char kEolNotificationURL[];

// The URL for providing more information about Google nameservers.
extern const char kGoogleNameserversLearnMoreURL[];

// The URL for the "learn more" link for Instant Tethering.
extern const char kInstantTetheringLearnMoreURL[];

// The URL for the Learn More page about enterprise enrolled devices.
extern const char kLearnMoreEnterpriseURL[];

// The URL for the "Learn more" link for natural scrolling on ChromeOS.
extern const char kNaturalScrollHelpURL[];

extern const char kOemEulaURLPath[];

extern const char kOnlineEulaURLPath[];

// The URL for the "learn more" link for TPM firmware update.
extern const char kTPMFirmwareUpdateLearnMoreURL[];
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
// "Learn more" URL for the enterprise sign-in confirmation dialog.
extern const char kChromeEnterpriseSignInLearnMoreURL[];

// The URL for the "learn more" link on the 10.9 obsolescence infobar.
extern const char kMac10_9_ObsoleteURL[];
#endif

#if defined(OS_WIN)
// The URL for the Learn More link in the Chrome Cleanup settings card.
extern const char kChromeCleanerLearnMoreURL[];

// The URL for the Windows XP/Vista deprecation help center article.
extern const char kWindowsXPVistaDeprecationURL[];
#endif

#if BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)
// "Learn more" URL for the one click signin infobar.
extern const char kChromeSyncLearnMoreURL[];
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
// The URL for the "Learn more" page for the blocked plugin infobar.
extern const char kBlockedPluginLearnMoreURL[];

// The URL for the "Learn more" page for the outdated plugin infobar.
extern const char kOutdatedPluginLearnMoreURL[];
#endif

#if defined(OS_CHROMEOS)
// The URL for the "Learn more" page for the time zone settings page.
extern const char kTimeZoneSettingsLearnMoreURL[];
#endif

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
