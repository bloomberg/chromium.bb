// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_harness.h"

#include "chrome/test/live_sync/live_sync_test.h"

#include "chrome/test/live_sync/bookmarks_helper.h"

class SyncErrorTest : public LiveSyncTest{
 public:
  SyncErrorTest() : LiveSyncTest(SINGLE_CLIENT) {
  }
  ~SyncErrorTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncErrorTest);

};

IN_PROC_BROWSER_TEST_F(SyncErrorTest, BirthdayErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = BookmarksHelper::AddFolder(0, 0, L"title1");
  BookmarksHelper::SetTitle(0, node1, L"new_title1");
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Offline state change."));
  TriggerBirthdayError();

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = BookmarksHelper::AddFolder(0, 0, L"title2");
  BookmarksHelper::SetTitle(0, node2, L"new_title2");
  ASSERT_TRUE(GetClient(0)->AwaitSyncDisabled("Birthday error."));
}
