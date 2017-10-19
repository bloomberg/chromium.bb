// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for WebUI UI/Host/SubPage constants. Anything else go in
// chrome/common/url_constants.h.

#ifndef CHROME_COMMON_WEBUI_URL_CONSTANTS_H_
#define CHROME_COMMON_WEBUI_URL_CONSTANTS_H_

#include <stddef.h>

#include "build/build_config.h"
#include "chrome/common/features.h"
#include "content/public/common/url_constants.h"
#include "media/media_features.h"
#include "printing/features/features.h"

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
extern const char kChromeUIComponentsURL[];
extern const char kChromeUIConflictsURL[];
extern const char kChromeUIConstrainedHTMLTestURL[];
extern const char kChromeUICrashesURL[];
extern const char kChromeUICreditsURL[];
extern const char kChromeUIDevicesURL[];
extern const char kChromeUIDevToolsCustomURL[];
extern const char kChromeUIDevToolsURL[];
extern const char kChromeUIDomainReliabilityInternalsURL[];
extern const char kChromeUIDownloadsURL[];
extern const char kChromeUIExtensionIconURL[];
extern const char kChromeUIExtensionsFrameURL[];
extern const char kChromeUIExtensionsURL[];
extern const char kChromeUIFallbackIconURL[];
extern const char kChromeUIFaviconURL[];
extern const char kChromeUIFeedbackURL[];
extern const char kChromeUIFlagsURL[];
extern const char kChromeUIFlashURL[];
extern const char kChromeUIGCMInternalsURL[];
// TODO(dbeam): remove help-frame.
extern const char kChromeUIHelpFrameURL[];
extern const char kChromeUIHelpURL[];
extern const char kChromeUIHistoryURL[];
extern const char kDeprecatedChromeUIHistoryFrameURL[];
extern const char kChromeUIIdentityInternalsURL[];
extern const char kChromeUIInspectURL[];
extern const char kChromeUIInstantURL[];
extern const char kChromeUIInterstitialURL[];
extern const char kChromeUIInterventionsInternalsURL[];
extern const char kChromeUIInvalidationsURL[];
extern const char kChromeUIPolicyToolURL[];
extern const char kChromeUIMediaEngagementHost[];
extern const char kChromeUIMemoryInternalsURL[];
extern const char kChromeUINaClURL[];
extern const char kChromeUINetInternalsURL[];
extern const char kChromeUINewProfileURL[];
extern const char kChromeUINewTabURL[];
extern const char kChromeUINTPTilesInternalsURL[];
extern const char kChromeUIOmniboxURL[];
extern const char kChromeUIPasswordManagerInternalsHost[];
extern const char kChromeUIPolicyURL[];
extern const char kChromeUIMdUserManagerUrl[];
extern const char kChromeUIPrintURL[];
extern const char kChromeUIQuitURL[];
extern const char kChromeUIRestartURL[];
extern const char kChromeUISettingsURL[];
extern const char kChromeUIContentSettingsURL[];
extern const char kChromeUISigninEmailConfirmationURL[];
extern const char kChromeUISigninErrorURL[];
extern const char kChromeUISiteDetailsPrefixURL[];
extern const char kChromeUISiteEngagementHost[];
extern const char kChromeUISuggestionsURL[];
extern const char kChromeUISupervisedUserPassphrasePageURL[];
extern const char kChromeUISyncConfirmationURL[];
extern const char kChromeUITermsURL[];
extern const char kChromeUIThemeURL[];
extern const char kChromeUIThumbnailURL[];
extern const char kChromeUIThumbnailListURL[];
extern const char kChromeUIUberFrameURL[];
extern const char kChromeUIUserActionsURL[];
extern const char kChromeUIVersionURL[];
extern const char kChromeUIWelcomeURL[];
extern const char kChromeUIWelcomeWin10URL[];

#if defined(OS_ANDROID)
extern const char kChromeUIContextualSearchPromoURL[];
extern const char kChromeUIJavaCrashURL[];
extern const char kChromeUINativeScheme[];
extern const char kChromeUINativeNewTabURL[];
extern const char kChromeUINativeBookmarksURL[];
extern const char kChromeUINativePhysicalWebDiagnosticsURL[];
extern const char kChromeUINativeRecentTabsURL[];
extern const char kChromeUINativeHistoryURL[];
extern const char kChromeUIWebApksURL[];
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
extern const char kChromeUIBluetoothPairingURL[];
extern const char kChromeUICertificateManagerDialogURL[];
extern const char kChromeUIChooseMobileNetworkURL[];
extern const char kChromeUIDeviceEmulatorURL[];
extern const char kChromeUIFirstRunURL[];
extern const char kChromeUIKeyboardOverlayURL[];
extern const char kChromeUIMobileSetupURL[];
extern const char kChromeUIOobeURL[];
extern const char kChromeUIOSCreditsURL[];
extern const char kChromeUIIntenetConfigDialogURL[];
extern const char kChromeUIIntenetDetailDialogURL[];
extern const char kChromeUIScreenlockIconURL[];
extern const char kChromeUISetTimeURL[];
extern const char kChromeUISimUnlockURL[];
extern const char kChromeUISlowURL[];
extern const char kChromeUISysInternalsURL[];
extern const char kChromeUISystemInfoURL[];
extern const char kChromeUITermsOemURL[];
extern const char kChromeUIUserImageURL[];
extern const char kChromeUIMdCupsSettingsURL[];
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
extern const char kChromeUIMetroFlowURL[];
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
extern const char kChromeUITabModalConfirmDialogURL[];
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
extern const char kChromeUIWebRtcLogsURL[];
#endif

