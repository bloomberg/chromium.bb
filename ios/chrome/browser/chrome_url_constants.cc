// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/chrome_url_constants.h"

#include "base/macros.h"

const char kChromeUINewTabURL[] = "chrome://newtab/";

const char kChromeUIBookmarksHost[] = "bookmarks";
const char kChromeUIChromeURLsHost[] = "chrome-urls";
const char kChromeUICreditsHost[] = "credits";
const char kChromeUIExternalFileHost[] = "external-file";
const char kChromeUIFlagsHost[] = "flags";
const char kChromeUIHistogramHost[] = "histograms";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUINetExportHost[] = "net-export";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUIOmahaHost[] = "omaha";
const char kChromeUITermsHost[] = "terms";
const char kChromeUISyncInternalsHost[] = "sync-internals";
const char kChromeUIVersionHost[] = "version";

// Add hosts here to be included in chrome://chrome-urls (about:about).
// These hosts will also be suggested by BuiltinProvider.
// 'histograms' is chrome WebUI on iOS, content WebUI on other platforms.
const char* const kChromeHostURLs[] = {
    kChromeUIBookmarksHost, kChromeUIChromeURLsHost, kChromeUICreditsHost,
    kChromeUIFlagsHost,     kChromeUIHistogramHost,  kChromeUIHistoryHost,
    kChromeUINetExportHost, kChromeUINewTabHost,     kChromeUISyncInternalsHost,
    kChromeUITermsHost,     kChromeUIVersionHost,
};
const size_t kNumberOfChromeHostURLs = arraysize(kChromeHostURLs);
