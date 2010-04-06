// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/sync_customize_controller.h"
#import "chrome/browser/cocoa/sync_customize_controller_cppsafe.h"

#import <Cocoa/Cocoa.h>

#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;

class SyncCustomizeControllerTest : public CocoaTest {
 protected:
  virtual void SetUp() {
    CocoaTest::SetUp();
    syncable::ModelTypeSet initial_registered_types, initial_preferred_types;
    initial_registered_types.insert(syncable::BOOKMARKS);
    initial_registered_types.insert(syncable::PREFERENCES);
    initial_registered_types.insert(syncable::THEMES);
    initial_preferred_types.insert(syncable::PREFERENCES);
    EXPECT_CALL(sync_service_mock_, GetRegisteredDataTypes(_))
        .WillOnce(SetArgumentPointee<0>(initial_registered_types));
    EXPECT_CALL(sync_service_mock_, GetPreferredDataTypes(_))
        .WillOnce(SetArgumentPointee<0>(initial_preferred_types));
  }

  virtual SyncCustomizeController* MakeSyncCustomizeController() {
    return [[SyncCustomizeController alloc]
            initWithProfileSyncService:&sync_service_mock_];
  }

  ProfileSyncServiceMock sync_service_mock_;
};

TEST_F(SyncCustomizeControllerTest, InitialRead) {
  SyncCustomizeController* controller = MakeSyncCustomizeController();
  // Force nib load.
  [controller window];

  EXPECT_TRUE([controller bookmarksRegistered]);
  EXPECT_TRUE([controller preferencesRegistered]);
  EXPECT_FALSE([controller autofillRegistered]);
  EXPECT_TRUE([controller themesRegistered]);

  EXPECT_FALSE([controller bookmarksPreferred]);
  EXPECT_TRUE([controller preferencesPreferred]);
  EXPECT_FALSE([controller autofillPreferred]);
  EXPECT_FALSE([controller themesPreferred]);

  [controller close];
}

TEST_F(SyncCustomizeControllerTest, RunAsModalSheet) {
  SyncCustomizeController* controller = MakeSyncCustomizeController();
  [controller runAsModalSheet:test_window()];
  [controller endSheetWithCancel:nil];
}

TEST_F(SyncCustomizeControllerTest, ShowSyncCustomizeDialog) {
  ShowSyncCustomizeDialog(test_window(), &sync_service_mock_);
  id controller = [[test_window() attachedSheet] windowController];
  EXPECT_TRUE([controller isKindOfClass:[SyncCustomizeController class]]);
  [controller endSheetWithCancel:nil];
}

TEST_F(SyncCustomizeControllerTest, EndSheetWithCancel) {
  EXPECT_CALL(sync_service_mock_, ChangePreferredDataTypes(_)).Times(0);

  SyncCustomizeController* controller = MakeSyncCustomizeController();
  [controller runAsModalSheet:test_window()];
  [controller setPreferencesPreferred:NO];
  [controller setThemesPreferred:YES];
  [controller endSheetWithCancel:nil];

  // If ChangePreferredDataTypes() wasn't called, then that means the
  // changes weren't committed.
}

TEST_F(SyncCustomizeControllerTest, EndSheetWithOK) {
  syncable::ModelTypeSet preferred_types;
  EXPECT_CALL(sync_service_mock_, ChangePreferredDataTypes(_))
      .WillOnce(SaveArg<0>(&preferred_types));

  SyncCustomizeController* controller = MakeSyncCustomizeController();
  [controller runAsModalSheet:test_window()];
  [controller setPreferencesPreferred:NO];
  [controller setThemesPreferred:YES];
  [controller endSheetWithOK:nil];

  syncable::ModelTypeSet expected_preferred_types;
  expected_preferred_types.insert(syncable::THEMES);
  EXPECT_TRUE(preferred_types == expected_preferred_types);
}

}  // namespace
