// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Cookies API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CLEAR_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CLEAR_API_CONSTANTS_H_
#pragma once

namespace extension_clear_api_constants {

// Keys.
extern const char kCacheKey[];
extern const char kCookiesKey[];
extern const char kDownloadsKey[];
extern const char kFormDataKey[];
extern const char kHistoryKey[];
extern const char kPasswordsKey[];

// Timeframe "enum" values.
extern const char kHourEnum[];
extern const char kDayEnum[];
extern const char kWeekEnum[];
extern const char kMonthEnum[];
extern const char kEverythingEnum[];

// Errors!
extern const char kOneAtATimeError[];

}  // namespace extension_clear_api_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CLEAR_API_CONSTANTS_H_
