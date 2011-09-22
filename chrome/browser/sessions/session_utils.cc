// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_utils.h"

#include <set>

#include "chrome/browser/sessions/tab_restore_service.h"

namespace {

// This class is used to compare two entries and considers that they are
// identical if they share the same URL. See FilteredEntries() below for how
// this is used.
class EntryComparator {
 public:
  bool operator() (const TabRestoreService::Entry* lhs,
                   const TabRestoreService::Entry* rhs) {
    // Two entries are deemed identical when their visible title and URL are
    // the same.
    if (lhs->type == TabRestoreService::WINDOW ||
        rhs->type == TabRestoreService::WINDOW)
      return false;

    DCHECK(lhs->type == TabRestoreService::TAB);
    DCHECK(rhs->type == TabRestoreService::TAB);

    const TabRestoreService::Tab* rh_tab =
    static_cast<const TabRestoreService::Tab*>(rhs);
    const TabRestoreService::Tab* lh_tab =
    static_cast<const TabRestoreService::Tab*>(lhs);

    const TabNavigation& rh_entry =
    rh_tab->navigations[rh_tab->current_navigation_index];
    const TabNavigation& lh_entry =
    lh_tab->navigations[lh_tab->current_navigation_index];

    if (rh_entry.title() == lh_entry.title())
      return rh_entry.virtual_url().spec() < lh_entry.virtual_url().spec();

    return rh_entry.title() < lh_entry.title();
  }
};
}  // namespace

void SessionUtils::FilteredEntries(const TabRestoreService::Entries& entries,
    TabRestoreService::Entries* filtered_entries) {
  // A set to remember the entries we already seen.
  std::set<TabRestoreService::Entry*, EntryComparator> uniquing;

  for (TabRestoreService::Entries::const_iterator iter = entries.begin();
       iter != entries.end(); ++iter) {
    TabRestoreService::Entry* entry = *iter;

    if (uniquing.insert(entry).second)
      // This entry was not seen before, add it to the list.
      filtered_entries->push_back(entry);
  }
}

