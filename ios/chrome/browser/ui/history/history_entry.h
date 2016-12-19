// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_ENTRY_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_ENTRY_H_

#include <set>
#include <string>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace history {

// Represents a history entry to be shown to the user, representing either
// a local or remote visit. A single entry can represent multiple visits,
// since only the most recent visit on a particular day is shown.
struct HistoryEntry {
  // Values indicating whether an entry represents only local visits, only
  // remote visits, or a mixture of both.
  enum EntryType { EMPTY_ENTRY = 0, LOCAL_ENTRY, REMOTE_ENTRY, COMBINED_ENTRY };

  HistoryEntry(EntryType type,
               const GURL& url,
               const base::string16& title,
               base::Time time,
               const std::string& client_id,
               bool is_search_result,
               const base::string16& snippet,
               bool blocked_visit);
  HistoryEntry();
  HistoryEntry(const HistoryEntry&);
  ~HistoryEntry();

  // Comparison function for sorting HistoryEntries from newest to oldest.
  static bool SortByTimeDescending(const HistoryEntry& entry1,
                                   const HistoryEntry& entry2);

  // The type of visits this entry represents: local, remote, or both.
  EntryType entry_type;

  // URL of the entry.
  GURL url;

  // Title of the entry. May be empty.
  base::string16 title;

  // Time of the entry. Usually this will be the time of the most recent
  // visit to |url| on a particular day as defined in the local timezone.
  base::Time time;

  // Sync ID of the client on which the most recent visit occurred.
  std::string client_id;

  // Timestamps of all local or remote visits to the same URL on the same day.
  std::set<int64_t> all_timestamps;

  // If true, this entry is a history query result.
  bool is_search_result;

  // The entry's search snippet, if this entry is a history query result.
  base::string16 snippet;

  // Whether this entry was blocked when it was attempted.
  bool blocked_visit;
};
}  // namespace history

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_ENTRY_H_
