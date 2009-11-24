// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_status_ui_helper_mac.h"

#import <Cocoa/Cocoa.h>

#include "app/l10n_util_mac.h"
#include "base/logging.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/sync_status_ui_helper.h"
#include "grit/generated_resources.h"

namespace browser_sync {

void UpdateSyncItem(id syncItem, BOOL syncEnabled, Profile* profile) {
  ProfileSyncService* syncService =
    profile->GetOriginalProfile()->GetProfileSyncService();
  // TODO(timsteele): Need a ui helper method to just get the type
  // without needing labels.
  string16 label, link;
  SyncStatusUIHelper::MessageType status =
      SyncStatusUIHelper::GetLabels(syncService, &label, &link);
  UpdateSyncItemForStatus(syncItem, syncEnabled, status);
}

void UpdateSyncItemForStatus(id syncItem, BOOL syncEnabled,
                             SyncStatusUIHelper::MessageType status) {
  DCHECK([syncItem isKindOfClass:[NSMenuItem class]]);
  NSMenuItem* syncMenuItem = static_cast<NSMenuItem*>(syncItem);
  // Look for a separator immediately after the menu item.
  NSMenuItem* followingSeparator = nil;
  NSMenu* menu = [syncItem menu];
  if (menu) {
    NSInteger syncItemIndex = [menu indexOfItem:syncMenuItem];
    DCHECK_NE(syncItemIndex, -1);
    if ((syncItemIndex + 1) < [menu numberOfItems]) {
      NSMenuItem* menuItem = [menu itemAtIndex:(syncItemIndex + 1)];
      if ([menuItem isSeparatorItem]) {
        followingSeparator = menuItem;
      }
    }
  }

  // TODO(akalin): consolidate this code with the equivalent Windows code in
  // chrome/browser/views/toolbar_view.cc.
  int titleId;
  switch (status) {
    case SyncStatusUIHelper::SYNCED:
      titleId = IDS_SYNC_MENU_BOOKMARKS_SYNCED_LABEL;
      break;
    case SyncStatusUIHelper::SYNC_ERROR:
      titleId = IDS_SYNC_MENU_BOOKMARK_SYNC_ERROR_LABEL;
      break;
    case SyncStatusUIHelper::PRE_SYNCED:
      titleId = IDS_SYNC_START_SYNC_BUTTON_LABEL;
      break;
    default:
      NOTREACHED();
      // Needed to prevent release-mode warnings.
      titleId = IDS_SYNC_START_SYNC_BUTTON_LABEL;
      break;
  }
  NSString* title = l10n_util::GetNSStringWithFixup(titleId);
  [syncMenuItem setTitle:title];

  // If we don't have a sync service, hide any sync-related menu
  // items.  However, sync_menu_item is enabled/disabled outside of this
  // function so we don't touch it here, and separators are always disabled.
  [syncMenuItem setHidden:!syncEnabled];
  [followingSeparator setHidden:!syncEnabled];
}

}  // namespace browser_sync
