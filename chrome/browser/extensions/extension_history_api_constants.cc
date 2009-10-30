// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_history_api_constants.h"

namespace extension_history_api_constants {

const wchar_t kAllHistoryKey[] = L"allHistory";
const wchar_t kEndTimeKey[] = L"endTime";
const wchar_t kFavIconUrlKey[] = L"favIconUrl";
const wchar_t kIdKey[] = L"id";
const wchar_t kLastVisitdKey[] = L"lastVisitTime";
const wchar_t kMaxResultsKey[] = L"maxResults";
const wchar_t kNewKey[] = L"new";
const wchar_t kReferringVisitId[] = L"referringVisitId";
const wchar_t kRemovedKey[] = L"removed";
const wchar_t kSearchKey[] = L"search";
const wchar_t kStartTimeKey[] = L"startTime";
const wchar_t kTitleKey[] = L"title";
const wchar_t kTypedCountKey[] = L"typedCount";
const wchar_t kVisitCountKey[] = L"visitCount";
const wchar_t kTransition[] = L"transition";
const wchar_t kUrlKey[] = L"url";
const wchar_t kUrlsKey[] = L"urls";
const wchar_t kVisitId[] = L"visitId";
const wchar_t kVisitTime[] = L"visitTime";

const char kOnVisited[] = "experimental.history.onVisited";
const char kOnVisitRemoved[] = "experimental.history.onVisitRemoved";

const char kInvalidIdError[] = "History item id is invalid.";
const char kInvalidUrlError[] = "Url is invalid.";

}  // namespace extension_history_api_constants
