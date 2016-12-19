// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_UTIL_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_UTIL_H_

#include <vector>

#include "base/strings/string16.h"

namespace base {
class Time;
}

namespace history {

struct HistoryEntry;

// Returns a localized version of |visit_time| including a relative
// indicator (e.g. today, yesterday).
base::string16 GetRelativeDateLocalized(const base::Time& visit_time);

// Sorts and merges duplicate entries from the query results, only retaining the
// most recent visit to a URL on a particular day. That visit contains the
// timestamps of the other visits.
void MergeDuplicateHistoryEntries(std::vector<history::HistoryEntry>* entries);
}  // namespace history

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_UTIL_H_
