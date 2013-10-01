// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_session.h"

#include "base/stl_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/url_constants.h"

namespace browser_sync {

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
    const GURL& virtual_url = tab.navigations.at(i).virtual_url();
    if (virtual_url.is_valid() &&
        !virtual_url.SchemeIs(chrome::kChromeUIScheme) &&
        !virtual_url.SchemeIs(chrome::kChromeNativeScheme) &&
        !virtual_url.SchemeIsFile()) {
      found_valid_url = true;
      break;
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
