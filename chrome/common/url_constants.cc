// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/url_constants.h"

#include "googleurl/src/url_util.h"

namespace {
const char* kSavableSchemes[] = {
  chrome::kExtensionScheme,
  NULL
};
}  // namespace

namespace chrome {

#if defined(OS_CHROMEOS)
const char kCrosScheme[] = "cros";
#endif

const char kAboutPluginsURL[] = "about:plugins";
const char kAboutVersionURL[] = "about:version";

// Add Chrome UI URLs as necessary, in alphabetical order.
// Be sure to add the corresponding kChromeUI*Host constant below.
const char kChromeUIAboutURL[] = "chrome://about/";
const char kChromeUIBookmarksURL[] = "chrome://bookmarks/";
const char kChromeUIBugReportURL[] = "chrome://bugreport/";
const char kChromeUICertificateViewerURL[] = "chrome://view-cert/";
const char kChromeUIChromeURLsURL[] = "chrome://chrome-urls/";
const char kChromeUICloudPrintResourcesURL[] = "chrome://cloudprintresources/";
const char kChromeUIConflictsURL[] = "chrome://conflicts/";
const char kChromeUIConstrainedHTMLTestURL[] = "chrome://constrained-test/";
const char kChromeUICrashesURL[] = "chrome://crashes/";
const char kChromeUICrashURL[] = "chrome://crash/";
const char kChromeUICreditsURL[] = "chrome://credits/";
const char kChromeUIDevToolsURL[] = "chrome-devtools://devtools/";
const char kChromeUIDownloadsURL[] = "chrome://downloads/";
const char kChromeUIEditSearchEngineDialogURL[] = "chrome://editsearchengine/";
const char kChromeUIExtensionIconURL[] = "chrome://extension-icon/";
const char kChromeUIExtensionsURL[] = "chrome://extensions/";
const char kChromeUIFaviconURL[] = "chrome://favicon/";
const char kChromeUIFlagsURL[] = "chrome://flags/";
const char kChromeUIFlashURL[] = "chrome://flash/";
const char kChromeUIGpuCleanURL[] = "chrome://gpuclean";
const char kChromeUIGpuCrashURL[] = "chrome://gpucrash";
const char kChromeUIGpuHangURL[] = "chrome://gpuhang";
const char kChromeUIHangURL[] = "chrome://hang/";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kChromeUIHungRendererDialogURL[] = "chrome://hung-renderer/";
const char kChromeUIInputWindowDialogURL[] = "chrome://input-window-dialog/";
const char kChromeUIIPCURL[] = "chrome://ipc/";
const char kChromeUIKeyboardURL[] = "chrome://keyboard/";
const char kChromeUIKillURL[] = "chrome://kill/";
const char kChromeUIMemoryRedirectURL[] = "chrome://memory-redirect/";
const char kChromeUIMemoryURL[] = "chrome://memory/";
const char kChromeUINetInternalsURL[] = "chrome://net-internals/";
const char kChromeUINetworkViewCacheURL[] = "chrome://view-http-cache/";
const char kChromeUINewProfile[] = "chrome://newprofile/";
const char kChromeUINewTabURL[] = "chrome://newtab/";
const char kChromeUIPluginsURL[] = "chrome://plugins/";
const char kChromeUIPolicyURL[] = "chrome://policy/";
const char kChromeUIPrintURL[] = "chrome://print/";
const char kChromeUISessionsURL[] = "chrome://sessions/";
const char kChromeUISettingsURL[] = "chrome://settings/";
const char kChromeUIShorthangURL[] = "chrome://shorthang/";
const char kChromeUISSLClientCertificateSelectorURL[] = "chrome://select-cert/";
const char kChromeUISyncPromoURL[] = "chrome://syncpromo/";
const char kChromeUITaskManagerURL[] = "chrome://tasks/";
const char kChromeUITermsURL[] = "chrome://terms/";
const char kChromeUIThumbnailURL[] = "chrome://thumb/";
const char kChromeUIVersionURL[] = "chrome://version/";
const char kChromeUIWorkersURL[] = "chrome://workers/";

#if defined(OS_CHROMEOS)
const char kChromeUIActivationMessage[] = "chrome://activationmessage/";
const char kChromeUIActiveDownloadsURL[] = "chrome://active-downloads/";
const char kChromeUIChooseMobileNetworkURL[] =
    "chrome://choose-mobile-network/";
const char kChromeUIDiscardsURL[] = "chrome://discards/";
const char kChromeUIImageBurnerURL[] = "chrome://imageburner/";
const char kChromeUIKeyboardOverlayURL[] = "chrome://keyboardoverlay/";
const char kChromeUILockScreenURL[] = "chrome://lock/";
const char kChromeUIMediaplayerURL[] = "chrome://mediaplayer/";
const char kChromeUIMobileSetupURL[] = "chrome://mobilesetup/";
const char kChromeUIOobeURL[] = "chrome://oobe/";
const char kChromeUIOSCreditsURL[] = "chrome://os-credits/";
const char kChromeUIProxySettingsURL[] = "chrome://proxy-settings/";
const char kChromeUIRegisterPageURL[] = "chrome://register/";
const char kChromeUISimUnlockURL[] = "chrome://sim-unlock/";
const char kChromeUISlideshowURL[] = "chrome://slideshow/";
const char kChromeUISystemInfoURL[] = "chrome://system/";
const char kChromeUITermsOemURL[] = "chrome://terms/oem";
const char kChromeUIUserImageURL[] = "chrome://userimage/";
#endif

#if defined(FILE_MANAGER_EXTENSION)
const char kChromeUIFileManagerURL[] = "chrome://files/";
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
const char kChromeUICollectedCookiesURL[] = "chrome://collected-cookies/";
const char kChromeUIHttpAuthURL[] = "chrome://http-auth/";
const char kChromeUIRepostFormWarningURL[] = "chrome://repost-form-warning/";
#endif

#if defined(USE_AURA)
const char kChromeUIAppListURL[] = "chrome://app-list/";
#endif

// Add Chrome UI hosts here, in alphabetical order.
// Add hosts to kChromePaths in browser_about_handler.cc to be listed by
// chrome://chrome-urls (about:about) and the built-in AutocompleteProvider.
const char kChromeUIAboutHost[] = "about";
const char kChromeUIAppCacheInternalsHost[] = "appcache-internals";
const char kChromeUIBlankHost[] = "blank";
const char kChromeUIBlobInternalsHost[] = "blob-internals";
const char kChromeUIBookmarksHost[] = "bookmarks";
const char kChromeUIBrowserCrashHost[] = "inducebrowsercrashforrealz";
const char kChromeUIBugReportHost[] = "bugreport";
const char kChromeUICacheHost[] = "cache";
const char kChromeUICertificateViewerHost[] = "view-cert";
const char kChromeUIChromeURLsHost[] = "chrome-urls";
const char kChromeUICloudPrintResourcesHost[] = "cloudprintresources";
const char kChromeUICloudPrintSetupHost[] = "cloudprintsetup";
const char kChromeUIConflictsHost[] = "conflicts";
const char kChromeUIConstrainedHTMLTestHost[] = "constrained-test";
const char kChromeUICrashesHost[] = "crashes";
const char kChromeUICrashHost[] = "crash";
const char kChromeUICreditsHost[] = "credits";
const char kChromeUIDefaultHost[] = "version";
const char kChromeUIDevToolsHost[] = "devtools";
const char kChromeUIDialogHost[] = "dialog";
const char kChromeUIDNSHost[] = "dns";
const char kChromeUIDownloadsHost[] = "downloads";
const char kChromeUIEditSearchEngineDialogHost[] = "editsearchengine";
const char kChromeUIExtensionIconHost[] = "extension-icon";
const char kChromeUIExtensionsHost[] = "extensions";
const char kChromeUIFaviconHost[] = "favicon";
const char kChromeUIFlagsHost[] = "flags";
const char kChromeUIFlashHost[] = "flash";
const char kChromeUIGpuCleanHost[] = "gpuclean";
const char kChromeUIGpuCrashHost[] = "gpucrash";
const char kChromeUIGpuHangHost[] = "gpuhang";
const char kChromeUIGpuHost[] = "gpu";
const char kChromeUIGpuInternalsHost[] = "gpu-internals";
const char kChromeUIHangHost[] = "hang";
const char kChromeUIHistogramsHost[] = "histograms";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUIHungRendererDialogHost[] = "hung-renderer";
const char kChromeUIInputWindowDialogHost[] = "input-window-dialog";
const char kChromeUIIPCHost[] = "ipc";
const char kChromeUIKeyboardHost[] = "keyboard";
const char kChromeUIKillHost[] = "kill";
const char kChromeUIMediaInternalsHost[] = "media-internals";
const char kChromeUIMemoryHost[] = "memory";
const char kChromeUIMemoryRedirectHost[] = "memory-redirect";
const char kChromeUINetInternalsHost[] = "net-internals";
const char kChromeUINetworkViewCacheHost[] = "view-http-cache";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUIPluginsHost[] = "plugins";
const char kChromeUIPolicyHost[] = "policy";
const char kChromeUIPrintHost[] = "print";
const char kChromeUIProfilerHost[] = "profiler";
const char kChromeUIQuotaInternalsHost[] = "quota-internals";
const char kChromeUIResourcesHost[] = "resources";
const char kChromeUISessionsHost[] = "sessions";
const char kChromeUISettingsHost[] = "settings";
const char kChromeUIShorthangHost[] = "shorthang";
const char kChromeUISSLClientCertificateSelectorHost[] = "select-cert";
const char kChromeUIStatsHost[] = "stats";
const char kChromeUISyncHost[] = "sync";
const char kChromeUISyncInternalsHost[] = "sync-internals";
const char kChromeUISyncPromoHost[] = "syncpromo";
const char kChromeUISyncResourcesHost[] = "syncresources";
const char kChromeUITaskManagerHost[] = "tasks";
const char kChromeUITCMallocHost[] = "tcmalloc";
const char kChromeUITermsHost[] = "terms";
const char kChromeUIThumbnailHost[] = "thumb";
const char kChromeUITouchIconHost[] = "touch-icon";
const char kChromeUITracingHost[] = "tracing";
const char kChromeUIVersionHost[] = "version";
const char kChromeUIWorkersHost[] = "workers";

const char kChromeUIScreenshotPath[] = "screenshots";
const char kChromeUIThemePath[] = "theme";

#if defined(OS_LINUX) || defined(OS_OPENBSD)
const char kChromeUILinuxProxyConfigHost[] = "linux-proxy-config";
const char kChromeUISandboxHost[] = "sandbox";
#endif

#if defined(OS_CHROMEOS)
const char kChromeUIActivationMessageHost[] = "activationmessage";
const char kChromeUIActiveDownloadsHost[] = "active-downloads";
const char kChromeUIChooseMobileNetworkHost[] = "choose-mobile-network";
const char kChromeUICryptohomeHost[] = "cryptohome";
const char kChromeUIDiscardsHost[] = "discards";
const char kChromeUIImageBurnerHost[] = "imageburner";
const char kChromeUIKeyboardOverlayHost[] = "keyboardoverlay";
const char kChromeUILockScreenHost[] = "lock";
const char kChromeUILoginContainerHost[] = "login-container";
const char kChromeUILoginHost[] = "login";
const char kChromeUIMediaplayerHost[] = "mediaplayer";
const char kChromeUIMobileSetupHost[] = "mobilesetup";
const char kChromeUINetworkHost[] = "network";
const char kChromeUIOobeHost[] = "oobe";
const char kChromeUIOSCreditsHost[] = "os-credits";
const char kChromeUIProxySettingsHost[] = "proxy-settings";
const char kChromeUIRegisterPageHost[] = "register";
const char kChromeUIRotateHost[] = "rotate";
const char kChromeUISimUnlockHost[] = "sim-unlock";
const char kChromeUISlideshowHost[] = "slideshow";
const char kChromeUISystemInfoHost[] = "system";
const char kChromeUIUserImageHost[] = "userimage";

const char kChromeUIMenu[] = "menu";
const char kChromeUINetworkMenu[] = "network-menu";
const char kChromeUIWrenchMenu[] = "wrench-menu";

const char kEULAPathFormat[] = "/usr/share/chromeos-assets/eula/%s/eula.html";
const char kOemEulaURLPath[] = "oem";
#endif

#if defined(FILE_MANAGER_EXTENSION)
const char kChromeUIFileManagerHost[] = "files";
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
const char kChromeUICollectedCookiesHost[] = "collected-cookies";
const char kChromeUIHttpAuthHost[] = "http-auth";
const char kChromeUIRepostFormWarningHost[] = "repost-form-warning";
#endif

#if defined(USE_AURA)
const char kChromeUIAppListHost[] = "app-list";
#endif

// Option sub pages.
// Add sub page paths to kChromeSettingsSubPages in builtin_provider.cc to be
// listed by the built-in AutocompleteProvider.
const char kAdvancedOptionsSubPage[] = "advanced";
const char kAutofillSubPage[] = "autofill";
const char kBrowserOptionsSubPage[] = "browser";
const char kClearBrowserDataSubPage[] = "clearBrowserData";
const char kContentSettingsExceptionsSubPage[] = "contentExceptions";
const char kContentSettingsSubPage[] = "content";
const char kExtensionsSubPage[] = "extensions";
const char kHandlerSettingsSubPage[] = "handlers";
const char kImportDataSubPage[] = "importData";
const char kInstantConfirmPage[] = "instantConfirm";
const char kLanguageOptionsSubPage[] = "languages";
const char kManageProfileSubPage[] = "manageProfile";
const char kPasswordManagerSubPage[] = "passwords";
const char kPersonalOptionsSubPage[] = "personal";
const char kSearchEnginesSubPage[] = "searchEngines";
const char kSyncSetupSubPage[] = "syncSetup";
#if defined(OS_CHROMEOS)
const char kAboutOptionsSubPage[] = "about";
const char kInternetOptionsSubPage[] = "internet";
const char kSystemOptionsSubPage[] = "system";
#endif

const char kSyncGoogleDashboardURL[] = "https://www.google.com/dashboard/";

const char kPasswordManagerLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://www.google.com/support/chromeos/bin/answer.py?answer=95606";
#else
    "https://www.google.com/support/chrome/bin/answer.py?answer=95606";
#endif

const char kChromeHelpURL[] =
#if defined(OS_CHROMEOS)
  "https://www.google.com/support/chromeos/";
#else
  "https://www.google.com/support/chrome/";
#endif

const char kPageInfoHelpCenterURL[] =
#if defined(OS_CHROMEOS)
    "https://www.google.com/support/chromeos/bin/answer.py?answer=95617";
#else
    "https://www.google.com/support/chrome/bin/answer.py?answer=95617";
#endif

const char kCrashReasonURL[] =
#if defined(OS_CHROMEOS)
    "https://www.google.com/support/chromeos/bin/answer.py?answer=1047340";
#else
    "https://www.google.com/support/chrome/bin/answer.py?answer=95669";
#endif

const char kKillReasonURL[] =
    "http://www.google.com/support/chrome/bin/answer.py?answer=1270364";

const char kPrivacyLearnMoreURL[] =
#if defined(OS_CHROMEOS)
    "https://www.google.com/support/chromeos/bin/answer.py?answer=1047334";
#else
    "https://www.google.com/support/chrome/bin/answer.py?answer=114836";
#endif

const char kChromiumProjectURL[] = "http://code.google.com/chromium/";

const char kLearnMoreReportingURL[] =
    "https://www.google.com/support/chrome/bin/answer.py?answer=96817";

const char kOutdatedPluginLearnMoreURL[] =
    "https://www.google.com/support/chrome/bin/answer.py?answer=1181003";

const char kBlockedPluginLearnMoreURL[] =
    "https://www.google.com/support/chrome/bin/answer.py?answer=1247383";

const char kSpeechInputAboutURL[] =
    "https://www.google.com/support/chrome/bin/answer.py?answer=1407892";

const char kLearnMoreRegisterProtocolHandlerURL[] =
    "http://www.google.com/support/chrome/bin/answer.py?answer=1382847";

const char kSyncLearnMoreURL[] =
    "http://www.google.com/support/chrome/bin/answer.py?answer=165139";

const char kDownloadScanningLearnMoreURL[] =
    "http://www.google.com/support/chrome/bin/answer.py?answer=99020";

#if defined(OS_CHROMEOS)
const char kCloudPrintLearnMoreURL[] =
    "https://www.google.com/support/chromeos/bin/topic.py?topic=29023";
#endif

const char* const kChromeDebugURLs[] = {
  kChromeUICrashURL,
  kChromeUIKillURL,
  kChromeUIHangURL,
  kChromeUIShorthangURL,
  kChromeUIGpuCleanURL,
  kChromeUIGpuCrashURL,
  kChromeUIGpuHangURL,
};
int kNumberOfChromeDebugURLs = static_cast<int>(arraysize(kChromeDebugURLs));

const char kExtensionScheme[] = "chrome-extension";

void RegisterChromeSchemes() {
  url_util::AddStandardScheme(kExtensionScheme);
#if defined(OS_CHROMEOS)
  url_util::AddStandardScheme(kCrosScheme);
#endif

  // This call will also lock the list of standard schemes.
  RegisterContentSchemes(kSavableSchemes);
}

}  // namespace chrome
