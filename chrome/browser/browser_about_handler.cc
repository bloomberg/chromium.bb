// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_about_handler.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"

namespace {

// Add paths here to be included in chrome://chrome-urls (about:about).
// These paths will also be suggested by BuiltinProvider.
const char* const kPaths[] = {
  chrome::kChromeUIAppCacheInternalsHost,
  chrome::kChromeUIBlobInternalsHost,
  chrome::kChromeUICacheHost,
  chrome::kChromeUIChromeURLsHost,
  chrome::kChromeUICrashesHost,
  chrome::kChromeUICreditsHost,
  chrome::kChromeUIDNSHost,
  chrome::kChromeUIFlagsHost,
  chrome::kChromeUIGpuInternalsHost,
  chrome::kChromeUIHistoryHost,
  chrome::kChromeUIIPCHost,
  chrome::kChromeUIMediaInternalsHost,
  chrome::kChromeUIMemoryHost,
  chrome::kChromeUINetInternalsHost,
  chrome::kChromeUINetworkViewCacheHost,
  chrome::kChromeUINewTabHost,
  chrome::kChromeUIOmniboxHost,
  chrome::kChromeUIPredictorsHost,
  chrome::kChromeUIProfilerHost,
  chrome::kChromeUIQuotaInternalsHost,
  chrome::kChromeUISignInInternalsHost,
  chrome::kChromeUIStatsHost,
  chrome::kChromeUISyncInternalsHost,
  chrome::kChromeUITermsHost,
  chrome::kChromeUIUserActionsHost,
  chrome::kChromeUIVersionHost,
#if defined(OS_ANDROID)
  chrome::kChromeUIWelcomeHost,
#else
  chrome::kChromeUIBookmarksHost,
  chrome::kChromeUIDownloadsHost,
  chrome::kChromeUIFlashHost,
  chrome::kChromeUIInspectHost,
  chrome::kChromeUIPluginsHost,
  chrome::kChromeUISettingsHost,
  chrome::kChromeUITracingHost,
#endif
#if defined(OS_WIN)
  chrome::kChromeUIConflictsHost,
#endif
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  chrome::kChromeUILinuxProxyConfigHost,
  chrome::kChromeUISandboxHost,
#endif
#if defined(OS_CHROMEOS)
  chrome::kChromeUIChooseMobileNetworkHost,
  chrome::kChromeUICryptohomeHost,
  chrome::kChromeUIDiagnosticsHost,
  chrome::kChromeUIDiscardsHost,
  chrome::kChromeUIDriveInternalsHost,
  chrome::kChromeUIImageBurnerHost,
  chrome::kChromeUIKeyboardOverlayHost,
  chrome::kChromeUILoginHost,
  chrome::kChromeUINetworkHost,
  chrome::kChromeUIOobeHost,
  chrome::kChromeUIOSCreditsHost,
  chrome::kChromeUIProxySettingsHost,
  chrome::kChromeUISystemInfoHost,
  chrome::kChromeUITaskManagerHost,
#endif
#if !defined(DISABLE_NACL)
  chrome::kChromeUINaClHost,
#endif
#if defined(ENABLE_CONFIGURATION_POLICY)
  chrome::kChromeUIPolicyHost,
#endif
#if defined(ENABLE_EXTENSIONS)
  chrome::kChromeUIExtensionsHost,
#endif
#if defined(ENABLE_PRINTING)
  chrome::kChromeUIPrintHost,
#endif
};

}  // namespace

bool WillHandleBrowserAboutURL(GURL* url,
                               content::BrowserContext* browser_context) {
  // TODO(msw): Eliminate "about:*" constants and literals from code and tests,
  //            then hopefully we can remove this forced fixup.
  *url = URLFixerUpper::FixupURL(url->possibly_invalid_spec(), std::string());

  // Check that about: URLs are fixed up to chrome: by URLFixerUpper::FixupURL.
  DCHECK((*url == GURL(chrome::kAboutBlankURL)) ||
         !url->SchemeIs(chrome::kAboutScheme));

  // Only handle chrome://foo/, URLFixerUpper::FixupURL translates about:foo.
  if (!url->SchemeIs(chrome::kChromeUIScheme))
    return false;

  std::string host(url->host());
  std::string path;
  // Replace about with chrome-urls.
  if (host == chrome::kChromeUIAboutHost)
    host = chrome::kChromeUIChromeURLsHost;
  // Replace cache with view-http-cache.
  if (host == chrome::kChromeUICacheHost) {
    host = chrome::kChromeUINetworkViewCacheHost;
  // Replace gpu with gpu-internals.
  } else if (host == chrome::kChromeUIGpuHost) {
    host = chrome::kChromeUIGpuInternalsHost;
  // Replace sync with sync-internals (for legacy reasons).
  } else if (host == chrome::kChromeUISyncHost) {
    host = chrome::kChromeUISyncInternalsHost;
  // Redirect chrome://extensions.
  } else if (host == chrome::kChromeUIExtensionsHost) {
    host = chrome::kChromeUIUberHost;
    path = chrome::kChromeUIExtensionsHost + url->path();
  // Redirect chrome://settings/extensions.
  // TODO(csilv): Remove this URL after M22 (legacy URL).
  } else if (host == chrome::kChromeUISettingsHost &&
      url->path() == std::string("/") + chrome::kExtensionsSubPage) {
    host = chrome::kChromeUIUberHost;
    path = chrome::kChromeUIExtensionsHost;
  // Redirect chrome://history.
  } else if (host == chrome::kChromeUIHistoryHost) {
#if defined(OS_ANDROID)
    // On Android, redirect directly to chrome://history-frame since
    // uber page is unsupported.
    host = chrome::kChromeUIHistoryFrameHost;
#else
    host = chrome::kChromeUIUberHost;
    path = chrome::kChromeUIHistoryHost + url->path();
#endif
  // Redirect chrome://settings
  } else if (host == chrome::kChromeUISettingsHost) {
    host = chrome::kChromeUIUberHost;
    path = chrome::kChromeUISettingsHost + url->path();
  // Redirect chrome://help
  } else if (host == chrome::kChromeUIHelpHost) {
    host = chrome::kChromeUIUberHost;
    path = chrome::kChromeUIHelpHost + url->path();
  }
  GURL::Replacements replacements;
  replacements.SetHostStr(host);
  if (!path.empty())
    replacements.SetPathStr(path);
  *url = url->ReplaceComponents(replacements);

  // Having re-written the URL, make the chrome: handler process it.
  return false;
}

bool HandleNonNavigationAboutURL(const GURL& url) {
  std::string host(url.host());

  // chrome://ipc/ is currently buggy, so we disable it for official builds.
#if !defined(OFFICIAL_BUILD)

#if (defined(OS_MACOSX) || defined(OS_WIN)) && defined(IPC_MESSAGE_LOG_ENABLED)
  if (LowerCaseEqualsASCII(url.spec(), chrome::kChromeUIIPCURL)) {
    // Run the dialog. This will re-use the existing one if it's already up.
    chrome::ShowAboutIPCDialog();
    return true;
  }
#endif

#endif  // OFFICIAL_BUILD

  return false;
}

std::vector<std::string> ChromePaths() {
  std::vector<std::string> paths(kPaths, kPaths + arraysize(kPaths));
  std::sort(paths.begin(), paths.end());
  return paths;
}
