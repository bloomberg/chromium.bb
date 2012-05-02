// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/url_constants.h"

#include <algorithm>

#include "base/string_util.h"
#include "content/public/common/content_client.h"
#include "googleurl/src/url_util.h"

namespace {
const char* kDefaultSavableSchemes[] = {
  chrome::kHttpScheme,
  chrome::kHttpsScheme,
  chrome::kFileScheme,
  chrome::kFileSystemScheme,
  chrome::kFtpScheme,
  chrome::kChromeDevToolsScheme,
  chrome::kChromeUIScheme,
  chrome::kDataScheme,
  NULL
};
char** g_savable_schemes = const_cast<char**>(kDefaultSavableSchemes);

void AddStandardSchemeHelper(const std::string& scheme) {
  url_util::AddStandardScheme(scheme.c_str());
}

}  // namespace

namespace chrome {

const char kAboutScheme[] = "about";
const char kBlobScheme[] = "blob";

// Before adding new chrome schemes please check with security@chromium.org.
// There are security implications associated with introducing new schemes.
const char kChromeDevToolsScheme[] = "chrome-devtools";
const char kChromeInternalScheme[] = "chrome-internal";
const char kChromeUIScheme[] = "chrome";
const char kDataScheme[] = "data";
const char kFileScheme[] = "file";
const char kFileSystemScheme[] = "filesystem";
const char kFtpScheme[] = "ftp";
const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
const char kJavaScriptScheme[] = "javascript";
const char kMailToScheme[] = "mailto";
const char kMetadataScheme[] = "metadata";
const char kSwappedOutScheme[] = "swappedout";
const char kViewSourceScheme[] = "view-source";

const char kAboutBlankURL[] = "about:blank";
const char kChromeUIAppCacheInternalsHost[] = "appcache-internals";
const char kChromeUIBlobInternalsHost[] = "blob-internals";
const char kChromeUIBrowserCrashHost[] = "inducebrowsercrashforrealz";
const char kChromeUINetworkViewCacheHost[] = "view-http-cache";
const char kChromeUITcmallocHost[] = "tcmalloc";
const char kChromeUICrashURL[] = "chrome://crash";
const char kChromeUIGpuCleanURL[] = "chrome://gpuclean";
const char kChromeUIGpuCrashURL[] = "chrome://gpucrash";
const char kChromeUIGpuHangURL[] = "chrome://gpuhang";
const char kChromeUIHangURL[] = "chrome://hang";
const char kChromeUIKillURL[] = "chrome://kill";
const char kChromeUINetworkViewCacheURL[] = "chrome://view-http-cache/";
const char kChromeUIShorthangURL[] = "chrome://shorthang";

}  // namespace chrome

namespace content {

const char kStandardSchemeSeparator[] = "://";

// This error URL is loaded in normal web renderer processes, so it should not
// have a chrome:// scheme that might let it be confused with a WebUI page.
const char kUnreachableWebDataURL[] = "data:text/html,chromewebdata";

// This URL is loaded when a page is swapped out and replaced by a page in a
// different renderer process.  It must have a unique origin that cannot be
// scripted by other pages in the process.
const char kSwappedOutURL[] = "swappedout://";

const char** GetSavableSchemes() {
  return const_cast<const char**>(g_savable_schemes);
}

void RegisterContentSchemes(bool lock_standard_schemes) {
  std::vector<std::string> additional_standard_schemes;
  std::vector<std::string> additional_savable_schemes;
  GetContentClient()->AddAdditionalSchemes(
       &additional_standard_schemes,
       &additional_savable_schemes);

  // Don't need "chrome-internal" which was used in old versions of Chrome for
  // the new tab page.
  url_util::AddStandardScheme(chrome::kChromeDevToolsScheme);
  url_util::AddStandardScheme(chrome::kChromeUIScheme);
  url_util::AddStandardScheme(chrome::kMetadataScheme);
  std::for_each(additional_standard_schemes.begin(),
                additional_standard_schemes.end(),
                AddStandardSchemeHelper);

  // Prevent future modification of the standard schemes list. This is to
  // prevent accidental creation of data races in the program. AddStandardScheme
  // isn't threadsafe so must be called when GURL isn't used on any other
  // thread. This is really easy to mess up, so we say that all calls to
  // AddStandardScheme in Chrome must be inside this function.
  if (lock_standard_schemes)
    url_util::LockStandardSchemes();

  // We rely on the above lock to protect this part from being invoked twice.
  if (!additional_savable_schemes.empty()) {
    int schemes = static_cast<int>(additional_savable_schemes.size());
    // The array, and the copied schemes won't be freed, but will remain
    // reachable.
    g_savable_schemes = new char*[schemes + arraysize(kDefaultSavableSchemes)];
    memcpy(g_savable_schemes,
           kDefaultSavableSchemes,
           arraysize(kDefaultSavableSchemes) * sizeof(char*));
    for (int i = 0; i < schemes; ++i) {
      g_savable_schemes[arraysize(kDefaultSavableSchemes) + i - 1] =
          base::strdup(additional_savable_schemes[i].c_str());
    }
    g_savable_schemes[arraysize(kDefaultSavableSchemes) + schemes - 1] = 0;
  }
}

}  // namespace content