extern const char kChromeUIMediaRouterURL[];
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
extern const char kChromeUICastURL[];
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
extern const char kChromeUIDiscardsURL[];
#endif

// chrome components of URLs. Should be kept in sync with the full URLs above.
extern const char kChromeUIAboutHost[];
extern const char kChromeUIAboutPageFrameHost[];
extern const char kChromeUIBlankHost[];
extern const char kChromeUIAppLauncherPageHost[];
extern const char kChromeUIAppListStartPageHost[];
extern const char kChromeUIBluetoothInternalsHost[];
extern const char kChromeUIBookmarksHost[];
extern const char kChromeUICacheHost[];
extern const char kChromeUICertificateViewerHost[];
extern const char kChromeUICertificateViewerDialogHost[];
extern const char kChromeUIChromeSigninHost[];
extern const char kChromeUIChromeURLsHost[];
extern const char kChromeUIComponentsHost[];
extern const char kChromeUIConflictsHost[];
extern const char kChromeUIConstrainedHTMLTestHost[];
extern const char kChromeUICrashesHost[];
extern const char kChromeUICrashHost[];
extern const char kChromeUICreditsHost[];
extern const char kChromeUIDefaultHost[];
extern const char kChromeUIDelayedHangUIHost[];
extern const char kChromeUIDeviceLogHost[];
extern const char kChromeUIDevicesHost[];
extern const char kChromeUIDevToolsHost[];
extern const char kChromeUIDevToolsBlankPath[];
extern const char kChromeUIDevToolsBundledPath[];
extern const char kChromeUIDevToolsCustomPath[];
extern const char kChromeUIDevToolsRemotePath[];
extern const char kChromeUIDNSHost[];
extern const char kChromeUIDomainReliabilityInternalsHost[];
extern const char kChromeUIDownloadsHost[];
extern const char kChromeUIDownloadInternalsHost[];
extern const char kChromeUIDriveInternalsHost[];
extern const char kChromeUIExtensionIconHost[];
extern const char kChromeUIExtensionsFrameHost[];
extern const char kChromeUIExtensionsHost[];
extern const char kChromeUIFallbackIconHost[];
extern const char kChromeUIFaviconHost[];
extern const char kChromeUIFeedbackHost[];
extern const char kChromeUIFlagsHost[];
extern const char kChromeUIFlashHost[];
extern const char kChromeUIGCMInternalsHost[];
// TODO(dbeam): remove help-frame.
extern const char kChromeUIHelpFrameHost[];
extern const char kChromeUIHelpHost[];
extern const char kChromeUIHangHost[];
extern const char kChromeUIHangUIHost[];
extern const char kChromeUIHistoryHost[];
extern const char kDeprecatedChromeUIHistoryFrameHost[];
extern const char kChromeUIIdentityInternalsHost[];
extern const char kChromeUIInspectHost[];
extern const char kChromeUIInstantHost[];
extern const char kChromeUIInterstitialHost[];
extern const char kChromeUIInterventionsInternalsHost[];
extern const char kChromeUIInvalidationsHost[];
extern const char kChromeUIKillHost[];
extern const char kChromeUILargeIconHost[];
extern const char kChromeUILocalStateHost[];
extern const char kChromeUIMemoryInternalsHost[];
extern const char kChromeUIPolicyToolHost[];
extern const char kChromeUINaClHost[];
extern const char kChromeUINetExportHost[];
extern const char kChromeUINetInternalsHost[];
extern const char kChromeUINewTabHost[];
extern const char kChromeUINTPTilesInternalsHost[];
extern const char kChromeUIOfflineInternalsHost[];
extern const char kChromeUIOmniboxHost[];
extern const char kChromeUIPhysicalWebHost[];
extern const char kChromeUIPolicyHost[];
extern const char kChromeUIPrefsInternalsHost[];
extern const char kChromeUIMdUserManagerHost[];
extern const char kChromeUIPredictorsHost[];
extern const char kChromeUIQuotaInternalsHost[];
extern const char kChromeUIQuitHost[];
extern const char kChromeUIRestartHost[];
extern const char kChromeUISettingsHost[];
extern const char kChromeUIShorthangHost[];
extern const char kChromeUISigninEmailConfirmationHost[];
extern const char kChromeUISigninErrorHost[];
extern const char kChromeUISignInInternalsHost[];
extern const char kChromeUISuggestionsHost[];
extern const char kChromeUISupervisedUserInternalsHost[];
extern const char kChromeUISupervisedUserPassphrasePageHost[];
extern const char kChromeUISyncConfirmationHost[];
extern const char kChromeUISyncHost[];
extern const char kChromeUISyncFileSystemInternalsHost[];
extern const char kChromeUISyncInternalsHost[];
extern const char kChromeUISyncResourcesHost[];
extern const char kChromeUISystemInfoHost[];
extern const char kChromeUITaskSchedulerInternalsHost[];
extern const char kChromeUITermsHost[];
extern const char kChromeUIThemeHost[];
extern const char kChromeUIThumbnailHost[];
extern const char kChromeUIThumbnailHost2[];
extern const char kChromeUIThumbnailListHost[];
extern const char kChromeUITranslateInternalsHost[];
extern const char kChromeUIUberFrameHost[];
extern const char kChromeUIUberHost[];
extern const char kChromeUIUsbInternalsHost[];
extern const char kChromeUIUserActionsHost[];
extern const char kChromeUIVersionHost[];
extern const char kChromeUIWelcomeHost[];
extern const char kChromeUIWelcomeWin10Host[];
extern const char kChromeUIWorkersHost[];

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
extern const char kChromeUIPrintHost[];
#endif  // ENABLE_PRINT_PREVIEW

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
extern const char kChromeUIDiscardsHost[];
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
extern const char kChromeUILinuxProxyConfigHost[];
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
extern const char kChromeUISandboxHost[];
#endif

