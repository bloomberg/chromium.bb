// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace sync_ui_util {

void UpdateSyncItem(id syncItem, BOOL syncEnabled, Profile* profile) {
  ProfileSyncService* syncService =
    profile->GetOriginalProfile()->GetProfileSyncService();
  UpdateSyncItemForStatus(syncItem, syncEnabled,
                          sync_ui_util::GetStatus(syncService));
}

void UpdateSyncItemForStatus(id syncItem, BOOL syncEnabled,
                             sync_ui_util::MessageType status) {
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
  // chrome/browser/ui/views/toolbar_view.cc.
  int titleId;
  switch (status) {
    case sync_ui_util::SYNCED:
      titleId = IDS_SYNC_MENU_SYNCED_LABEL;
      break;
    case sync_ui_util::SYNC_ERROR:
      titleId = IDS_SYNC_MENU_SYNC_ERROR_LABEL;
      break;
    case sync_ui_util::PRE_SYNCED:
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

}  // namespace sync_ui_util
