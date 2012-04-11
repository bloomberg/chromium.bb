// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/utf_string_conversions.h"
#include "base/logging.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetNSStringWithFixup;
using l10n_util::GetNSStringFWithFixup;

namespace sync_ui_util {

void UpdateSyncItem(id syncItem, BOOL syncEnabled, Profile* profile) {
  ProfileSyncService* syncService =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(
          profile->GetOriginalProfile());
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile);
  UpdateSyncItemForStatus(
      syncItem,
      syncEnabled,
      sync_ui_util::GetStatus(syncService, *signin),
      profile->GetPrefs()->GetString(prefs::kGoogleServicesUsername));
}

void UpdateSyncItemForStatus(id syncItem, BOOL syncEnabled,
                             sync_ui_util::MessageType status,
                             const std::string& userName) {
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
  NSString* title = NULL;
  switch (status) {
    case sync_ui_util::SYNCED:
      title = GetNSStringFWithFixup(IDS_SYNC_MENU_SYNCED_LABEL,
                                    UTF8ToUTF16(userName));
      break;
    case sync_ui_util::SYNC_ERROR:
      title = GetNSStringWithFixup(IDS_SYNC_MENU_SYNC_ERROR_LABEL);
      break;
    case sync_ui_util::PRE_SYNCED:
    case sync_ui_util::SYNC_PROMO:
    default:
      title = GetNSStringFWithFixup(IDS_SYNC_MENU_PRE_SYNCED_LABEL,
                                    GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
      break;
  }
  [syncMenuItem setTitle:title];

  // If we don't have a sync service, hide any sync-related menu
  // items.  However, sync_menu_item is enabled/disabled outside of this
  // function so we don't touch it here, and separators are always disabled.
  [syncMenuItem setHidden:!syncEnabled];
  [followingSeparator setHidden:!syncEnabled];
}

}  // namespace sync_ui_util
