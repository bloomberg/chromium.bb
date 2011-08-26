// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_clear_api_constants.h"

namespace extension_clear_api_constants {

// Keys.
const char kCacheKey[] = "cache";
const char kCookiesKey[] = "cookies";
const char kDownloadsKey[] = "downloads";
const char kFormDataKey[] = "formData";
const char kHistoryKey[] = "history";
const char kPasswordsKey[] = "passwords";

// Timeframe "enum" values.
const char kHourEnum[] = "last_hour";
const char kDayEnum[] = "last_day";
const char kWeekEnum[] = "last_week";
const char kMonthEnum[] = "last_month";
const char kEverythingEnum[] = "everything";

// Errors!
const char kOneAtATimeError[] = "Only one 'clear' API call can run at a time.";

}  // namespace extension_clear_api_constants
