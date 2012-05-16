// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Cookies API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_API_CONSTANTS_H_
#pragma once

namespace extensions {
namespace cookies_api_constants {

// Keys.
extern const char kCauseKey[];
extern const char kCookieKey[];
extern const char kDomainKey[];
extern const char kExpirationDateKey[];
extern const char kHostOnlyKey[];
extern const char kHttpOnlyKey[];
extern const char kIdKey[];
extern const char kNameKey[];
extern const char kPathKey[];
extern const char kRemovedKey[];
extern const char kSecureKey[];
extern const char kSessionKey[];
extern const char kStoreIdKey[];
extern const char kTabIdsKey[];
extern const char kUrlKey[];
extern const char kValueKey[];

// Cause Constants
extern const char kEvictedChangeCause[];
extern const char kExpiredChangeCause[];
extern const char kExpiredOverwriteChangeCause[];
extern const char kExplicitChangeCause[];
extern const char kOverwriteChangeCause[];

// Events.
extern const char kOnChanged[];

// Errors.
extern const char kCookieSetFailedError[];
extern const char kInvalidStoreIdError[];
extern const char kInvalidUrlError[];
extern const char kNoCookieStoreFoundError[];
extern const char kNoHostPermissionsError[];

}  // namespace cookies_api_constants
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_API_CONSTANTS_H_
