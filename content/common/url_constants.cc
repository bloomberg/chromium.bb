// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/url_constants.h"

#include "base/string_util.h"
#include "googleurl/src/url_util.h"

namespace {
const char* kDefaultSavableSchemes[] = {
  chrome::kHttpScheme,
  chrome::kHttpsScheme,
  chrome::kFileScheme,
  chrome::kFtpScheme,
  chrome::kChromeDevToolsScheme,
  chrome::kChromeUIScheme,
  NULL
};
char** g_savable_schemes = const_cast<char**>(kDefaultSavableSchemes);
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
const char kViewSourceScheme[] = "view-source";

const char kStandardSchemeSeparator[] = "://";

const char kAboutBlankURL[] = "about:blank";
const char kAboutCrashURL[] = "about:crash";

const char kUnreachableWebDataURL[] = "chrome://chromewebdata/";

const char** GetSavableSchemes() {
  return const_cast<const char**>(g_savable_schemes);
}

void RegisterContentSchemes(const char** additional_savable_schemes) {
  // Don't need "chrome-internal" which was used in old versions of Chrome for
  // the new tab page.
  url_util::AddStandardScheme(kChromeDevToolsScheme);
  url_util::AddStandardScheme(kChromeUIScheme);
  url_util::AddStandardScheme(kMetadataScheme);

  // Prevent future modification of the standard schemes list. This is to
  // prevent accidental creation of data races in the program. AddStandardScheme
  // isn't threadsafe so must be called when GURL isn't used on any other
  // thread. This is really easy to mess up, so we say that all calls to
  // AddStandardScheme in Chrome must be inside this function.
  url_util::LockStandardSchemes();

  // We rely on the above lock to protect this part from being invoked twice.
  if (additional_savable_schemes) {
    int schemes = 0;
    while (additional_savable_schemes[++schemes]);
    // The array, and the copied schemes won't be freed, but will remain
    // reachable.
    g_savable_schemes = new char*[schemes + arraysize(kDefaultSavableSchemes)];
    memcpy(g_savable_schemes,
           kDefaultSavableSchemes,
           arraysize(kDefaultSavableSchemes) * sizeof(char*));
    for (int i = 0; i < schemes; ++i) {
      g_savable_schemes[arraysize(kDefaultSavableSchemes) + i - 1] =
          base::strdup(additional_savable_schemes[i]);
    }
    g_savable_schemes[arraysize(kDefaultSavableSchemes) + schemes - 1] = 0;
  }
}

}  // namespace chrome
