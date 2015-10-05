// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CHROME_URL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_CHROME_URL_CONSTANTS_H_

#include <stddef.h>

// Contains constants for known URLs and portions thereof.

// TODO(blundell): This file should be ios_chrome_url_constants.*, and all of
// these constants should have a kIOSChrome prefix instead of a kChrome
// prefix. crbug.com/537174

// chrome: URLs (including schemes). Should be kept in sync with the
// components below.
extern const char kChromeUINewTabURL[];

// URL components for Chrome on iOS.
extern const char kChromeUIBookmarksHost[];
extern const char kChromeUIChromeURLsHost[];
extern const char kChromeUICreditsHost[];
extern const char kChromeUIExternalFileHost[];
extern const char kChromeUIFlagsHost[];
extern const char kChromeUIHistogramHost[];
extern const char kChromeUIHistoryHost[];
extern const char kChromeUINetExportHost[];
extern const char kChromeUINewTabHost[];
extern const char kChromeUIOmahaHost[];
extern const char kChromeUITermsHost[];
extern const char kChromeUISyncInternalsHost[];
extern const char kChromeUIVersionHost[];

// Gets the hosts/domains that are shown in chrome://chrome-urls.
extern const char* const kChromeHostURLs[];
extern const size_t kNumberOfChromeHostURLs;

#endif  // IOS_CHROME_BROWSER_CHROME_URL_CONSTANTS_H_
