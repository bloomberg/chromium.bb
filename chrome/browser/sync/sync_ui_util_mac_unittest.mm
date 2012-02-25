// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

class SyncStatusUIHelperMacTest : public CocoaTest {
};

TEST_F(SyncStatusUIHelperMacTest, UpdateSyncItem) {
  scoped_nsobject<NSMenuItem> syncMenuItem(
      [[NSMenuItem alloc] initWithTitle:@""
                                 action:@selector(commandDispatch)
                          keyEquivalent:@""]);
  [syncMenuItem setTag:IDC_SHOW_SYNC_SETUP];

  std::string userName = "foo@example.com";

  NSString* bookmarksSynced =
    l10n_util::GetNSStringFWithFixup(IDS_SYNC_MENU_SYNCED_LABEL,
                                     UTF8ToUTF16(userName));
  NSString* bookmarkSyncError =
    l10n_util::GetNSStringWithFixup(IDS_SYNC_MENU_SYNC_ERROR_LABEL);
  NSString* startSync =
    l10n_util::GetNSStringFWithFixup(
        IDS_SYNC_MENU_PRE_SYNCED_LABEL,
        l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));

  [syncMenuItem setTitle:@""];
  [syncMenuItem setHidden:NO];

  sync_ui_util::UpdateSyncItemForStatus(syncMenuItem, NO,
                                        sync_ui_util::PRE_SYNCED,
                                        userName);
  EXPECT_TRUE([[syncMenuItem title] isEqualTo:startSync]);
  EXPECT_TRUE([syncMenuItem isHidden]);

  [syncMenuItem setTitle:@""];
  [syncMenuItem setHidden:YES];
  sync_ui_util::UpdateSyncItemForStatus(syncMenuItem, YES,
                                        sync_ui_util::SYNC_ERROR,
                                        userName);
  EXPECT_TRUE([[syncMenuItem title] isEqualTo:bookmarkSyncError]);
  EXPECT_FALSE([syncMenuItem isHidden]);

  [syncMenuItem setTitle:@""];
  [syncMenuItem setHidden:NO];
  sync_ui_util::UpdateSyncItemForStatus(syncMenuItem, NO,
                                        sync_ui_util::SYNCED,
                                        userName);
  EXPECT_TRUE([[syncMenuItem title] isEqualTo:bookmarksSynced]);
  EXPECT_TRUE([syncMenuItem isHidden]);
}

TEST_F(SyncStatusUIHelperMacTest, UpdateSyncItemWithSeparator) {
  scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  NSMenuItem* syncMenuItem =
      [menu addItemWithTitle:@""
                      action:@selector(commandDispatch)
               keyEquivalent:@""];
  [syncMenuItem setTag:IDC_SHOW_SYNC_SETUP];
  NSMenuItem* followingSeparator = [NSMenuItem separatorItem];
  [menu addItem:followingSeparator];

  const sync_ui_util::MessageType kStatus = sync_ui_util::PRE_SYNCED;

  [syncMenuItem setHidden:NO];
  [followingSeparator setHidden:NO];
  sync_ui_util::UpdateSyncItemForStatus(syncMenuItem, NO, kStatus, "");
  EXPECT_FALSE([followingSeparator isEnabled]);
  EXPECT_TRUE([syncMenuItem isHidden]);
  EXPECT_TRUE([followingSeparator isHidden]);

  [syncMenuItem setHidden:YES];
  [followingSeparator setHidden:YES];
  sync_ui_util::UpdateSyncItemForStatus(syncMenuItem, YES, kStatus, "");
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
  [syncMenuItem setTag:IDC_SHOW_SYNC_SETUP];
  NSMenuItem* followingNonSeparator =
      [menu addItemWithTitle:@""
                      action:@selector(commandDispatch)
               keyEquivalent:@""];

  const sync_ui_util::MessageType kStatus = sync_ui_util::PRE_SYNCED;

  sync_ui_util::UpdateSyncItemForStatus(syncMenuItem, NO, kStatus, "");
  EXPECT_TRUE([followingNonSeparator isEnabled]);
  EXPECT_FALSE([followingNonSeparator isHidden]);

  sync_ui_util::UpdateSyncItemForStatus(syncMenuItem, YES, kStatus, "");
  EXPECT_TRUE([followingNonSeparator isEnabled]);
  EXPECT_FALSE([followingNonSeparator isHidden]);
}

}  // namespace
