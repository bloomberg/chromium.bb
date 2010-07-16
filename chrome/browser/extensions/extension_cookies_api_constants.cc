// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_cookies_api_constants.h"

namespace extension_cookies_api_constants {

const wchar_t kCookieKey[] = L"cookie";
const wchar_t kDomainKey[] = L"domain";
const wchar_t kExpirationDateKey[] = L"expirationDate";
const wchar_t kHostOnlyKey[] = L"hostOnly";
const wchar_t kHttpOnlyKey[] = L"httpOnly";
const wchar_t kIdKey[] = L"id";
const wchar_t kNameKey[] = L"name";
const wchar_t kPathKey[] = L"path";
const wchar_t kRemovedKey[] = L"removed";
const wchar_t kSecureKey[] = L"secure";
const wchar_t kSessionKey[] = L"session";
const wchar_t kStoreIdKey[] = L"storeId";
const wchar_t kTabIdsKey[] = L"tabIds";
const wchar_t kUrlKey[] = L"url";
const wchar_t kValueKey[] = L"value";

const char kOnChanged[] = "cookies.onChanged";

const char kCookieSetFailedError[] =
    "Failed to parse or set cookie named \"*\".";
const char kInvalidStoreIdError[] = "Invalid cookie store id: \"*\".";
const char kInvalidUrlError[] = "Invalid url: \"*\".";
const char kNoCookieStoreFoundError[] =
    "No accessible cookie store found for the current execution context.";
const char kNoHostPermissionsError[] =
    "No host permissions for cookies at url: \"*\".";

}  // namespace extension_cookies_api_constants
