// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
extern const char kChromeInternalScheme[];
extern const char kChromeUIScheme[];  // The scheme used for DOMUIs.
extern const char kCrosScheme[];      // The scheme used for ChromeOS.
extern const char kDataScheme[];
extern const char kExtensionScheme[];
extern const char kFileScheme[];
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
extern const char kAboutCacheURL[];
extern const char kAboutCrashURL[];
extern const char kAboutCreditsURL[];
extern const char kAboutDNSURL[];
extern const char kAboutGpuCrashURL[];
extern const char kAboutHangURL[];
extern const char kAboutHistogramsURL[];
extern const char kAboutLabsURL[];
extern const char kAboutMemoryURL[];
extern const char kAboutNetInternalsURL[];
extern const char kAboutPluginsURL[];
extern const char kAboutShorthangURL[];
extern const char kAboutSystemURL[];
extern const char kAboutTermsURL[];
extern const char kAboutVaporwareURL[];
extern const char kAboutVersionURL[];

// chrome: URLs (including schemes). Should be kept in sync with the
// components below.
extern const char kChromeUIAboutURL[];
extern const char kChromeUIAppLauncherURL[];
extern const char kChromeUIBookmarksURL[];
extern const char kChromeUIBugReportURL[];
extern const char kChromeUIDevToolsURL[];
extern const char kChromeUIDownloadsURL[];
extern const char kChromeUIExtensionsURL[];
extern const char kChromeUIFavIconURL[];
extern const char kChromeUIFileBrowseURL[];
extern const char kChromeUIHistory2URL[];
extern const char kChromeUIHistoryURL[];
extern const char kChromeUIImageBurnerURL[];
extern const char kChromeUIIPCURL[];
extern const char kChromeUILabsURL[];
extern const char kChromeUIMediaplayerURL[];
extern const char kChromeUINewTabURL[];
extern const char kChromeUIOptionsURL[];
extern const char kChromeUIPluginsURL[];
extern const char kChromeUIPrintURL[];
extern const char kChromeUIRegisterPageURL[];
extern const char kChromeUISlideshowURL[];

// chrome components of URLs. Should be kept in sync with the full URLs
// above.
extern const char kChromeUIBookmarksHost[];
extern const char kChromeUIBugReportHost[];
extern const char kChromeUIDevToolsHost[];
extern const char kChromeUIDialogHost[];
extern const char kChromeUIDownloadsHost[];
extern const char kChromeUIExtensionsHost[];
extern const char kChromeUIFavIconHost[];
extern const char kChromeUIFileBrowseHost[];
extern const char kChromeUIHistory2Host[];
extern const char kChromeUIHistoryHost[];
extern const char kChromeUIImageBurnerHost[];
extern const char kChromeUIInspectorHost[];
extern const char kChromeUILabsHost[];
extern const char kChromeUIMediaplayerHost[];
extern const char kChromeUINetInternalsHost[];
extern const char kChromeUINewTabHost[];
extern const char kChromeUIOptionsHost[];
extern const char kChromeUIPluginsHost[];
extern const char kChromeUIPrintHost[];
extern const char kChromeUIRegisterPageHost[];
extern const char kChromeUIRemotingHost[];
extern const char kChromeUIRemotingResourcesHost[];
extern const char kChromeUIResourcesHost[];
extern const char kChromeUIScreenshotPath[];
extern const char kChromeUISlideshowHost[];
extern const char kChromeUISyncResourcesHost[];
extern const char kChromeUIThemePath[];
extern const char kChromeUIThumbnailPath[];

// AppCache related URL.
extern const char kAppCacheViewInternalsURL[];

// Cloud Print dialog URL components.
extern const char kCloudPrintResourcesURL[];
extern const char kCloudPrintResourcesHost[];

// Network related URLs.
extern const char kNetworkViewCacheURL[];
extern const char kNetworkViewInternalsURL[];

// Options sub-pages.
extern const char kDefaultOptionsSubPage[];
extern const char kBrowserOptionsSubPage[];
extern const char kPersonalOptionsSubPage[];
extern const char kAdvancedOptionsSubPage[];
extern const char kSearchEnginesOptionsSubPage[];
extern const char kClearBrowserDataSubPage[];
extern const char kImportDataSubPage[];
extern const char kContentSettingsSubPage[];
#if defined(OS_CHROMEOS)
extern const char kSystemOptionsSubPage[];
extern const char kLanguageOptionsSubPage[];
extern const char kInternetOptionsSubPage[];
#endif

// Call near the beginning of startup to register Chrome's internal URLs that
// should be parsed as "standard" with the googleurl library.
void RegisterChromeSchemes();

}  // namespace chrome

#endif  // CHROME_COMMON_URL_CONSTANTS_H_
