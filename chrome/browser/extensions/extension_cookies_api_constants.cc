// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_cookies_api_constants.h"

namespace extension_cookies_api_constants {

const char kCookieKey[] = "cookie";
const char kDomainKey[] = "domain";
const char kExpirationDateKey[] = "expirationDate";
const char kHostOnlyKey[] = "hostOnly";
const char kHttpOnlyKey[] = "httpOnly";
const char kIdKey[] = "id";
const char kNameKey[] = "name";
const char kPathKey[] = "path";
const char kRemovedKey[] = "removed";
const char kSecureKey[] = "secure";
const char kSessionKey[] = "session";
const char kStoreIdKey[] = "storeId";
const char kTabIdsKey[] = "tabIds";
const char kUrlKey[] = "url";
const char kValueKey[] = "value";

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
