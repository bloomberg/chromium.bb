// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_session_util.h"

#include "chrome/common/url_constants.h"
#include "components/sessions/session_types.h"
#include "content/public/common/url_constants.h"

namespace {

// Controls which foreign tabs we're interested in syncing/displaying. Checks
// that the tab has navigations and contains at least one valid url.
// Note: chrome:// and file:// are not considered valid urls (for syncing).
bool ShouldSyncSessionTab(const sessions::SessionTab& tab) {
  if (tab.navigations.empty())
    return false;
  bool found_valid_url = false;
  for (size_t i = 0; i < tab.navigations.size(); ++i) {
    const GURL& virtual_url = tab.navigations.at(i).virtual_url();
    if (virtual_url.is_valid() &&
        !virtual_url.SchemeIs(content::kChromeUIScheme) &&
        !virtual_url.SchemeIs(chrome::kChromeNativeScheme) &&
        !virtual_url.SchemeIsFile()) {
      found_valid_url = true;
      break;
    }
  }
  return found_valid_url;
}

}  // namespace

namespace browser_sync {

bool SessionWindowHasNoTabsToSync(const sessions::SessionWindow& window) {
  int num_populated = 0;
  for (auto i = window.tabs.begin(); i != window.tabs.end(); ++i) {
    const sessions::SessionTab* tab = *i;
    if (ShouldSyncSessionTab(*tab))
      num_populated++;
  }
  return (num_populated == 0);
}

}  // namespace browser_sync
