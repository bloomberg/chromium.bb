// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/live_sync/bookmarks_helper.h"
#include "chrome/browser/sync/test/live_sync/live_sync_test.h"

using bookmarks_helper::AddFolder;
using bookmarks_helper::SetTitle;

class SyncErrorTest : public LiveSyncTest{
 public:
  SyncErrorTest() : LiveSyncTest(SINGLE_CLIENT) {}
  virtual ~SyncErrorTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncErrorTest);
};

IN_PROC_BROWSER_TEST_F(SyncErrorTest, BirthdayErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = AddFolder(0, 0, L"title1");
  SetTitle(0, node1, L"new_title1");
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Offline state change."));
  TriggerBirthdayError();

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, 0, L"title2");
  SetTitle(0, node2, L"new_title2");
  ASSERT_TRUE(GetClient(0)->AwaitSyncDisabled("Birthday error."));
}

IN_PROC_BROWSER_TEST_F(SyncErrorTest, TransientErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = AddFolder(0, 0, L"title1");
  SetTitle(0, node1, L"new_title1");
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Offline state change."));
  TriggerTransientError();

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, 0, L"title2");
  SetTitle(0, node2, L"new_title2");
  ASSERT_TRUE(
      GetClient(0)->AwaitExponentialBackoffVerification());
}
