// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_status_ui_helper_mac.h"

#import <Cocoa/Cocoa.h>

#include "app/l10n_util_mac.h"
#include "base/scoped_nsobject.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SyncStatusUIHelperMacTest : public CocoaTest {
};

TEST_F(SyncStatusUIHelperMacTest, UpdateSyncItem) {
  scoped_nsobject<NSMenuItem> syncMenuItem(
      [[NSMenuItem alloc] initWithTitle:@""
                                 action:@selector(commandDispatch)
                          keyEquivalent:@""]);
  [syncMenuItem setTag:IDC_SYNC_BOOKMARKS];

  NSString* bookmarksSynced =
    l10n_util::GetNSStringWithFixup(IDS_SYNC_MENU_BOOKMARKS_SYNCED_LABEL);
  NSString* bookmarkSyncError =
    l10n_util::GetNSStringWithFixup(IDS_SYNC_MENU_BOOKMARK_SYNC_ERROR_LABEL);
  NSString* startSync =
    l10n_util::GetNSStringWithFixup(IDS_SYNC_START_SYNC_BUTTON_LABEL);

  [syncMenuItem setTitle:@""];
  [syncMenuItem setHidden:NO];

  browser_sync::UpdateSyncItemForStatus(syncMenuItem, NO,
                                        SyncStatusUIHelper::PRE_SYNCED);
  EXPECT_TRUE([[syncMenuItem title] isEqualTo:startSync]);
  EXPECT_TRUE([syncMenuItem isHidden]);

  [syncMenuItem setTitle:@""];
  [syncMenuItem setHidden:YES];
  browser_sync::UpdateSyncItemForStatus(syncMenuItem, YES,
                                        SyncStatusUIHelper::SYNC_ERROR);
  EXPECT_TRUE([[syncMenuItem title] isEqualTo:bookmarkSyncError]);
  EXPECT_FALSE([syncMenuItem isHidden]);

  [syncMenuItem setTitle:@""];
  [syncMenuItem setHidden:NO];
  browser_sync::UpdateSyncItemForStatus(syncMenuItem, NO,
                                        SyncStatusUIHelper::SYNCED);
  EXPECT_TRUE([[syncMenuItem title] isEqualTo:bookmarksSynced]);
  EXPECT_TRUE([syncMenuItem isHidden]);
}

TEST_F(SyncStatusUIHelperMacTest, UpdateSyncItemWithSeparator) {
  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  NSMenuItem* syncMenuItem =
      [menu addItemWithTitle:@""
                      action:@selector(commandDispatch)
               keyEquivalent:@""];
  [syncMenuItem setTag:IDC_SYNC_BOOKMARKS];
  NSMenuItem* followingSeparator = [NSMenuItem separatorItem];
  [menu addItem:followingSeparator];

  const SyncStatusUIHelper::MessageType kStatus =
    SyncStatusUIHelper::PRE_SYNCED;

  [syncMenuItem setHidden:NO];
  [followingSeparator setHidden:NO];
  browser_sync::UpdateSyncItemForStatus(syncMenuItem, NO, kStatus);
  EXPECT_FALSE([followingSeparator isEnabled]);
  EXPECT_TRUE([syncMenuItem isHidden]);
  EXPECT_TRUE([followingSeparator isHidden]);

  [syncMenuItem setHidden:YES];
  [followingSeparator setHidden:YES];
  browser_sync::UpdateSyncItemForStatus(syncMenuItem, YES, kStatus);
  EXPECT_FALSE([followingSeparator isEnabled]);
  EXPECT_FALSE([syncMenuItem isHidden]);
  EXPECT_FALSE([followingSeparator isHidden]);
}

TEST_F(SyncStatusUIHelperMacTest, UpdateSyncItemWithNonSeparator) {
  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  NSMenuItem* syncMenuItem =
      [menu addItemWithTitle:@""
                      action:@selector(commandDispatch)
               keyEquivalent:@""];
  [syncMenuItem setTag:IDC_SYNC_BOOKMARKS];
  NSMenuItem* followingNonSeparator =
      [menu addItemWithTitle:@""
                      action:@selector(commandDispatch)
               keyEquivalent:@""];

  const SyncStatusUIHelper::MessageType kStatus =
    SyncStatusUIHelper::PRE_SYNCED;

  browser_sync::UpdateSyncItemForStatus(syncMenuItem, NO, kStatus);
  EXPECT_TRUE([followingNonSeparator isEnabled]);
  EXPECT_FALSE([followingNonSeparator isHidden]);

  browser_sync::UpdateSyncItemForStatus(syncMenuItem, YES, kStatus);
  EXPECT_TRUE([followingNonSeparator isEnabled]);
  EXPECT_FALSE([followingNonSeparator isHidden]);
}

}  // namespace
