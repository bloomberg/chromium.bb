// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for known URLs and portions thereof.

#ifndef CHROME_COMMON_URL_CONSTANTS_H_
#define CHROME_COMMON_URL_CONSTANTS_H_
#pragma once

#include "build/build_config.h"
#include "content/public/common/url_constants.h"

namespace chrome {

// TODO(msw): Resolve chrome_frame dependency on these constants.
extern const char kAboutPluginsURL[];
extern const char kAboutVersionURL[];

// chrome: URLs (including schemes). Should be kept in sync with the
// components below.
extern const char kChromeUIAboutURL[];
extern const char kChromeUIBookmarksURL[];
extern const char kChromeUIBugReportURL[];
extern const char kChromeUICertificateViewerURL[];
extern const char kChromeUIChromeURLsURL[];
extern const char kChromeUICloudPrintResourcesURL[];
extern const char kChromeUIConflictsURL[];
extern const char kChromeUIConstrainedHTMLTestURL[];
extern const char kChromeUICrashesURL[];
extern const char kChromeUICrashURL[];
extern const char kChromeUICreditsURL[];
extern const char kChromeUIDevToolsURL[];
extern const char kChromeUIDownloadsURL[];
extern const char kChromeUIEditSearchEngineDialogURL[];
extern const char kChromeUIExtensionIconURL[];
extern const char kChromeUIExtensionsURL[];
extern const char kChromeUIFaviconURL[];
extern const char kChromeUIFlagsURL[];
extern const char kChromeUIFlashURL[];
extern const char kChromeUIGpuCleanURL[];
extern const char kChromeUIGpuCrashURL[];
extern const char kChromeUIGpuHangURL[];
extern const char kChromeUIHangURL[];
extern const char kChromeUIHistoryURL[];
extern const char kChromeUIHungRendererDialogURL[];
extern const char kChromeUIInputWindowDialogURL[];
extern const char kChromeUIIPCURL[];
extern const char kChromeUIKeyboardURL[];
extern const char kChromeUIKillURL[];
extern const char kChromeUIMemoryRedirectURL[];
extern const char kChromeUIMemoryURL[];
extern const char kChromeUINetInternalsURL[];
extern const char kChromeUINetworkViewCacheURL[];
extern const char kChromeUINewProfile[];
extern const char kChromeUINewTabURL[];
extern const char kChromeUIPluginsURL[];
extern const char kChromeUIPolicyURL[];
extern const char kChromeUIPrintURL[];
extern const char kChromeUISessionsURL[];
extern const char kChromeUISettingsURL[];
extern const char kChromeUIShorthangURL[];
extern const char kChromeUISSLClientCertificateSelectorURL[];
extern const char kChromeUISyncPromoURL[];
extern const char kChromeUITaskManagerURL[];
extern const char kChromeUITermsURL[];
extern const char kChromeUIThumbnailURL[];
extern const char kChromeUIVersionURL[];
extern const char kChromeUIWorkersURL[];

#if defined(OS_CHROMEOS)
extern const char kChromeUIActivationMessage[];
extern const char kChromeUIActiveDownloadsURL[];
extern const char kChromeUIChooseMobileNetworkURL[];
extern const char kChromeUIDiscardsURL[];
extern const char kChromeUIImageBurnerURL[];
extern const char kChromeUIKeyboardOverlayURL[];
extern const char kChromeUILockScreenURL[];
extern const char kChromeUIMediaplayerURL[];
extern const char kChromeUIMobileSetupURL[];
extern const char kChromeUIOobeURL[];
extern const char kChromeUIOSCreditsURL[];
extern const char kChromeUIProxySettingsURL[];
extern const char kChromeUIRegisterPageURL[];
extern const char kChromeUISimUnlockURL[];
extern const char kChromeUISlideshowURL[];
extern const char kChromeUISystemInfoURL[];
extern const char kChromeUITermsOemURL[];
extern const char kChromeUIUserImageURL[];
#endif

#if defined(FILE_MANAGER_EXTENSION)
extern const char kChromeUIFileManagerURL[];
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
extern const char kChromeUICollectedCookiesURL[];
extern const char kChromeUIHttpAuthURL[];
extern const char kChromeUIRepostFormWarningURL[];
#endif

#if defined(USE_AURA)
extern const char kChromeUIAppListURL[];
#endif

// chrome components of URLs. Should be kept in sync with the full URLs above.
extern const char kChromeUIAboutHost[];
extern const char kChromeUIAppCacheInternalsHost[];
extern const char kChromeUIBlankHost[];
extern const char kChromeUIBlobInternalsHost[];
extern const char kChromeUIBookmarksHost[];
extern const char kChromeUIBrowserCrashHost[];
extern const char kChromeUIBugReportHost[];
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
extern const char kChromeUIDevToolsHost[];
extern const char kChromeUIDialogHost[];
extern const char kChromeUIDNSHost[];
extern const char kChromeUIDownloadsHost[];
extern const char kChromeUIEditSearchEngineDialogHost[];
extern const char kChromeUIExtensionIconHost[];
extern const char kChromeUIExtensionsHost[];
extern const char kChromeUIFaviconHost[];
extern const char kChromeUIFlagsHost[];
extern const char kChromeUIFlashHost[];
extern const char kChromeUIGpuCleanHost[];
extern const char kChromeUIGpuCrashHost[];
extern const char kChromeUIGpuHangHost[];
extern const char kChromeUIGpuHost[];
extern const char kChromeUIGpuInternalsHost[];
extern const char kChromeUIHangHost[];
extern const char kChromeUIHistogramsHost[];
extern const char kChromeUIHistoryHost[];
extern const char kChromeUIHungRendererDialogHost[];
extern const char kChromeUIInputWindowDialogHost[];
extern const char kChromeUIIPCHost[];
extern const char kChromeUIKeyboardHost[];
extern const char kChromeUIKillHost[];
extern const char kChromeUIMediaInternalsHost[];
extern const char kChromeUIMemoryHost[];
extern const char kChromeUIMemoryRedirectHost[];
extern const char kChromeUINetInternalsHost[];
extern const char kChromeUINetworkViewCacheHost[];
extern const char kChromeUINewTabHost[];
extern const char kChromeUIPluginsHost[];
extern const char kChromeUIPolicyHost[];
extern const char kChromeUIPrintHost[];
extern const char kChromeUIProfilerHost[];
extern const char kChromeUIQuotaInternalsHost[];
extern const char kChromeUIResourcesHost[];
extern const char kChromeUISessionsHost[];
extern const char kChromeUISettingsHost[];
extern const char kChromeUIShorthangHost[];
extern const char kChromeUISSLClientCertificateSelectorHost[];
extern const char kChromeUIStatsHost[];
extern const char kChromeUISyncHost[];
extern const char kChromeUISyncInternalsHost[];
extern const char kChromeUISyncPromoHost[];
extern const char kChromeUISyncResourcesHost[];
extern const char kChromeUITaskManagerHost[];
extern const char kChromeUITCMallocHost[];
extern const char kChromeUITermsHost[];
extern const char kChromeUIThumbnailHost[];
extern const char kChromeUITouchIconHost[];
extern const char kChromeUITracingHost[];
extern const char kChromeUIVersionHost[];
extern const char kChromeUIWorkersHost[];

extern const char kChromeUIScreenshotPath[];
extern const char kChromeUIThemePath[];

#if defined(OS_LINUX) || defined(OS_OPENBSD)
extern const char kChromeUILinuxProxyConfigHost[];
extern const char kChromeUISandboxHost[];
#endif

#if defined(OS_CHROMEOS)
extern const char kChromeUIActivationMessageHost[];
extern const char kChromeUIActiveDownloadsHost[];
extern const char kChromeUIChooseMobileNetworkHost[];
extern const char kChromeUICryptohomeHost[];
extern const char kChromeUIDiscardsHost[];
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
extern const char kChromeUIRegisterPageHost[];
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

#if defined(FILE_MANAGER_EXTENSION)
extern const char kChromeUIFileManagerHost[];
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
extern const char kChromeUICollectedCookiesHost[];
extern const char kChromeUIHttpAuthHost[];
extern const char kChromeUIRepostFormWarningHost[];
#endif

#if defined(USE_AURA)
extern const char kChromeUIAppListHost[];
#endif

// Options sub-pages.
extern const char kAdvancedOptionsSubPage[];
extern const char kAutofillSubPage[];
extern const char kBrowserOptionsSubPage[];
extern const char kClearBrowserDataSubPage[];
extern const char kContentSettingsExceptionsSubPage[];
extern const char kContentSettingsSubPage[];
extern const char kExtensionsSubPage[];
extern const char kHandlerSettingsSubPage[];
extern const char kImportDataSubPage[];
extern const char kInstantConfirmPage[];
extern const char kLanguageOptionsSubPage[];
extern const char kManageProfileSubPage[];
extern const char kPasswordManagerSubPage[];
extern const char kPersonalOptionsSubPage[];
extern const char kSearchEnginesSubPage[];
extern const char kSyncSetupSubPage[];
#if defined(OS_CHROMEOS)
extern const char kAboutOptionsSubPage[];
extern const char kInternetOptionsSubPage[];
extern const char kSystemOptionsSubPage[];
#endif

extern const char kSyncGoogleDashboardURL[];

extern const char kPasswordManagerLearnMoreURL[];

// General help link for Chrome.
extern const char kChromeHelpURL[];

// "What do these mean?" URL for the Page Info bubble.
extern const char kPageInfoHelpCenterURL[];

// "Learn more" URL for "Aw snap" page.
extern const char kCrashReasonURL[];

// "Learn more" URL for killed tab page.
extern const char kKillReasonURL[];

// "Learn more" URL for the Privacy section under Options.
extern const char kPrivacyLearnMoreURL[];

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

// "Debug" pages which are dangerous and not for general consumption.
extern const char* const kChromeDebugURLs[];
extern int kNumberOfChromeDebugURLs;

// Canonical schemes you can use as input to GURL.SchemeIs().
extern const char kExtensionScheme[];

// Call near the beginning of startup to register Chrome's internal URLs that
// should be parsed as "standard" with the googleurl library.
void RegisterChromeSchemes();

#if defined(OS_CHROMEOS)
// "Learn more" URL for the Cloud Print section under Options.
extern const char kCloudPrintLearnMoreURL[];
#endif

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
