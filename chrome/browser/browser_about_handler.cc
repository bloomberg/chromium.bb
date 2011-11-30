// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_about_handler.h"

#include <string>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/about_handler.h"
#include "chrome/common/url_constants.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/sensors/sensors_provider.h"

#if defined(USE_TCMALLOC)
#include "third_party/tcmalloc/chromium/src/google/malloc_extension.h"
#endif

namespace {

// Add paths here to be included in chrome://chrome-urls (about:about).
// These paths will also be suggested by BuiltinProvider.
const char* const kChromePaths[] = {
  chrome::kChromeUIAppCacheInternalsHost,
  chrome::kChromeUIBlobInternalsHost,
  chrome::kChromeUIBookmarksHost,
  chrome::kChromeUICacheHost,
  chrome::kChromeUIChromeURLsHost,
  chrome::kChromeUICrashesHost,
  chrome::kChromeUICreditsHost,
  chrome::kChromeUIDNSHost,
  chrome::kChromeUIDownloadsHost,
  chrome::kChromeUIExtensionsHost,
  chrome::kChromeUIFlagsHost,
  chrome::kChromeUIFlashHost,
  chrome::kChromeUIGpuInternalsHost,
  chrome::kChromeUIHistogramsHost,
  chrome::kChromeUIHistoryHost,
  chrome::kChromeUIIPCHost,
  chrome::kChromeUIMediaInternalsHost,
  chrome::kChromeUIMemoryHost,
  chrome::kChromeUINetInternalsHost,
  chrome::kChromeUINetworkViewCacheHost,
  chrome::kChromeUINewTabHost,
  chrome::kChromeUIPluginsHost,
  chrome::kChromeUIPrintHost,
  chrome::kChromeUIProfilerHost,
  chrome::kChromeUIQuotaInternalsHost,
  chrome::kChromeUISessionsHost,
  chrome::kChromeUISettingsHost,
  chrome::kChromeUIStatsHost,
  chrome::kChromeUISyncInternalsHost,
  chrome::kChromeUITaskManagerHost,
  chrome::kChromeUITCMallocHost,
  chrome::kChromeUITermsHost,
  chrome::kChromeUITracingHost,
  chrome::kChromeUIVersionHost,
  chrome::kChromeUIWorkersHost,
#if defined(OS_WIN)
  chrome::kChromeUIConflictsHost,
#endif
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  chrome::kChromeUILinuxProxyConfigHost,
  chrome::kChromeUISandboxHost,
#endif
#if defined(OS_CHROMEOS)
  chrome::kChromeUIActiveDownloadsHost,
  chrome::kChromeUIChooseMobileNetworkHost,
  chrome::kChromeUICryptohomeHost,
  chrome::kChromeUIDiscardsHost,
  chrome::kChromeUIImageBurnerHost,
  chrome::kChromeUIKeyboardOverlayHost,
  chrome::kChromeUILoginHost,
  chrome::kChromeUINetworkHost,
  chrome::kChromeUIOobeHost,
  chrome::kChromeUIOSCreditsHost,
  chrome::kChromeUIProxySettingsHost,
  chrome::kChromeUISystemInfoHost,
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
  // TAB_CONTENTS_WEB handles about:blank, which frames are allowed to access.
  if (!url->SchemeIs(chrome::kChromeUIScheme))
    return false;

  // Circumvent processing URLs that the renderer process will handle.
  if (chrome_about_handler::WillHandle(*url))
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
  // Redirect chrome://extensions to chrome://settings/extensions.
  } else if (host == chrome::kChromeUIExtensionsHost) {
    host = chrome::kChromeUISettingsHost;
    path = chrome::kExtensionsSubPage;
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
    browser::ShowAboutIPCDialog();
    return true;
  }
#endif

#endif  // OFFICIAL_BUILD

  // Handle URLs to crash the browser or wreck the gpu process.
  if (host == chrome::kChromeUIBrowserCrashHost) {
    // Induce an intentional crash in the browser process.
    CHECK(false);
  }

  if (host == chrome::kChromeUIGpuCleanHost) {
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim)
      shim->SimulateRemoveAllContext();
    return true;
  }

  if (host == chrome::kChromeUIGpuCrashHost) {
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim)
      shim->SimulateCrash();
    return true;
  }

  if (host == chrome::kChromeUIGpuHangHost) {
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim)
      shim->SimulateHang();
    return true;
  }

#if defined(OS_CHROMEOS)
  if (host == chrome::kChromeUIRotateHost) {
    content::ScreenOrientation change = content::SCREEN_ORIENTATION_TOP;
    std::string query(url.query());
    if (query == "left") {
      change = content::SCREEN_ORIENTATION_LEFT;
    } else if (query == "right") {
      change = content::SCREEN_ORIENTATION_RIGHT;
    } else if (query == "top") {
      change = content::SCREEN_ORIENTATION_TOP;
    } else if (query == "bottom") {
      change = content::SCREEN_ORIENTATION_BOTTOM;
    } else {
      NOTREACHED() << "Unknown orientation";
    }
    sensors::Provider::GetInstance()->ScreenOrientationChanged(change);
    return true;
  }
#endif

  return false;
}

std::vector<std::string> ChromePaths() {
  std::vector<std::string> paths;
  paths.reserve(arraysize(kChromePaths));
  for (size_t i = 0; i < arraysize(kChromePaths); i++)
    paths.push_back(kChromePaths[i]);
  return paths;
}

#if defined(USE_TCMALLOC)
// static
AboutTcmallocOutputs* AboutTcmallocOutputs::GetInstance() {
  return Singleton<AboutTcmallocOutputs>::get();
}

AboutTcmallocOutputs::AboutTcmallocOutputs() {}

AboutTcmallocOutputs::~AboutTcmallocOutputs() {}

// Glue between the callback task and the method in the singleton.
void AboutTcmallocRendererCallback(base::ProcessId pid,
                                   const std::string& output) {
  AboutTcmallocOutputs::GetInstance()->RendererCallback(pid, output);
}
#endif