#if defined(OS_ANDROID)
extern const char kChromeUIContextualSearchPromoHost[];
extern const char kChromeUIOfflineInternalsURL[];
extern const char kChromeUIPhysicalWebDiagnosticsHost[];
extern const char kChromeUISnippetsInternalsHost[];
extern const char kChromeUIWebApksHost[];
#endif

#if defined(OS_CHROMEOS)
extern const char kChromeUIActivationMessageHost[];
extern const char kChromeUIAppLaunchHost[];
extern const char kChromeUIBluetoothPairingHost[];
extern const char kChromeUICertificateManagerHost[];
extern const char kChromeUIChooseMobileNetworkHost[];
extern const char kChromeUICryptohomeHost[];
extern const char kChromeUIDeviceEmulatorHost[];
extern const char kChromeUIFirstRunHost[];
extern const char kChromeUIKeyboardOverlayHost[];
extern const char kChromeUILoginContainerHost[];
extern const char kChromeUILoginHost[];
extern const char kChromeUIMobileSetupHost[];
extern const char kChromeUINetworkHost[];
extern const char kChromeUIOobeHost[];
extern const char kChromeUIOSCreditsHost[];
extern const char kChromeUIPowerHost[];
extern const char kChromeUIInternetConfigDialogHost[];
extern const char kChromeUIInternetDetailDialogHost[];
extern const char kChromeUIRotateHost[];
extern const char kChromeUIScreenlockIconHost[];
extern const char kChromeUISetTimeHost[];
extern const char kChromeUISimUnlockHost[];
extern const char kChromeUISlowHost[];
extern const char kChromeUISlowTraceHost[];
extern const char kChromeUISysInternalsHost[];
extern const char kChromeUIUserImageHost[];
extern const char kChromeUIVoiceSearchHost[];
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
extern const char kChromeUIMetroFlowHost[];
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
extern const char kChromeUITabModalConfirmDialogHost[];
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
extern const char kChromeUIWebRtcLogsHost[];
#endif

extern const char kChromeUIMediaRouterHost[];
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
extern const char kChromeUICastHost[];
#endif

// Settings sub-pages.
extern const char kAutofillSubPage[];
extern const char kClearBrowserDataSubPage[];
extern const char kContentSettingsSubPage[];
extern const char kCreateProfileSubPage[];
extern const char kDeprecatedExtensionsSubPage[];
extern const char kHandlerSettingsSubPage[];
extern const char kImportDataSubPage[];
extern const char kLanguageOptionsSubPage[];
extern const char kManageProfileSubPage[];
extern const char kPasswordManagerSubPage[];
extern const char kResetProfileSettingsSubPage[];
extern const char kSearchEnginesSubPage[];
extern const char kSignOutSubPage[];
extern const char kSyncSetupSubPage[];
extern const char kTriggeredResetProfileSettingsSubPage[];
#if defined(OS_CHROMEOS)
extern const char kAccessibilitySubPage[];
extern const char kBluetoothSubPage[];
extern const char kDateTimeSubPage[];
extern const char kDisplaySubPage[];
extern const char kHelpSubPage[];
extern const char kInternetSubPage[];
extern const char kNetworkDetailSubPage[];
extern const char kPowerSubPage[];
extern const char kStylusSubPage[];
#endif

// Extensions sub pages.
extern const char kExtensionConfigureCommandsSubPage[];

// Gets the hosts/domains that are shown in chrome://chrome-urls.
extern const char* const kChromeHostURLs[];
extern const size_t kNumberOfChromeHostURLs;

// "Debug" pages which are dangerous and not for general consumption.
extern const char* const kChromeDebugURLs[];
extern const size_t kNumberOfChromeDebugURLs;

}  // namespace chrome

#endif  // CHROME_COMMON_WEBUI_URL_CONSTANTS_H_
