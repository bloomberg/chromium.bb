// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_session.h"

#include "base/stl_util.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/common/url_constants.h"

namespace browser_sync {

SyncedSession::SyncedSession() : session_tag("invalid"),
                                 device_type(TYPE_UNSET) {
}

SyncedSession::~SyncedSession() {
  STLDeleteContainerPairSecondPointers(windows.begin(), windows.end());
}

// Note: if you modify this, make sure you modify IsValidTab in
// SessionModelAssociator.
bool IsValidSessionTab(const SessionTab& tab) {
  if (tab.navigations.empty())
    return false;
  int selected_index = tab.current_navigation_index;
  selected_index = std::max(
      0,
      std::min(selected_index,
          static_cast<int>(tab.navigations.size() - 1)));
  if (selected_index == 0 &&
      tab.navigations.size() == 1 &&
      tab.navigations.at(selected_index).virtual_url().GetOrigin() ==
          GURL(chrome::kChromeUINewTabURL)) {
    // This is a new tab with no further history, skip.
    return false;
  }
  return true;
}

bool SessionWindowHasNoTabsToSync(const SessionWindow& window) {
  int num_populated = 0;
  for (std::vector<SessionTab*>::const_iterator i = window.tabs.begin();
      i != window.tabs.end(); ++i) {
    const SessionTab* tab = *i;
    if (IsValidSessionTab(*tab))
      num_populated++;
  }
  return (num_populated == 0);
}

}  // namespace browser_sync
