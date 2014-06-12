// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sessions_util.h"

#include "chrome/browser/sync/glue/synced_tab_delegate.h"
#include "chrome/browser/sync/glue/synced_window_delegate.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "url/gurl.h"

namespace browser_sync {

namespace sessions_util {

bool ShouldSyncTab(const SyncedTabDelegate& tab) {
  if (SyncedWindowDelegate::FindSyncedWindowDelegateWithId(
          tab.GetWindowId()) == NULL) {
    return false;
  }

  // Does the tab have a valid NavigationEntry?
  if (tab.ProfileIsSupervised() && tab.GetBlockedNavigations()->size() > 0)
    return true;

  int entry_count = tab.GetEntryCount();
  if (entry_count == 0)
    return false;  // This deliberately ignores a new pending entry.

  int pending_index = tab.GetPendingEntryIndex();
  bool found_valid_url = false;
  for (int i = 0; i < entry_count; ++i) {
    const content::NavigationEntry* entry = (i == pending_index) ?
       tab.GetPendingEntry() : tab.GetEntryAtIndex(i);
    if (!entry)
      return false;
    const GURL& virtual_url = entry->GetVirtualURL();
    if (virtual_url.is_valid() &&
        !virtual_url.SchemeIs(content::kChromeUIScheme) &&
        !virtual_url.SchemeIs(chrome::kChromeNativeScheme) &&
        !virtual_url.SchemeIsFile()) {
      found_valid_url = true;
    }
  }
  return found_valid_url;
}

bool ShouldSyncWindow(const SyncedWindowDelegate* window) {
  if (window->IsApp())
    return false;
  return window->IsTypeTabbed() || window->IsTypePopup();
}

}  // namespace sessions_util

}  // namespace browser_sync
