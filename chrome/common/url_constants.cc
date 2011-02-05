
// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "chrome/common/url_constants.h"
#include "googleurl/src/url_util.h"

namespace chrome {

const char kAboutScheme[] = "about";
const char kBlobScheme[] = "blob";
const char kChromeDevToolsScheme[] = "chrome-devtools";
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
  kChromeDevToolsScheme,
  kChromeUIScheme,
  NULL
};

const char kAboutAboutURL[] = "about:about";
const char kAboutAppCacheInternalsURL[] = "about:appcache-internals";
const char kAboutBlankURL[] = "about:blank";
const char kAboutCacheURL[] = "about:cache";
const char kAboutConflicts[] = "about:conflicts";
const char kAboutCrashURL[] = "about:crash";
const char kAboutKillURL[] = "about:kill";
const char kAboutCreditsURL[] = "about:credits";
const char kAboutDNSURL[] = "about:dns";
const char kAboutFlagsURL[] = "about:flags";
const char kAboutGpuURL[] = "about:gpu";
const char kAboutGpuCrashURL[] = "about:gpucrash";
const char kAboutGpuHangURL[] = "about:gpuhang";
const char kAboutHangURL[] = "about:hang";
const char kAboutHistogramsURL[] = "about:histograms";
const char kAboutMemoryURL[] = "about:memory";
const char kAboutNetInternalsURL[] = "about:net-internals";
const char kAboutPluginsURL[] = "about:plugins";
const char kAboutShorthangURL[] = "about:shorthang";
const char kAboutSyncURL[] = "about:sync";
const char kAboutSyncInternalsURL[] = "about:sync-internals";
const char kAboutTermsURL[] = "about:terms";
const char kAboutVaporwareURL[] = "about:vaporware";
const char kAboutVersionURL[] = "about:version";

// Use an obfuscated URL to make this nondiscoverable, we only want this
// to be used for testing.
const char kAboutBrowserCrash[] = "about:inducebrowsercrashforrealz";

const char kChromeUIAboutAboutURL[] = "chrome://about/about";
const char kChromeUIAboutCreditsURL[] = "chrome://about/credits";
const char kChromeUIAboutURL[] = "chrome://settings/about";
const char kChromeUIAppLauncherURL[] = "chrome://newtab/#mode=app-launcher";
const char kChromeUIBookmarksURL[] = "chrome://bookmarks/";
const char kChromeUIBugReportURL[] = "chrome://bugreport/";
const char kChromeUIConflictsURL[] = "chrome://conflicts/";
const char kChromeUIConstrainedHTMLTestURL[] = "chrome://constrained-test/";
const char kChromeUIDevToolsURL[] = "chrome-devtools://devtools/";
const char kChromeUIDownloadsURL[] = "chrome://downloads/";
const char kChromeUIExtensionsURL[] = "chrome://extensions/";
const char kChromeUIFavIconURL[] = "chrome://favicon/";
const char kChromeUIFlagsURL[] = "chrome://flags/";
const char kChromeUIHistory2URL[] = "chrome://history2/";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kChromeUIIPCURL[] = "chrome://about/ipc";
const char kChromeUIKeyboardURL[] = "chrome://keyboard/";
const char kChromeUINewTabURL[] = "chrome://newtab";
const char kChromeUIPluginsURL[] = "chrome://plugins/";
const char kChromeUIPrintURL[] = "chrome://print/";
const char kChromeUISettingsURL[] = "chrome://settings/";
const char kChromeUITextfieldsURL[] = "chrome://textfields/";

#if defined(OS_CHROMEOS)
const char kChromeUIAboutOSCreditsURL[] = "chrome://about/os-credits";
const char kChromeUIActivationMessage[] = "chrome://activationmessage/";
const char kChromeUIFileBrowseURL[] = "chrome://filebrowse/";
const char kChromeUIImageBurnerURL[] = "chrome://imageburner/";
const char kChromeUIKeyboardOverlayURL[] = "chrome://keyboardoverlay/";
const char kChromeUIMediaplayerURL[] = "chrome://mediaplayer/";
const char kChromeUIMobileSetupURL[] = "chrome://mobilesetup/";
const char kChromeUIRegisterPageURL[] = "chrome://register/";
const char kChromeUISlideshowURL[] = "chrome://slideshow/";
const char kChromeUISystemInfoURL[] = "chrome://system/";
#endif

