// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants for known URLs and portions thereof.

#ifndef IOS_CHROME_BROWSER_CHROME_URL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_CHROME_URL_CONSTANTS_H_

// TODO(blundell): This file should be ios_chrome_url_constants.*, and all of
// these constants should have a kIOSChrome prefix instead of a kChrome
// prefix. crbug.com/537174

// chrome: URLs (including schemes). Should be kept in sync with the
// components below.
extern const char kChromeUINewTabURL[];

// URL components for Chrome on iOS.
extern const char kChromeUIExternalFileHost[];
extern const char kChromeUINetExportHost[];
extern const char kChromeUIOmahaHost[];
extern const char kChromeUISyncInternalsHost[];

#endif  // IOS_CHROME_BROWSER_CHROME_URL_CONSTANTS_H_
