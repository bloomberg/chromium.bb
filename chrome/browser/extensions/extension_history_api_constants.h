// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used to for the History API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_HISTORY_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_HISTORY_API_CONSTANTS_H_
#pragma once

namespace extension_history_api_constants {

// Keys.
extern const char kAllHistoryKey[];
extern const char kEndTimeKey[];
extern const char kFaviconUrlKey[];
extern const char kIdKey[];
extern const char kLastVisitdKey[];
extern const char kMaxResultsKey[];
extern const char kNewKey[];
extern const char kReferringVisitId[];
extern const char kRemovedKey[];
extern const char kStartTimeKey[];
extern const char kTextKey[];
extern const char kTitleKey[];
extern const char kTypedCountKey[];
extern const char kVisitCountKey[];
extern const char kTransition[];
extern const char kUrlKey[];
extern const char kUrlsKey[];
extern const char kVisitId[];
extern const char kVisitTime[];

// Events.
extern const char kOnVisited[];
extern const char kOnVisitRemoved[];

// Errors.
extern const char kInvalidIdError[];
extern const char kInvalidUrlError[];

};  // namespace extension_history_api_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_HISTORY_API_CONSTANTS_H_
