// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/history/history_entry.h"

namespace history {

HistoryEntry::HistoryEntry(HistoryEntry::EntryType entry_type,
                           const GURL& url,
                           const base::string16& title,
                           base::Time time,
                           const std::string& client_id,
                           bool is_search_result,
                           const base::string16& snippet,
                           bool blocked_visit)
    : entry_type(entry_type),
      url(url),
      title(title),
      time(time),
      client_id(client_id),
      is_search_result(is_search_result),
      snippet(snippet),
      blocked_visit(blocked_visit) {
  all_timestamps.insert(time.ToInternalValue());
}

HistoryEntry::HistoryEntry()
    : entry_type(EMPTY_ENTRY), is_search_result(false), blocked_visit(false) {}

HistoryEntry::HistoryEntry(const HistoryEntry& other) = default;

HistoryEntry::~HistoryEntry() {}

bool HistoryEntry::SortByTimeDescending(const HistoryEntry& entry1,
                                        const HistoryEntry& entry2) {
  return entry1.time > entry2.time;
}

}  // namespace history
