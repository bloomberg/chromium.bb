// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/new_tab_page_url_handler.h"

#include <string>

#include "base/strings/string_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace {
const char kLegacyBookmarksFragment[] = "bookmarks";
const char kLegacyOpenTabsFragment[] = "open_tabs";
const char kLegacyRecentTabsHost[] = "recent_tabs";
}

namespace chrome {
namespace android {

bool HandleAndroidNewTabURL(GURL* url,
                            content::BrowserContext* browser_context) {
  if (url->SchemeIs(content::kChromeUIScheme) &&
      url->host() == chrome::kChromeUINewTabHost) {
    std::string ref = url->ref();
    if (StartsWithASCII(ref, kLegacyBookmarksFragment, true)) {
      *url = GURL(chrome::kChromeUINativeBookmarksURL);
    } else if (ref == kLegacyOpenTabsFragment) {
      *url = GURL(chrome::kChromeUINativeRecentTabsURL);
    } else {
      *url = GURL(chrome::kChromeUINativeNewTabURL);
    }
    return true;
  }

  if (url->SchemeIs(chrome::kChromeNativeScheme) &&
      url->host() == kLegacyRecentTabsHost) {
    *url = GURL(chrome::kChromeUINativeRecentTabsURL);
    return true;
  }

  return false;
}

}  // namespace android
}  // namespace chrome
