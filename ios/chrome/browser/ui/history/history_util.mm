// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/history/history_util.h"

#include <map>

#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/history/history_entry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace history {

base::string16 GetRelativeDateLocalized(const base::Time& visit_time) {
  base::Time midnight = base::Time::Now().LocalMidnight();
  base::string16 date_str = ui::TimeFormat::RelativeDate(visit_time, &midnight);
  if (date_str.empty()) {
    date_str = base::TimeFormatFriendlyDate(visit_time);
  } else {
    date_str = l10n_util::GetStringFUTF16(
        IDS_HISTORY_DATE_WITH_RELATIVE_TIME, date_str,
        base::TimeFormatFriendlyDate(visit_time));
  }
  return date_str;
}

void MergeDuplicateHistoryEntries(std::vector<history::HistoryEntry>* entries) {
  std::vector<history::HistoryEntry> new_entries;
  // Pre-reserve the size of the new vector. Since we're working with pointers
  // later on not doing this could lead to the vector being resized and to
  // pointers to invalid locations.
  new_entries.reserve(entries->size());
  // Maps a URL to the most recent entry on a particular day.
  std::map<GURL, history::HistoryEntry*> current_day_entries;

  // Keeps track of the day that |current_day_urls| is holding the URLs for,
  // in order to handle removing per-day duplicates.
  base::Time current_day_midnight;

  std::sort(entries->begin(), entries->end(),
            history::HistoryEntry::SortByTimeDescending);

  for (const history::HistoryEntry& entry : *entries) {
    // Reset the list of found URLs when a visit from a new day is encountered.
    if (current_day_midnight != entry.time.LocalMidnight()) {
      current_day_entries.clear();
      current_day_midnight = entry.time.LocalMidnight();
    }

    // Keep this visit if it's the first visit to this URL on the current day.
    if (current_day_entries.count(entry.url) == 0) {
      new_entries.push_back(entry);
      current_day_entries[entry.url] = &new_entries.back();
    } else {
      // Keep track of the timestamps of all visits to the URL on the same day.
      history::HistoryEntry* combined_entry = current_day_entries[entry.url];
      combined_entry->all_timestamps.insert(entry.all_timestamps.begin(),
                                            entry.all_timestamps.end());

      if (combined_entry->entry_type != entry.entry_type) {
        combined_entry->entry_type = history::HistoryEntry::COMBINED_ENTRY;
      }
    }
  }
  entries->swap(new_entries);
}
}  // namespace history
