// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used to for the History API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_HISTORY_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_HISTORY_API_CONSTANTS_H_

namespace extension_history_api_constants {

// Keys.
extern const wchar_t kAllHistoryKey[];
extern const wchar_t kEndTimeKey[];
extern const wchar_t kFavIconUrlKey[];
extern const wchar_t kIdKey[];
extern const wchar_t kLastVisitdKey[];
extern const wchar_t kMaxResultsKey[];
extern const wchar_t kNewKey[];
extern const wchar_t kReferringVisitId[];
extern const wchar_t kRemovedKey[];
extern const wchar_t kStartTimeKey[];
extern const wchar_t kTextKey[];
extern const wchar_t kTitleKey[];
extern const wchar_t kTypedCountKey[];
extern const wchar_t kVisitCountKey[];
extern const wchar_t kTransition[];
extern const wchar_t kUrlKey[];
extern const wchar_t kUrlsKey[];
extern const wchar_t kVisitId[];
extern const wchar_t kVisitTime[];

// Events.
extern const char kOnVisited[];
extern const char kOnVisitRemoved[];

// Errors.
extern const char kInvalidIdError[];
extern const char kInvalidUrlError[];

};  // namespace extension_history_api_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_HISTORY_API_CONSTANTS_H_
