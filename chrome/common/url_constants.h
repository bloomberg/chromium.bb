// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for known URLs and portions thereof.

#ifndef CHROME_COMMON_URL_CONSTANTS_H_
#define CHROME_COMMON_URL_CONSTANTS_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "content/public/common/url_constants.h"

namespace chrome {

// chrome: URLs (including schemes). Should be kept in sync with the
// components below.
extern const char kChromeUIAboutURL[];
extern const char kChromeUIAppsURL[];
extern const char kChromeUIAppListStartPageURL[];
extern const char kChromeUIBookmarksURL[];
extern const char kChromeUICertificateViewerURL[];
extern const char kChromeUICertificateViewerDialogURL[];
extern const char kChromeUIChromeSigninURL[];
extern const char kChromeUIChromeURLsURL[];
extern const char kChromeUICloudPrintResourcesURL[];
extern const char kChromeUIComponentsURL[];
extern const char kChromeUIConflictsURL[];
extern const char kChromeUIConstrainedHTMLTestURL[];
extern const char kChromeUIContextualSearchPromoURL[];
extern const char kChromeUICrashesURL[];
extern const char kChromeUICreditsURL[];
extern const char kChromeUIDevicesURL[];
extern const char kChromeUIDevToolsURL[];
extern const char kChromeUIDomainReliabilityInternalsURL[];
extern const char kChromeUIDownloadsURL[];
extern const char kChromeUIEditSearchEngineDialogURL[];
extern const char kChromeUIExtensionIconURL[];
extern const char kChromeUIExtensionInfoURL[];
extern const char kChromeUIExtensionsFrameURL[];
extern const char kChromeUIExtensionsURL[];
extern const char kChromeUIFaviconURL[];
extern const char kChromeUIFeedbackURL[];
extern const char kChromeUIFlagsURL[];
extern const char kChromeUIFlashURL[];
extern const char kChromeUIGCMInternalsURL[];
extern const char kChromeUIHelpFrameURL[];
extern const char kChromeUIHistoryURL[];
extern const char kChromeUIHistoryFrameURL[];
extern const char kChromeUIIdentityInternalsURL[];
extern const char kChromeUIInspectURL[];
extern const char kChromeUIInstantURL[];
extern const char kChromeUIInvalidationsURL[];
extern const char kChromeUIIPCURL[];
extern const char kChromeUIMemoryRedirectURL[];
extern const char kChromeUIMemoryURL[];
extern const char kChromeUIMetroFlowURL[];
extern const char kChromeUINaClURL[];
extern const char kChromeUINetInternalsURL[];
extern const char kChromeUINewProfile[];
extern const char kChromeUINewTabURL[];
extern const char kChromeUIOmniboxURL[];
extern const char kChromeUIPasswordManagerInternalsHost[];
extern const char kChromeUIPerformanceMonitorURL[];
extern const char kChromeUIPluginsURL[];
extern const char kChromeUIPolicyURL[];
extern const char kChromeUIProfileSigninConfirmationURL[];
extern const char kChromeUIUserManagerURL[];
extern const char kChromeUIPrintURL[];
extern const char kChromeUIQuitURL[];
extern const char kChromeUIRestartURL[];
extern const char kChromeUISessionFaviconURL[];
extern const char kChromeUISettingsURL[];
extern const char kChromeUISettingsFrameURL[];
extern const char kChromeUISuggestions[];
extern const char kChromeUISuggestionsInternalsURL[];
extern const char kChromeUISupervisedUserPassphrasePageURL[];
extern const char kChromeUISSLClientCertificateSelectorURL[];
extern const char kChromeUITermsURL[];
extern const char kChromeUIThemeURL[];
extern const char kChromeUIThumbnailURL[];
extern const char kChromeUIThumbnailListURL[];
extern const char kChromeUIUberURL[];
extern const char kChromeUIUberFrameURL[];
extern const char kChromeUIUserActionsURL[];
extern const char kChromeUIVersionURL[];
extern const char kChromeUIVoiceSearchURL[];

#if defined(OS_ANDROID)
extern const char kChromeUINativeNewTabURL[];
extern const char kChromeUINativeBookmarksURL[];
extern const char kChromeUINativeRecentTabsURL[];
extern const char kChromeUIWelcomeURL[];
#endif

#if defined(OS_CHROMEOS)
extern const char kChromeUIActivationMessage[];
extern const char kChromeUIBluetoothPairingURL[];
extern const char kChromeUICertificateManagerDialogURL[];
extern const char kChromeUIChargerReplacementURL[];
extern const char kChromeUIChooseMobileNetworkURL[];
extern const char kChromeUIDiagnosticsURL[];
extern const char kChromeUIDiscardsURL[];
extern const char kChromeUIFirstRunURL[];
extern const char kChromeUIIdleLogoutDialogURL[];
extern const char kChromeUIImageBurnerURL[];
extern const char kChromeUIKeyboardOverlayURL[];
extern const char kChromeUILockScreenURL[];
extern const char kChromeUIMediaplayerURL[];
extern const char kChromeUIMobileSetupURL[];
extern const char kChromeUINfcDebugURL[];
extern const char kChromeUIOobeURL[];
extern const char kChromeUIOSCreditsURL[];
extern const char kChromeUIProxySettingsURL[];
extern const char kChromeUIScreenlockIconURL[];
extern const char kChromeUISetTimeURL[];
extern const char kChromeUISimUnlockURL[];
extern const char kChromeUISlideshowURL[];
extern const char kChromeUISlowURL[];
extern const char kChromeUISystemInfoURL[];
extern const char kChromeUITermsOemURL[];
extern const char kChromeUIUserImageURL[];
#endif

#if defined(USE_AURA)
extern const char kChromeUIGestureConfigURL[];
extern const char kChromeUIGestureConfigHost[];
extern const char kChromeUISalsaURL[];
extern const char kChromeUISalsaHost[];
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
extern const char kChromeUITabModalConfirmDialogURL[];
#endif

#if defined(ENABLE_WEBRTC)
extern const char kChromeUIWebRtcLogsURL[];
#endif

// chrome components of URLs. Should be kept in sync with the full URLs above.
extern const char kChromeUIAboutHost[];
extern const char kChromeUIAboutPageFrameHost[];
extern const char kChromeUIBlankHost[];
extern const char kChromeUIAppLauncherPageHost[];
extern const char kChromeUIAppListStartPageHost[];
extern const char kChromeUIBookmarksHost[];
extern const char kChromeUICacheHost[];
extern const char kChromeUICertificateViewerHost[];
extern const char kChromeUICertificateViewerDialogHost[];
extern const char kChromeUIChromeSigninHost[];
extern const char kChromeUIChromeURLsHost[];
extern const char kChromeUICloudPrintResourcesHost[];
extern const char kChromeUICloudPrintSetupHost[];
extern const char kChromeUIConflictsHost[];
extern const char kChromeUIConstrainedHTMLTestHost[];
extern const char kChromeUIContextualSearchPromoHost[];
extern const char kChromeUICrashesHost[];
extern const char kChromeUICrashHost[];
extern const char kChromeUICreditsHost[];
extern const char kChromeUIDefaultHost[];
extern const char kChromeUIDevicesHost[];
extern const char kChromeUIDevToolsHost[];
extern const char kChromeUIDevToolsBundledPath[];
extern const char kChromeUIDevToolsRemotePath[];
extern const char kChromeUIDNSHost[];
extern const char kChromeUIDomainReliabilityInternalsHost[];
extern const char kChromeUIDownloadsHost[];
extern const char kChromeUIDriveInternalsHost[];
extern const char kChromeUIEditSearchEngineDialogHost[];
extern const char kChromeUIExtensionIconHost[];
extern const char kChromeUIExtensionInfoHost[];
extern const char kChromeUIExtensionsFrameHost[];
extern const char kChromeUIExtensionsHost[];
extern const char kChromeUIFaviconHost[];
extern const char kChromeUIFeedbackHost[];
extern const char kChromeUIFlagsHost[];
extern const char kChromeUIFlashHost[];
extern const char kChromeUIGCMInternalsHost[];
extern const char kChromeUIHelpFrameHost[];
extern const char kChromeUIHelpHost[];
extern const char kChromeUIHangHost[];
extern const char kChromeUIHistoryHost[];
extern const char kChromeUIHistoryFrameHost[];
extern const char kChromeUIIdentityInternalsHost[];
extern const char kChromeUIInspectHost[];
extern const char kChromeUIInstantHost[];
extern const char kChromeUIInvalidationsHost[];
extern const char kChromeUIIPCHost[];
extern const char kChromeUIKillHost[];
extern const char kChromeUIMemoryHost[];
extern const char kChromeUIMemoryInternalsHost[];
extern const char kChromeUIMemoryRedirectHost[];
extern const char kChromeUIMetroFlowHost[];
extern const char kChromeUINaClHost[];
extern const char kChromeUINetExportHost[];
extern const char kChromeUINetInternalsHost[];
extern const char kChromeUINewTabHost[];
extern const char kChromeUIOmniboxHost[];
extern const char kChromeUIPerformanceMonitorHost[];
extern const char kChromeUIPluginsHost[];
extern const char kChromeUIComponentsHost[];
extern const char kChromeUIPolicyHost[];
extern const char kChromeUIProfileSigninConfirmationHost[];
extern const char kChromeUIProvidedFileSystemsHost[];
extern const char kChromeUIUserManagerHost[];
extern const char kChromeUIPredictorsHost[];
extern const char kChromeUIPrintHost[];
extern const char kChromeUIProfilerHost[];
extern const char kChromeUIQuotaInternalsHost[];
extern const char kChromeUIQuitHost[];
extern const char kChromeUIRestartHost[];
extern const char kChromeUISessionFaviconHost[];
extern const char kChromeUISettingsHost[];
extern const char kChromeUISettingsFrameHost[];
extern const char kChromeUIShorthangHost[];
extern const char kChromeUISignInInternalsHost[];
extern const char kChromeUISuggestionsHost[];
extern const char kChromeUISuggestionsInternalsHost[];
extern const char kChromeUISSLClientCertificateSelectorHost[];
extern const char kChromeUIStatsHost[];
extern const char kChromeUISupervisedUserPassphrasePageHost[];
extern const char kChromeUISyncHost[];
extern const char kChromeUISyncFileSystemInternalsHost[];
extern const char kChromeUISyncInternalsHost[];
extern const char kChromeUISyncResourcesHost[];
extern const char kChromeUISystemInfoHost[];
extern const char kChromeUITaskManagerHost[];
extern const char kChromeUITermsHost[];
extern const char kChromeUIThemeHost[];
extern const char kChromeUIThumbnailHost[];
extern const char kChromeUIThumbnailHost2[];
extern const char kChromeUIThumbnailListHost[];
extern const char kChromeUITouchIconHost[];
extern const char kChromeUITranslateInternalsHost[];
extern const char kChromeUIUberFrameHost[];
extern const char kChromeUIUberHost[];
extern const char kChromeUIUserActionsHost[];
extern const char kChromeUIVersionHost[];
extern const char kChromeUIVoiceSearchHost[];
extern const char kChromeUIWorkersHost[];

extern const char kChromeUIScreenshotPath[];
extern const char kChromeUIThemePath[];

#if defined(OS_ANDROID)
extern const char kChromeUIWelcomeHost[];
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
extern const char kChromeUILinuxProxyConfigHost[];
extern const char kChromeUISandboxHost[];
#endif

#if defined(OS_CHROMEOS)
extern const char kChromeUIActivationMessageHost[];
extern const char kChromeUIAppLaunchHost[];
extern const char kChromeUIBluetoothPairingHost[];
extern const char kChromeUICertificateManagerHost[];
extern const char kChromeUIChargerReplacementHost[];
extern const char kChromeUIChooseMobileNetworkHost[];
extern const char kChromeUICryptohomeHost[];
extern const char kChromeUIDiagnosticsHost[];
extern const char kChromeUIDiscardsHost[];
extern const char kChromeUIFirstRunHost[];
extern const char kChromeUIIdleLogoutDialogHost[];
extern const char kChromeUIImageBurnerHost[];
extern const char kChromeUIKeyboardOverlayHost[];
extern const char kChromeUILockScreenHost[];
extern const char kChromeUILoginContainerHost[];
extern const char kChromeUILoginHost[];
extern const char kChromeUIMediaplayerHost[];
extern const char kChromeUIMobileSetupHost[];
extern const char kChromeUINetworkHost[];
extern const char kChromeUINfcDebugHost[];
extern const char kChromeUIOobeHost[];
extern const char kChromeUIOSCreditsHost[];
extern const char kChromeUIPowerHost[];
extern const char kChromeUIProxySettingsHost[];
extern const char kChromeUIRotateHost[];
extern const char kChromeUIScreenlockIconHost[];
extern const char kChromeUISetTimeHost[];
extern const char kChromeUISimUnlockHost[];
extern const char kChromeUISlideshowHost[];
extern const char kChromeUISlowHost[];
extern const char kChromeUISlowTraceHost[];
extern const char kChromeUIUserImageHost[];

extern const char kChromeUIMenu[];
extern const char kChromeUINetworkMenu[];
extern const char kChromeUIWrenchMenu[];

extern const char kEULAPathFormat[];
extern const char kOemEulaURLPath[];
extern const char kOnlineEulaURLPath[];
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
extern const char kChromeUITabModalConfirmDialogHost[];
#endif

#if defined(ENABLE_WEBRTC)
extern const char kChromeUIWebRtcLogsHost[];
#endif

// Options sub-pages.
extern const char kAutofillSubPage[];
extern const char kClearBrowserDataSubPage[];
extern const char kContentSettingsExceptionsSubPage[];
extern const char kContentSettingsSubPage[];
extern const char kCreateProfileSubPage[];
extern const char kExtensionsSubPage[];
extern const char kHandlerSettingsSubPage[];
extern const char kImportDataSubPage[];
extern const char kLanguageOptionsSubPage[];
extern const char kManageProfileSubPage[];
extern const char kPasswordManagerSubPage[];
extern const char kResetProfileSettingsSubPage[];
extern const char kSearchEnginesSubPage[];
extern const char kSearchSubPage[];
extern const char kSearchUsersSubPage[];
extern const char kSupervisedUserSettingsSubPage[];
extern const char kSyncSetupSubPage[];
#if defined(OS_CHROMEOS)
extern const char kInternetOptionsSubPage[];
extern const char kBluetoothAddDeviceSubPage[];
extern const char kChangeProfilePictureSubPage[];
#endif

// Extensions sub pages.
extern const char kExtensionConfigureCommandsSubPage[];

// URLs used to indicate that an extension resource load request
// was invalid.
extern const char kExtensionInvalidRequestURL[];
extern const char kExtensionResourceInvalidRequestURL[];

extern const char kSyncGoogleDashboardURL[];

// "Learn more" URL for the auto password generation.
extern const char kAutoPasswordGenerationLearnMoreURL[];

extern const char kPasswordManagerLearnMoreURL[];

// "Learn more" URL for the Settings API, NTP bubble and other settings bubbles
// showing which extension is controlling them.
extern const char kExtensionControlledSettingLearnMoreURL[];

// General help links for Chrome, opened using various actions.
extern const char kChromeHelpViaKeyboardURL[];
extern const char kChromeHelpViaMenuURL[];
extern const char kChromeHelpViaWebUIURL[];

#if defined(OS_CHROMEOS)
// Accessibility help link for Chrome.
extern const char kChromeAccessibilityHelpURL[];
// Accessibility settings link for Chrome.
extern const char kChromeAccessibilitySettingsURL[];
#endif

#if defined (ENABLE_ONE_CLICK_SIGNIN)
// "Learn more" URL for the one click signin infobar.
extern const char kChromeSyncLearnMoreURL[];

// "Learn more" URL for the "Sign in with a different account" confirmation
// dialog.
extern const char kChromeSyncMergeTroubleshootingURL[];
#endif

// "Learn more" URL for the enterprise sign-in confirmation dialog.
extern const char kChromeEnterpriseSignInLearnMoreURL[];

// "Learn more" URL for resetting profile preferences.
extern const char kResetProfileSettingsLearnMoreURL[];

// "Learn more" URL for when profile settings are automatically reset.
extern const char kAutomaticSettingsResetLearnMoreURL[];

// Management URL for the supervised users.
extern const char kSupervisedUserManagementURL[];

// Management URL for the supervised users - version without scheme, used
// for display.
extern const char kSupervisedUserManagementDisplayURL[];

// Help URL for the settings page's search feature.
extern const char kSettingsSearchHelpURL[];

// Help URL for the Omnibox setting.
extern const char kOmniboxLearnMoreURL[];

// "What do these mean?" URL for the Page Info bubble.
extern const char kPageInfoHelpCenterURL[];

// "Learn more" URL for "Aw snap" page.
extern const char kCrashReasonURL[];

// "Learn more" URL for killed tab page.
extern const char kKillReasonURL[];

// "Learn more" URL for the Privacy section under Options.
extern const char kPrivacyLearnMoreURL[];

// "Learn more" URL for the "Do not track" setting in the privacy section.
extern const char kDoNotTrackLearnMoreURL[];

#if defined(OS_CHROMEOS)
// These URLs are currently ChromeOS only.

// "Learn more" URL for the attestation of content protection setting.
extern const char kAttestationForContentProtectionLearnMoreURL[];

// "Learn more" URL for the enhanced playback notification dialog.
extern const char kEnhancedPlaybackNotificationLearnMoreURL[];
#endif

// The URL for the Chromium project used in the About dialog.
extern const char kChromiumProjectURL[];

// The URL for the "Learn more" page for the usage/crash reporting option in the
// first run dialog.
extern const char kLearnMoreReportingURL[];

// The URL for the "Learn more" page for the outdated plugin infobar.
extern const char kOutdatedPluginLearnMoreURL[];

// The URL for the "Learn more" page for the blocked plugin infobar.
extern const char kBlockedPluginLearnMoreURL[];

// The URL for the "Learn more" page for hotword search voice trigger.
extern const char kHotwordLearnMoreURL[];

// The URL for the "Learn more" page for register protocol handler infobars.
extern const char kLearnMoreRegisterProtocolHandlerURL[];

// The URL for the "Learn more" page for sync setup on the personal stuff page.
extern const char kSyncLearnMoreURL[];

// The URL for the "Learn more" page for download scanning.
extern const char kDownloadScanningLearnMoreURL[];

// The URL for the "Learn more" page for interrupted downloads.
extern const char kDownloadInterruptedLearnMoreURL[];

// The URL for the "Learn more" page on the sync setup dialog, when syncing
// everything.
extern const char kSyncEverythingLearnMoreURL[];

// The URL for information on how to use the app launcher.
extern const char kAppLauncherHelpURL[];

// The URL for the "Learn more" page on sync encryption.
extern const char kSyncEncryptionHelpURL[];

// The URL for the "Learn more" link when there is a sync error.
extern const char kSyncErrorsHelpURL[];

#if defined(OS_CHROMEOS)
// The URL for the "Learn more" link for natural scrolling on ChromeOS.
extern const char kNaturalScrollHelpURL[];

// The URL for the Learn More page about enterprise enrolled devices.
extern const char kLearnMoreEnterpriseURL[];
#endif

// The URL for the Learn More link of the non-CWS bubble.
extern const char kRemoveNonCWSExtensionURL[];

// The URL for the Learn More link for the corrupt extension message.
extern const char kCorruptExtensionURL[];

extern const char kNotificationsHelpURL[];

// The Welcome Notification More Info URL.
extern const char kNotificationWelcomeLearnMoreURL[];

// Gets the hosts/domains that are shown in chrome://chrome-urls.
extern const char* const kChromeHostURLs[];
extern const size_t kNumberOfChromeHostURLs;

// "Debug" pages which are dangerous and not for general consumption.
extern const char* const kChromeDebugURLs[];
extern const int kNumberOfChromeDebugURLs;

// The chrome-native: scheme is used show pages rendered with platform specific
// widgets instead of using HTML.
extern const char kChromeNativeScheme[];

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

// Pages under chrome-search.
extern const char kChromeSearchLocalNtpHost[];
extern const char kChromeSearchLocalNtpUrl[];
extern const char kChromeSearchRemoteNtpHost[];

// Host and URL for most visited iframes used on the Instant Extended NTP.
extern const char kChromeSearchMostVisitedHost[];
extern const char kChromeSearchMostVisitedUrl[];

#if defined(OS_CHROMEOS)
extern const char kCrosScheme[];
extern const char kDriveScheme[];
#endif

// Scheme for the DOM Distiller component.
extern const char kDomDistillerScheme[];

// "Learn more" URL for the Cloud Print section under Options.
extern const char kCloudPrintLearnMoreURL[];

// "Learn more" URL for the Cloud Print Preview No Destinations Promotion.
extern const char kCloudPrintNoDestinationsLearnMoreURL[];

// Parameters that get appended to force SafeSearch.
extern const char kSafeSearchSafeParameter[];
extern const char kSafeSearchSsuiParameter[];

// The URL for the "Learn more" link in the media access infobar.
extern const char kMediaAccessLearnMoreUrl[];

// The URL for the "Learn more" link in the language settings.
extern const char kLanguageSettingsLearnMoreUrl[];

#if defined(OS_MACOSX)
// The URL for the 32-bit Mac deprecation help center article
extern const char kMac32BitDeprecationURL[];
#endif

// The URL for the "Learn more" link the the Easy Unlock settings.
extern const char kEasyUnlockLearnMoreUrl[];

// The URL to the device management page in the Easy Unlock settings.
extern const char kEasyUnlockManagementUrl[];

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
