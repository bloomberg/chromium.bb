// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/protocol/sync_protocol_error.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using bookmarks_helper::AddFolder;
using bookmarks_helper::SetTitle;

class SyncErrorTest : public SyncTest{
 public:
  SyncErrorTest() : SyncTest(SINGLE_CLIENT) {}
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

IN_PROC_BROWSER_TEST_F(SyncErrorTest, ActionableErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = AddFolder(0, 0, L"title1");
  SetTitle(0, node1, L"new_title1");
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Sync."));

  browser_sync::SyncProtocolError protocol_error;
  protocol_error.error_type = browser_sync::TRANSIENT_ERROR;
  protocol_error.action = browser_sync::UPGRADE_CLIENT;
  protocol_error.error_description = "Not My Fault";
  protocol_error.url = "www.google.com";
  TriggerSyncError(protocol_error);

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, 0, L"title2");
  SetTitle(0, node2, L"new_title2");
  ASSERT_TRUE(
      GetClient(0)->AwaitActionableError());
  ProfileSyncService::Status status = GetClient(0)->GetStatus();
  ASSERT_EQ(status.sync_protocol_error.error_type, protocol_error.error_type);
  ASSERT_EQ(status.sync_protocol_error.action, protocol_error.action);
  ASSERT_EQ(status.sync_protocol_error.url, protocol_error.url);
  ASSERT_EQ(status.sync_protocol_error.error_description,
      protocol_error.error_description);
}

IN_PROC_BROWSER_TEST_F(SyncErrorTest,
    BirthdayErrorUsingActionableErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = AddFolder(0, 0, L"title1");
  SetTitle(0, node1, L"new_title1");
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Sync."));

  browser_sync::SyncProtocolError protocol_error;
  protocol_error.error_type = browser_sync::NOT_MY_BIRTHDAY;
  protocol_error.action = browser_sync::DISABLE_SYNC_ON_CLIENT;
  protocol_error.error_description = "Not My Fault";
  protocol_error.url = "www.google.com";
  TriggerSyncError(protocol_error);

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, 0, L"title2");
  SetTitle(0, node2, L"new_title2");
  ASSERT_TRUE(
      GetClient(0)->AwaitSyncDisabled("Birthday Error."));
  ProfileSyncService::Status status = GetClient(0)->GetStatus();
  ASSERT_EQ(status.sync_protocol_error.error_type, protocol_error.error_type);
  ASSERT_EQ(status.sync_protocol_error.action, protocol_error.action);
  ASSERT_EQ(status.sync_protocol_error.url, protocol_error.url);
  ASSERT_EQ(status.sync_protocol_error.error_description,
      protocol_error.error_description);
}
