// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_tab_delegate.h"

#include "chrome/browser/sync/glue/synced_window_delegate.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "url/gurl.h"

using browser_sync::SyncedTabDelegate;

namespace browser_sync {

content::NavigationEntry* SyncedTabDelegate::GetCurrentEntryMaybePending()
    const {
  return GetEntryAtIndexMaybePending(GetCurrentEntryIndex());
}

content::NavigationEntry* SyncedTabDelegate::GetEntryAtIndexMaybePending(
    int i) const {
  return (GetPendingEntryIndex() == i) ? GetPendingEntry() : GetEntryAtIndex(i);
}

bool SyncedTabDelegate::ShouldSync() const {
  if (GetSyncedWindowDelegate() == NULL)
    return false;

  // Is there a valid NavigationEntry?
  if (ProfileIsSupervised() && GetBlockedNavigations()->size() > 0)
    return true;

  int entry_count = GetEntryCount();
  if (entry_count == 0)
    return false;  // This deliberately ignores a new pending entry.

  bool found_valid_url = false;
  for (int i = 0; i < entry_count; ++i) {
    const content::NavigationEntry* entry = GetEntryAtIndexMaybePending(i);
    if (!entry) {
      return false;
    }
    const GURL& virtual_url = entry->GetVirtualURL();

    if (virtual_url.is_valid() &&
        !virtual_url.SchemeIs(content::kChromeUIScheme) &&
        !virtual_url.SchemeIs(chrome::kChromeNativeScheme) &&
        !virtual_url.SchemeIsFile()) {
      found_valid_url = true;
    } else if (virtual_url == GURL(chrome::kChromeUIHistoryURL)) {
      // The history page is treated specially as we want it to trigger syncable
      // events for UI purposes.
      found_valid_url = true;
    }
  }
  return found_valid_url;
}

const SyncedWindowDelegate* SyncedTabDelegate::GetSyncedWindowDelegate() const {
  return SyncedWindowDelegate::FindById(GetWindowId());
}

}  // namespace browser_sync
