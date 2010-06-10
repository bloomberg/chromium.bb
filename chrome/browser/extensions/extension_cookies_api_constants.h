// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Cookies API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_API_CONSTANTS_H_

namespace extension_cookies_api_constants {

// Keys.
extern const wchar_t kCookieKey[];
extern const wchar_t kDomainKey[];
extern const wchar_t kExpirationDateKey[];
extern const wchar_t kHostOnlyKey[];
extern const wchar_t kHttpOnlyKey[];
extern const wchar_t kIdKey[];
extern const wchar_t kNameKey[];
extern const wchar_t kPathKey[];
extern const wchar_t kRemovedKey[];
extern const wchar_t kSecureKey[];
extern const wchar_t kSessionKey[];
extern const wchar_t kStoreIdKey[];
extern const wchar_t kTabIdsKey[];
extern const wchar_t kUrlKey[];
extern const wchar_t kValueKey[];

// Events.
extern const char kOnChanged[];

// Errors.
extern const char kCookieSetFailedError[];
extern const char kInvalidStoreIdError[];
extern const char kInvalidUrlError[];
extern const char kNoCookieStoreFoundError[];
extern const char kNoHostPermissionsError[];

}  // namespace extension_cookies_api_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_API_CONSTANTS_H_
