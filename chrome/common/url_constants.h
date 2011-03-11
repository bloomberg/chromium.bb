// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for known URLs and portions thereof.

#ifndef CHROME_COMMON_URL_CONSTANTS_H_
#define CHROME_COMMON_URL_CONSTANTS_H_
#pragma once

namespace chrome {

// Canonical schemes you can use as input to GURL.SchemeIs().
extern const char kAboutScheme[];
extern const char kBlobScheme[];
extern const char kChromeDevToolsScheme[];
extern const char kChromeInternalScheme[];
extern const char kChromeUIScheme[];  // The scheme used for WebUIs.
extern const char kCrosScheme[];      // The scheme used for ChromeOS.
extern const char kDataScheme[];
extern const char kExtensionScheme[];
extern const char kFileScheme[];
extern const char kFileSystemScheme[];
extern const char kFtpScheme[];
extern const char kGearsScheme[];
extern const char kHttpScheme[];
extern const char kHttpsScheme[];
extern const char kJavaScriptScheme[];
extern const char kMailToScheme[];
extern const char kMetadataScheme[];
extern const char kUserScriptScheme[];
extern const char kViewSourceScheme[];

// Used to separate a standard scheme and the hostname: "://".
extern const char kStandardSchemeSeparator[];

// Null terminated list of schemes that are savable.
extern const char* kSavableSchemes[];

// About URLs (including schemes).
extern const char kAboutAboutURL[];
extern const char kAboutAppCacheInternalsURL[];
extern const char kAboutBlankURL[];
extern const char kAboutBrowserCrash[];
extern const char kAboutConflicts[];
extern const char kAboutCacheURL[];
extern const char kAboutCrashURL[];
extern const char kAboutKillURL[];
extern const char kAboutCreditsURL[];
extern const char kAboutDNSURL[];
extern const char kAboutFlagsURL[];
extern const char kAboutGpuURL[];
extern const char kAboutGpuCrashURL[];
extern const char kAboutGpuHangURL[];
extern const char kAboutHangURL[];
extern const char kAboutHistogramsURL[];
extern const char kAboutMemoryURL[];
extern const char kAboutNetInternalsURL[];
extern const char kAboutPluginsURL[];
extern const char kAboutShorthangURL[];
extern const char kAboutSyncURL[];
extern const char kAboutSyncInternalsURL[];
extern const char kAboutTermsURL[];
extern const char kAboutVersionURL[];

// chrome: URLs (including schemes). Should be kept in sync with the
// components below.
extern const char kChromeUIAboutAboutURL[];
extern const char kChromeUIAboutCreditsURL[];
extern const char kChromeUIAboutURL[];
extern const char kChromeUIAppLauncherURL[];
extern const char kChromeUIBookmarksURL[];
extern const char kChromeUIBugReportURL[];
extern const char kChromeUIConflictsURL[];
extern const char kChromeUICollectedCookiesURL[];
extern const char kChromeUIConstrainedHTMLTestURL[];
extern const char kChromeUICrashesURL[];
extern const char kChromeUIDevToolsURL[];
extern const char kChromeUIDownloadsURL[];
extern const char kChromeUIExtensionIconURL[];
extern const char kChromeUIExtensionsURL[];
extern const char kChromeUIFlagsURL[];
extern const char kChromeUIFavIconURL[];
extern const char kChromeUIHistory2URL[];
extern const char kChromeUIHistoryURL[];
extern const char kChromeUIIPCURL[];
extern const char kChromeUIKeyboardURL[];
extern const char kChromeUINewTabURL[];
extern const char kChromeUIPluginsURL[];
extern const char kChromeUIPrintURL[];
extern const char kChromeUISettingsURL[];
extern const char kChromeUITextfieldsURL[];

#if defined(OS_CHROMEOS)
extern const char kChromeUIAboutOSCreditsURL[];
extern const char kChromeUIActivationMessage[];
extern const char kChromeUIFileBrowseURL[];
extern const char kChromeUIImageBurnerURL[];
extern const char kChromeUIKeyboardOverlayURL[];
extern const char kChromeUIMediaplayerURL[];
extern const char kChromeUIMobileSetupURL[];
extern const char kChromeUIRegisterPageURL[];
extern const char kChromeUISlideshowURL[];
extern const char kChromeUISystemInfoURL[];
extern const char kChromeUIUserImageURL[];
#endif

// chrome components of URLs. Should be kept in sync with the full URLs
// above.
extern const char kChromeUIBookmarksHost[];
extern const char kChromeUIBugReportHost[];
extern const char kChromeUIConflictsHost[];
extern const char kChromeUICrashesHost[];
extern const char kChromeUICollectedCookiesHost[];
extern const char kChromeUIDevToolsHost[];
extern const char kChromeUIDialogHost[];
extern const char kChromeUIDownloadsHost[];
extern const char kChromeUIExtensionIconHost[];
extern const char kChromeUIExtensionsHost[];
extern const char kChromeUIFavIconHost[];
extern const char kChromeUIFlagsHost[];
extern const char kChromeUIGpuInternalsHost[];
extern const char kChromeUIHistory2Host[];
extern const char kChromeUIHistoryHost[];
extern const char kChromeUIKeyboardHost[];
extern const char kChromeUINetInternalsHost[];
extern const char kChromeUINewTabHost[];
extern const char kChromeUIPluginsHost[];
extern const char kChromeUIPrintHost[];
extern const char kChromeUIRemotingHost[];
extern const char kChromeUIRemotingResourcesHost[];
extern const char kChromeUIResourcesHost[];
extern const char kChromeUIScreenshotPath[];
extern const char kChromeUISettingsHost[];
extern const char kChromeUISyncInternalsHost[];
extern const char kChromeUISyncResourcesHost[];
extern const char kChromeUITextfieldsHost[];
extern const char kChromeUIThemePath[];
extern const char kChromeUIThumbnailPath[];

#if defined(OS_CHROMEOS)
extern const char kChromeUIActivationMessageHost[];
extern const char kChromeUIFileBrowseHost[];
extern const char kChromeUIImageBurnerHost[];
extern const char kChromeUIKeyboardOverlayHost[];
extern const char kChromeUIMediaplayerHost[];
extern const char kChromeUIMobileSetupHost[];
extern const char kChromeUIRegisterPageHost[];
extern const char kChromeUISlideshowHost[];
extern const char kChromeUISystemInfoHost[];
extern const char kChromeUIMenu[];
extern const char kChromeUIWrenchMenu[];
extern const char kChromeUINetworkMenu[];
extern const char kChromeUIUserImageHost[];
#endif

#if defined(OS_CHROMEOS) && defined(TOUCH_UI)
extern const char kChromeUILoginContainerHost[];
extern const char kChromeUILoginHost[];
#endif

// Special URL used to start a navigation to an error page.
extern const char kUnreachableWebDataURL[];

// AppCache related URL.
extern const char kAppCacheViewInternalsURL[];

// Blob related URL.
extern const char kBlobViewInternalsURL[];

// Cloud Print dialog URL components.
extern const char kCloudPrintResourcesURL[];
extern const char kCloudPrintResourcesHost[];
extern const char kCloudPrintSetupHost[];

// Network related URLs.
extern const char kNetworkViewCacheURL[];
extern const char kNetworkViewInternalsURL[];

// Sync related URLs.
extern const char kSyncViewInternalsURL[];

// GPU related URLs
extern const char kGpuInternalsURL[];

// Options sub-pages.
extern const char kAdvancedOptionsSubPage[];
extern const char kAutoFillSubPage[];
extern const char kBrowserOptionsSubPage[];
extern const char kClearBrowserDataSubPage[];
extern const char kContentSettingsSubPage[];
extern const char kContentSettingsExceptionsSubPage[];
extern const char kImportDataSubPage[];
extern const char kInstantConfirmPage[];
extern const char kPersonalOptionsSubPage[];
extern const char kPasswordManagerSubPage[];
extern const char kSearchEnginesSubPage[];
#if defined(OS_CHROMEOS)
extern const char kInternetOptionsSubPage[];
extern const char kLanguageOptionsSubPage[];
extern const char kSystemOptionsSubPage[];
#endif

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

// The URL for the "Learn more" page for the blocked/outdated plugin infobar.
extern const char kOutdatedPluginLearnMoreURL[];

// Call near the beginning of startup to register Chrome's internal URLs that
// should be parsed as "standard" with the googleurl library.
void RegisterChromeSchemes();

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