// Keep this list sorted please.
const char kChromeUIBookmarksHost[] = "bookmarks";
const char kChromeUIBugReportHost[] = "bugreport";
const char kChromeUIConflictsHost[] = "conflicts";
const char kChromeUIDevToolsHost[] = "devtools";
const char kChromeUIDialogHost[] = "dialog";
const char kChromeUIDownloadsHost[] = "downloads";
const char kChromeUIExtensionsHost[] = "extensions";
const char kChromeUIFavIconHost[] = "favicon";
const char kChromeUIFlagsHost[] = "flags";
const char kChromeUIGpuInternalsHost[] = "gpu-internals";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUIHistory2Host[] = "history2";
const char kChromeUIInspectorHost[] = "inspector";
const char kChromeUIKeyboardHost[] = "keyboard";
const char kChromeUILoginHost[] = "login";
const char kChromeUINetInternalsHost[] = "net-internals";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUIPluginsHost[] = "plugins";
const char kChromeUIPrintHost[] = "print";
const char kChromeUIRemotingHost[] = "remoting";
const char kChromeUIRemotingResourcesHost[] = "remotingresources";
const char kChromeUIResourcesHost[] = "resources";
const char kChromeUIScreenshotPath[] = "screenshots";
const char kChromeUISettingsHost[] = "settings";
const char kChromeUISyncInternalsHost[] = "sync-internals";
const char kChromeUISyncResourcesHost[] = "syncresources";
const char kChromeUITextfieldsHost[] = "textfields";
const char kChromeUIThemePath[] = "theme";
const char kChromeUIThumbnailPath[] = "thumb";

#if defined(OS_CHROMEOS)
const char kChromeUIActivationMessageHost[] = "activationmessage";
const char kChromeUIFileBrowseHost[] = "filebrowse";
const char kChromeUIImageBurnerHost[] = "imageburner";
const char kChromeUIKeyboardOverlayHost[] = "keyboardoverlay";
const char kChromeUIMediaplayerHost[] = "mediaplayer";
const char kChromeUIMobileSetupHost[] = "mobilesetup";
const char kChromeUIRegisterPageHost[] = "register";
const char kChromeUISlideshowHost[] = "slideshow";
const char kChromeUISystemInfoHost[] = "system";
const char kChromeUIMenu[] = "menu";
const char kChromeUIWrenchMenu[] = "wrench-menu";
const char kChromeUINetworkMenu[] = "network-menu";
#endif

const char kUnreachableWebDataURL[] = "chrome://chromewebdata/";

const char kAppCacheViewInternalsURL[] = "chrome://appcache-internals/";

const char kBlobViewInternalsURL[] = "chrome://blob-internals/";

const char kCloudPrintResourcesURL[] = "chrome://cloudprintresources/";
const char kCloudPrintResourcesHost[] = "cloudprintresources";
const char kCloudPrintSetupHost[] = "cloudprintsetup";

const char kNetworkViewInternalsURL[] = "chrome://net-internals/";
const char kNetworkViewCacheURL[] = "chrome://view-http-cache/";

const char kSyncViewInternalsURL[] = "chrome://sync-internals/";

// GPU sub pages
const char kGpuInternalsURL[] = "chrome://gpu-internals/";

// Option sub pages.
const char kAdvancedOptionsSubPage[] =  "advanced";
const char kAutoFillSubPage[] = "autoFillOptions";
const char kBrowserOptionsSubPage[] =  "browser";
const char kClearBrowserDataSubPage[] = "clearBrowserDataOverlay";
const char kContentSettingsSubPage[] = "content";
const char kContentSettingsExceptionsSubPage[] = "contentExceptions";
const char kDefaultOptionsSubPage[] =  "";
const char kImportDataSubPage[] = "importDataOverlay";
const char kPersonalOptionsSubPage[] =  "personal";
const char kSearchEnginesOptionsSubPage[] = "editSearchEngineOverlay";
const char kSearchEnginesSubPage[] = "searchEngines";
#if defined(OS_CHROMEOS)
const char kInternetOptionsSubPage[] = "internet";
const char kLanguageOptionsSubPage[] = "language";
const char kSystemOptionsSubPage[] = "system";
#endif

const char kChromeHelpURL[] =
#if defined(OS_CHROMEOS)
  "http://www.google.com/support/chromeos/";
#else
  "http://www.google.com/support/chrome/";
#endif

  const char kPageInfoHelpCenterURL[] =
#if defined(OS_CHROMEOS)
    "http://www.google.com/support/chromeos/bin/answer.py?answer=95617";
#else
    "http://www.google.com/support/chrome/bin/answer.py?answer=95617";
#endif

const char kCrashReasonURL[] =
#if defined(OS_CHROMEOS)
    "http://www.google.com/support/chromeos/bin/answer.py?answer=1047340";
#else
    "http://www.google.com/support/chrome/bin/answer.py?answer=95669";
#endif

// TODO: These are currently placeholders that point to the crash
// docs.  See bug http://crosbug.com/10711
const char kKillReasonURL[] =
#if defined(OS_CHROMEOS)
    "http://www.google.com/support/chromeos/bin/answer.py?answer=1047340";
#else
    "http://www.google.com/support/chrome/bin/answer.py?answer=95669";
#endif

const char kPrivacyLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "http://www.google.com/support/chromeos/bin/answer.py?answer=1047334";
#else
    "http://www.google.com/support/chrome/bin/answer.py?answer=114836";
#endif

const char kChromiumProjectURL[] = "http://code.google.com/chromium/";

const char kLearnMoreReportingURL[] =
    "http://www.google.com/support/chrome/bin/answer.py?answer=96817&amp;";

void RegisterChromeSchemes() {
  // Don't need "chrome-internal" which was used in old versions of Chrome for
  // the new tab page.
  url_util::AddStandardScheme(kChromeDevToolsScheme);
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
