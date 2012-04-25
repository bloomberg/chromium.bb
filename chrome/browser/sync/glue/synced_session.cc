// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_session.h"

#include "base/stl_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"

namespace browser_sync {

SyncedTabNavigation::SyncedTabNavigation() : unique_id_(0) {
}

SyncedTabNavigation::SyncedTabNavigation(const SyncedTabNavigation& tab)
    : TabNavigation(tab),
      unique_id_(tab.unique_id_),
      timestamp_(tab.timestamp_) {
}

SyncedTabNavigation::SyncedTabNavigation(int index,
                                         const GURL& virtual_url,
                                         const content::Referrer& referrer,
                                         const string16& title,
                                         const std::string& state,
                                         content::PageTransition transition,
                                         int unique_id,
                                         const base::Time& timestamp)
    : TabNavigation(index, virtual_url, referrer, title, state, transition),
      unique_id_(unique_id),
      timestamp_(timestamp) {
}

SyncedTabNavigation::~SyncedTabNavigation() {
}

void SyncedTabNavigation::set_unique_id(int unique_id) {
  unique_id_ = unique_id;
}

int SyncedTabNavigation::unique_id() const {
  return unique_id_;
}

void SyncedTabNavigation::set_timestamp(const base::Time& timestamp) {
  timestamp_ = timestamp;
}
base::Time SyncedTabNavigation::timestamp() const {
  return timestamp_;
}

SyncedSessionTab::SyncedSessionTab() {}
SyncedSessionTab::~SyncedSessionTab() {}

SyncedSession::SyncedSession() : session_tag("invalid"),
                                 device_type(TYPE_UNSET) {
}

SyncedSession::~SyncedSession() {
  STLDeleteContainerPairSecondPointers(windows.begin(), windows.end());
}

// Note: if you modify this, make sure you modify
// SessionModelAssociator::ShouldSyncTab to ensure the logic matches.
bool ShouldSyncSessionTab(const SessionTab& tab) {
  if (tab.navigations.empty())
    return false;
  bool found_valid_url = false;
  for (size_t i = 0; i < tab.navigations.size(); ++i) {
    if (tab.navigations.at(i).virtual_url().is_valid() &&
        !tab.navigations.at(i).virtual_url().SchemeIs("chrome") &&
        !tab.navigations.at(i).virtual_url().SchemeIsFile()) {
      found_valid_url = true;
    }
  }
  return found_valid_url;
}

bool SessionWindowHasNoTabsToSync(const SessionWindow& window) {
  int num_populated = 0;
  for (std::vector<SessionTab*>::const_iterator i = window.tabs.begin();
      i != window.tabs.end(); ++i) {
    const SessionTab* tab = *i;
    if (ShouldSyncSessionTab(*tab))
      num_populated++;
  }
  return (num_populated == 0);
}

}  // namespace browser_sync
