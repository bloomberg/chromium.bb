// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for known URLs and portions thereof.

#ifndef CHROME_COMMON_URL_CONSTANTS_H_
#define CHROME_COMMON_URL_CONSTANTS_H_

#include "build/build_config.h"
#include "content/public/common/url_constants.h"

namespace chrome {

// TODO(msw): Resolve chrome_frame dependency on these constants.
extern const char kAboutPluginsURL[];
extern const char kAboutVersionURL[];

// chrome: URLs (including schemes). Should be kept in sync with the
// components below.
extern const char kChromeUIAboutURL[];
extern const char kChromeUIAppsURL[];
extern const char kChromeUIBookmarksURL[];
extern const char kChromeUICertificateViewerURL[];
extern const char kChromeUIChromeURLsURL[];
extern const char kChromeUICloudPrintResourcesURL[];
extern const char kChromeUIConflictsURL[];
extern const char kChromeUIConstrainedHTMLTestURL[];
extern const char kChromeUICrashesURL[];
extern const char kChromeUICreditsURL[];
extern const char kChromeUIDevToolsURL[];
extern const char kChromeUIDownloadsURL[];
extern const char kChromeUIEditSearchEngineDialogURL[];
extern const char kChromeUIExtensionActivityURL[];
extern const char kChromeUIExtensionIconURL[];
extern const char kChromeUIExtensionInfoURL[];
extern const char kChromeUIExtensionsFrameURL[];
extern const char kChromeUIExtensionsURL[];
extern const char kChromeUIFaviconURL[];
extern const char kChromeUIFeedbackURL[];
extern const char kChromeUIFlagsURL[];
extern const char kChromeUIFlashURL[];
extern const char kChromeUIHelpFrameURL[];
extern const char kChromeUIHistoryURL[];
extern const char kChromeUIHistoryFrameURL[];
extern const char kChromeUIInspectURL[];
extern const char kChromeUIInstantURL[];
extern const char kChromeUIIPCURL[];
extern const char kChromeUIKeyboardURL[];
extern const char kChromeUIManagedUserPassphrasePageURL[];
extern const char kChromeUIMemoryRedirectURL[];
extern const char kChromeUIMemoryURL[];
extern const char kChromeUIMetroFlowURL[];
extern const char kChromeUINaClURL[];
extern const char kChromeUINetInternalsURL[];
extern const char kChromeUINewProfile[];
extern const char kChromeUINewTabURL[];
extern const char kChromeUIOmniboxURL[];
extern const char kChromeUIPerformanceMonitorURL[];
extern const char kChromeUIPluginsURL[];
extern const char kChromeUIPolicyURL[];
extern const char kChromeUIProfileSigninConfirmationURL[];
extern const char kChromeUIPrintURL[];
extern const char kChromeUISessionFaviconURL[];
extern const char kChromeUISettingsURL[];
extern const char kChromeUISettingsFrameURL[];
extern const char kChromeUISuggestionsInternalsURL[];
extern const char kChromeUISSLClientCertificateSelectorURL[];
extern const char kChromeUISyncPromoURL[];
extern const char kChromeUITaskManagerURL[];
extern const char kChromeUITermsURL[];
extern const char kChromeUIThemeURL[];
extern const char kChromeUIThumbnailURL[];
extern const char kChromeUIUberURL[];
extern const char kChromeUIUberFrameURL[];
extern const char kChromeUIUserActionsURL[];
extern const char kChromeUIVersionURL[];

#if defined(OS_ANDROID)
extern const char kChromeUIWelcomeURL[];
#endif

#if defined(OS_CHROMEOS)
extern const char kChromeUIActivationMessage[];
extern const char kChromeUIAppLaunchURL[];
extern const char kChromeUIBluetoothPairingURL[];
extern const char kChromeUIChooseMobileNetworkURL[];
extern const char kChromeUIDiagnosticsURL[];
extern const char kChromeUIDiscardsURL[];
extern const char kChromeUIIdleLogoutDialogURL[];
extern const char kChromeUIImageBurnerURL[];
extern const char kChromeUIKeyboardOverlayURL[];
extern const char kChromeUILockScreenURL[];
extern const char kChromeUIMediaplayerURL[];
extern const char kChromeUIMobileSetupURL[];
extern const char kChromeUIOobeURL[];
extern const char kChromeUIOSCreditsURL[];
extern const char kChromeUIProxySettingsURL[];
extern const char kChromeUISimUnlockURL[];
extern const char kChromeUISlideshowURL[];
extern const char kChromeUISystemInfoURL[];
extern const char kChromeUITermsOemURL[];
extern const char kChromeUIUserImageURL[];
#endif

#if defined(USE_ASH)
extern const char kChromeUITransparencyURL[];
#endif

#if defined(FILE_MANAGER_EXTENSION)
extern const char kChromeUIFileManagerURL[];
#endif

#if defined(USE_AURA)
extern const char kChromeUIGestureConfigURL[];
extern const char kChromeUIGestureConfigHost[];
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
extern const char kChromeUITabModalConfirmDialogURL[];
#endif

// chrome components of URLs. Should be kept in sync with the full URLs above.
extern const char kChromeUIAboutHost[];
extern const char kChromeUIAboutPageFrameHost[];
extern const char kChromeUIBlankHost[];
extern const char kChromeUIAppLauncherPageHost[];
extern const char kChromeUIBookmarksHost[];
extern const char kChromeUICacheHost[];
extern const char kChromeUICertificateViewerHost[];
extern const char kChromeUIChromeURLsHost[];
extern const char kChromeUICloudPrintResourcesHost[];
extern const char kChromeUICloudPrintSetupHost[];
extern const char kChromeUIConflictsHost[];
extern const char kChromeUIConstrainedHTMLTestHost[];
extern const char kChromeUICrashesHost[];
extern const char kChromeUICrashHost[];
extern const char kChromeUICreditsHost[];
extern const char kChromeUIDefaultHost[];
extern const char kChromeUIDevToolsBundledHost[];
extern const char kChromeUIDevToolsRemoteHost[];
extern const char kChromeUIDNSHost[];
extern const char kChromeUIDownloadsHost[];
extern const char kChromeUIDriveInternalsHost[];
extern const char kChromeUIEditSearchEngineDialogHost[];
extern const char kChromeUIExtensionActivityHost[];
extern const char kChromeUIExtensionIconHost[];
extern const char kChromeUIExtensionInfoHost[];
extern const char kChromeUIExtensionsFrameHost[];
extern const char kChromeUIExtensionsHost[];
extern const char kChromeUIFaviconHost[];
extern const char kChromeUIFeedbackHost[];
extern const char kChromeUIFlagsHost[];
extern const char kChromeUIFlashHost[];
extern const char kChromeUIHelpFrameHost[];
extern const char kChromeUIHelpHost[];
extern const char kChromeUIHangHost[];
extern const char kChromeUIHistoryHost[];
extern const char kChromeUIHistoryFrameHost[];
extern const char kChromeUIInspectHost[];
extern const char kChromeUIInstantHost[];
extern const char kChromeUIIPCHost[];
extern const char kChromeUIKeyboardHost[];
extern const char kChromeUIKillHost[];
extern const char kChromeUIManagedUserPassphrasePageHost[];
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
extern const char kChromeUIPolicyHost[];
extern const char kChromeUIProfileSigninConfirmationHost[];
extern const char kChromeUIPredictorsHost[];
extern const char kChromeUIPrintHost[];
extern const char kChromeUIProfilerHost[];
extern const char kChromeUIQuotaInternalsHost[];
extern const char kChromeUISessionFaviconHost[];
extern const char kChromeUISettingsHost[];
extern const char kChromeUISettingsFrameHost[];
extern const char kChromeUIShorthangHost[];
extern const char kChromeUISignInInternalsHost[];
extern const char kChromeUISuggestionsInternalsHost[];
extern const char kChromeUISSLClientCertificateSelectorHost[];
extern const char kChromeUIStatsHost[];
extern const char kChromeUISyncHost[];
extern const char kChromeUISyncInternalsHost[];
extern const char kChromeUISyncPromoHost[];
extern const char kChromeUISyncResourcesHost[];
extern const char kChromeUITaskManagerHost[];
extern const char kChromeUITermsHost[];
extern const char kChromeUIThemeHost[];
extern const char kChromeUIThumbnailHost[];
extern const char kChromeUITouchIconHost[];
extern const char kChromeUIUberFrameHost[];
extern const char kChromeUIUberHost[];
extern const char kChromeUIUserActionsHost[];
extern const char kChromeUIVersionHost[];
extern const char kChromeUIWorkersHost[];

extern const char kChromeUIScreenshotPath[];
extern const char kChromeUIThemePath[];

#if defined(OS_ANDROID)
extern const char kChromeUIWelcomeHost[];
#endif

#if defined(OS_LINUX) || defined(OS_OPENBSD)
extern const char kChromeUILinuxProxyConfigHost[];
extern const char kChromeUISandboxHost[];
#endif

#if defined(OS_CHROMEOS)
extern const char kChromeUIActivationMessageHost[];
extern const char kChromeUIAppLaunchHost[];
extern const char kChromeUIBluetoothPairingHost[];
extern const char kChromeUIChooseMobileNetworkHost[];
extern const char kChromeUICryptohomeHost[];
extern const char kChromeUIDiagnosticsHost[];
extern const char kChromeUIDiscardsHost[];
extern const char kChromeUIIdleLogoutDialogHost[];
extern const char kChromeUIImageBurnerHost[];
extern const char kChromeUIKeyboardOverlayHost[];
extern const char kChromeUILockScreenHost[];
extern const char kChromeUILoginContainerHost[];
extern const char kChromeUILoginHost[];
extern const char kChromeUIMediaplayerHost[];
extern const char kChromeUIMobileSetupHost[];
extern const char kChromeUINetworkHost[];
extern const char kChromeUIOobeHost[];
extern const char kChromeUIOSCreditsHost[];
extern const char kChromeUIProxySettingsHost[];
extern const char kChromeUIRotateHost[];
extern const char kChromeUISimUnlockHost[];
extern const char kChromeUISlideshowHost[];
extern const char kChromeUISystemInfoHost[];
extern const char kChromeUIUserImageHost[];

extern const char kChromeUIMenu[];
extern const char kChromeUINetworkMenu[];
extern const char kChromeUIWrenchMenu[];

extern const char kEULAPathFormat[];
extern const char kOemEulaURLPath[];
#endif

#if defined(USE_ASH)
extern const char kChromeUITransparencyHost[];
#endif

#if defined(FILE_MANAGER_EXTENSION)
extern const char kChromeUIFileManagerHost[];
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
extern const char kChromeUITabModalConfirmDialogHost[];
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
extern const char kManagedUserSettingsSubPage[];
extern const char kManageProfileSubPage[];
extern const char kPasswordManagerSubPage[];
extern const char kSearchEnginesSubPage[];
extern const char kSearchSubPage[];
extern const char kSearchUsersSubPage[];
extern const char kSyncSetupForceLoginSubPage[];
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

// General help links for Chrome, opened using various actions.
extern const char kChromeHelpViaKeyboardURL[];
extern const char kChromeHelpViaMenuURL[];
extern const char kChromeHelpViaWebUIURL[];

#if defined(OS_CHROMEOS)
// Accessibility help link for Chrome.
extern const char kChromeAccessibilityHelpURL[];
#endif

// "Learn more" URL for the one click signin infobar.
extern const char kChromeSyncLearnMoreURL[];

// Help URL for the settings page's search feature.
extern const char kSettingsSearchHelpURL[];

// "About" URL for the translate bar's options menu.
extern const char kAboutGoogleTranslateURL[];

// Help URL for the Omnibox setting.
extern const char kOmniboxLearnMoreURL[];

// "Learn more" URL for the Instant feature.
extern const char kInstantLearnMoreURL[];

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

// The URL for the Chromium project used in the About dialog.
extern const char kChromiumProjectURL[];

// The URL for the "Learn more" page for the usage/crash reporting option in the
// first run dialog.
extern const char kLearnMoreReportingURL[];

// The URL for the "Learn more" page for the outdated plugin infobar.
extern const char kOutdatedPluginLearnMoreURL[];

// The URL for the "Learn more" page for the blocked plugin infobar.
extern const char kBlockedPluginLearnMoreURL[];

// The URL for the "About Voice Recognition" menu item.
extern const char kSpeechInputAboutURL[];

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

// The URL for information on how to recover your password.
extern const char kInvalidPasswordHelpURL[];

// The URL for information on what to do if you can't sign in to your Google
// account.
extern const char kCanNotAccessAccountURL[];

// The URL for the "Learn more" page on sync encryption.
extern const char kSyncEncryptionHelpURL[];

// The URL for the "Learn more" link when there is a sync error.
extern const char kSyncErrorsHelpURL[];

// The URL to create a new Google account via sync.
extern const char kSyncCreateNewAccountURL[];

// The URL for the "Learn more" link in the Chrome To Mobile bubble.
extern const char kChromeToMobileLearnMoreURL[];

// The URL for the help article explaining sideload wipeout in more details.
extern const char kSideloadWipeoutHelpURL[];

#if defined(OS_CHROMEOS)
// The URL for the "Learn more" link for natural scrolling on ChromeOS.
extern const char kNaturalScrollHelpURL[];

// The URL for the Learn More page about enterprise enrolled devices.
extern const char kLearnMoreEnterpriseURL[];
#endif

// "Debug" pages which are dangerous and not for general consumption.
extern const char* const kChromeDebugURLs[];
extern const int kNumberOfChromeDebugURLs;

// Canonical schemes you can use as input to GURL.SchemeIs().
extern const char kExtensionResourceScheme[];

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

// The local omnibox host and pages under chrome-search.
extern const char kChromeSearchLocalOmniboxPopupHost[];
extern const char kChromeSearchLocalOmniboxPopupURL[];
extern const char kChromeSearchLocalNtpHost[];
extern const char kChromeSearchLocalNtpUrl[];

// Host for search suggestions iframes.
extern const char kChromeSearchSuggestionHost[];

#if defined(OS_CHROMEOS)
extern const char kCrosScheme[];
extern const char kDriveScheme[];

// "Learn more" URL for the Cloud Print section under Options.
extern const char kCloudPrintLearnMoreURL[];
#endif

// Parameters that get appended to force SafeSearch.
extern const char kSafeSearchSafeParameter[];
extern const char kSafeSearchSsuiParameter[];

// The URL for the "Learn more" link in the media access infobar.
extern const char kMediaAccessLearnMoreUrl[];

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
