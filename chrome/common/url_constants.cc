// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "chrome/common/url_constants.h"
#include "googleurl/src/url_util.h"

namespace chrome {

const char kAboutScheme[] = "about";
const char kBlobScheme[] = "blob";
const char kChromeInternalScheme[] = "chrome-internal";
const char kChromeUIScheme[] = "chrome";
const char kDataScheme[] = "data";
const char kExtensionScheme[] = "chrome-extension";
const char kFileScheme[] = "file";
const char kFtpScheme[] = "ftp";
const char kGearsScheme[] = "gears";
const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
const char kJavaScriptScheme[] = "javascript";
const char kMailToScheme[] = "mailto";
const char kMetadataScheme[] = "metadata";
const char kUserScriptScheme[] = "chrome-user-script";
const char kViewSourceScheme[] = "view-source";

#if defined(OS_CHROMEOS)
const char kCrosScheme[] = "cros";
#endif

const char kStandardSchemeSeparator[] = "://";

const char* kSavableSchemes[] = {
  kHttpScheme,
  kHttpsScheme,
  kFileScheme,
  kFtpScheme,
  kExtensionScheme,
  kChromeUIScheme,
  NULL
};

const char kAboutAboutURL[] = "about:about";
const char kAboutAppCacheInternalsURL[] = "about:appcache-internals";
const char kAboutBlankURL[] = "about:blank";
const char kAboutCacheURL[] = "about:cache";
const char kAboutCrashURL[] = "about:crash";
const char kAboutCreditsURL[] = "about:credits";
const char kAboutDNSURL[] = "about:dns";
const char kAboutHangURL[] = "about:hang";
const char kAboutHistogramsURL[] = "about:histograms";
const char kAboutLabsURL[] = "about:labs";
const char kAboutMemoryURL[] = "about:memory";
const char kAboutNetInternalsURL[] = "about:net-internals";
const char kAboutPluginsURL[] = "about:plugins";
const char kAboutShorthangURL[] = "about:shorthang";
const char kAboutSystemURL[] = "about:system";
const char kAboutTermsURL[] = "about:terms";
const char kAboutVaporwareURL[] = "about:vaporware";
const char kAboutVersionURL[] = "about:version";

// Use an obfuscated URL to make this nondiscoverable, we only want this
// to be used for testing.
const char kAboutBrowserCrash[] = "about:inducebrowsercrashforrealz";

const char kChromeUIAboutURL[] = "chrome://options/about";
const char kChromeUIAppLauncherURL[] = "chrome://newtab/#mode=app-launcher";
const char kChromeUIBookmarksURL[] = "chrome://bookmarks/";
const char kChromeUIBugReportURL[] = "chrome://bugreport/";
const char kChromeUIDevToolsURL[] = "chrome://devtools/";
const char kChromeUIDownloadsURL[] = "chrome://downloads/";
const char kChromeUIExtensionsURL[] = "chrome://extensions/";
const char kChromeUIFavIconURL[] = "chrome://favicon/";
const char kChromeUIFileBrowseURL[] = "chrome://filebrowse/";
const char kChromeUIHistory2URL[] = "chrome://history2/";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kChromeUIImageBurnerURL[] = "chrome://imageburner/";
const char kChromeUIIPCURL[] = "chrome://about/ipc";
const char kChromeUILabsURL[] = "chrome://labs/";
const char kChromeUIMediaplayerURL[] = "chrome://mediaplayer/";
const char kChromeUINewTabURL[] = "chrome://newtab";
const char kChromeUIOptionsURL[] = "chrome://options/";
const char kChromeUIPluginsURL[] = "chrome://plugins/";
const char kChromeUIPrintURL[] = "chrome://print/";
const char kChromeUIRegisterPageURL[] = "chrome://register/";
const char kChromeUISlideshowURL[] = "chrome://slideshow/";

const char kChromeUIBookmarksHost[] = "bookmarks";
const char kChromeUIBugReportHost[] = "bugreport";
const char kChromeUIDevToolsHost[] = "devtools";
const char kChromeUIDialogHost[] = "dialog";
const char kChromeUIDownloadsHost[] = "downloads";
const char kChromeUIExtensionsHost[] = "extensions";
const char kChromeUIFavIconHost[] = "favicon";
const char kChromeUIFileBrowseHost[] = "filebrowse";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUIHistory2Host[] = "history2";
const char kChromeUIImageBurnerHost[] = "imageburner";
const char kChromeUIInspectorHost[] = "inspector";
const char kChromeUILabsHost[] = "labs";
const char kChromeUIMediaplayerHost[] = "mediaplayer";
const char kChromeUINetInternalsHost[] = "net-internals";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUIOptionsHost[] = "options";
const char kChromeUIPluginsHost[] = "plugins";
const char kChromeUIPrintHost[] = "print";
const char kChromeUIRegisterPageHost[] = "register";
const char kChromeUIRemotingHost[] = "remoting";
const char kChromeUIResourcesHost[] = "resources";
const char kChromeUISlideshowHost[] = "slideshow";
const char kChromeUISyncResourcesHost[] = "syncresources";
const char kChromeUIRemotingResourcesHost[] = "remotingresources";
const char kChromeUIThemePath[] = "theme";
const char kChromeUIScreenshotPath[] = "screenshots";
const char kChromeUIThumbnailPath[] = "thumb";

const char kAppCacheViewInternalsURL[] = "chrome://appcache-internals/";

const char kCloudPrintResourcesURL[] = "chrome://cloudprintresources/";
const char kCloudPrintResourcesHost[] = "cloudprintresources";

const char kNetworkViewInternalsURL[] = "chrome://net-internals/";
const char kNetworkViewCacheURL[] = "chrome://view-http-cache/";

// Option sub pages.
const char kDefaultOptionsSubPage[] =  "";
const char kBrowserOptionsSubPage[] =  "browser";
const char kPersonalOptionsSubPage[] =  "personal";
const char kAdvancedOptionsSubPage[] =  "advanced";
const char kSearchEnginesOptionsSubPage[] = "editSearchEngineOverlay";
const char kClearBrowserDataSubPage[] = "clearBrowserDataOverlay";
const char kImportDataSubPage[] = "importDataOverlay";
const char kContentSettingsSubPage[] = "content";
#if defined(OS_CHROMEOS)
const char kSystemOptionsSubPage[] = "system";
const char kLanguageOptionsSubPage[] = "language";
const char kInternetOptionsSubPage[] = "internet";
#endif

void RegisterChromeSchemes() {
  // Don't need "chrome-internal" which was used in old versions of Chrome for
  // the new tab page.
  url_util::AddStandardScheme(kChromeUIScheme);
  url_util::AddStandardScheme(kGearsScheme);
  url_util::AddStandardScheme(kExtensionScheme);
  url_util::AddStandardScheme(kMetadataScheme);
#if defined(OS_CHROMEOS)
  url_util::AddStandardScheme(kCrosScheme);
#endif

  // Prevent future modification of the standard schemes list. This is to
  // prevent accidental creation of data races in the program. AddStandardScheme
  // isn't threadsafe so must be called when GURL isn't used on any other
  // thread. This is really easy to mess up, so we say that all calls to
  // AddStandardScheme in Chrome must be inside this function.
  url_util::LockStandardSchemes();
}

}  // namespace chrome
