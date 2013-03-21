// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "sync/protocol/sync_protocol_error.h"

using bookmarks_helper::AddFolder;
using bookmarks_helper::SetTitle;

class SyncErrorTest : public SyncTest {
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
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Offline state change."));
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
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Offline state change."));
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
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Sync."));

  syncer::SyncProtocolError protocol_error;
  protocol_error.error_type = syncer::TRANSIENT_ERROR;
  protocol_error.action = syncer::UPGRADE_CLIENT;
  protocol_error.error_description = "Not My Fault";
  protocol_error.url = "www.google.com";
  TriggerSyncError(protocol_error, SyncTest::ERROR_FREQUENCY_ALWAYS);

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

IN_PROC_BROWSER_TEST_F(SyncErrorTest, ErrorWhileSettingUp) {
  ASSERT_TRUE(SetupClients());

  syncer::SyncProtocolError protocol_error;
  protocol_error.error_type = syncer::TRANSIENT_ERROR;
  protocol_error.error_description = "Not My Fault";
  protocol_error.url = "www.google.com";

  if (clients()[0]->AutoStartEnabled()) {
    // In auto start enabled platforms like chrome os we should be
    // able to set up even if the first sync while setting up fails.
    // Trigger error on every 2 out of 3 requests.
    TriggerSyncError(protocol_error, SyncTest::ERROR_FREQUENCY_TWO_THIRDS);
    // Now setup sync and it should succeed.
    ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  } else {
    // In Non auto start enabled environments if the setup sync fails then
    // the setup would fail. So setup sync normally.
    ASSERT_TRUE(SetupSync()) << "Setup sync failed";
    ASSERT_TRUE(clients()[0]->DisableSyncForDatatype(syncer::AUTOFILL));

    // Trigger error on every 2 out of 3 requests.
    TriggerSyncError(protocol_error, SyncTest::ERROR_FREQUENCY_TWO_THIRDS);

    // Now enable a datatype, whose first 2 syncs would fail, but we should
    // recover and setup succesfully on the third attempt.
    ASSERT_TRUE(clients()[0]->EnableSyncForDatatype(syncer::AUTOFILL));
  }
}


IN_PROC_BROWSER_TEST_F(SyncErrorTest,
    BirthdayErrorUsingActionableErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = AddFolder(0, 0, L"title1");
  SetTitle(0, node1, L"new_title1");
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Sync."));

  syncer::SyncProtocolError protocol_error;
  protocol_error.error_type = syncer::NOT_MY_BIRTHDAY;
  protocol_error.action = syncer::DISABLE_SYNC_ON_CLIENT;
  protocol_error.error_description = "Not My Fault";
  protocol_error.url = "www.google.com";
  TriggerSyncError(protocol_error, SyncTest::ERROR_FREQUENCY_ALWAYS);

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

// Trigger an auth error and make sure the sync client detects it when
// trying to commit.
IN_PROC_BROWSER_TEST_F(SyncErrorTest, AuthErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = AddFolder(0, 0, L"title1");
  SetTitle(0, node1, L"new_title1");
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Sync."));

  TriggerAuthError();

  const BookmarkNode* node2 = AddFolder(0, 0, L"title2");
  SetTitle(0, node2, L"new_title2");
  ASSERT_TRUE(GetClient(0)->AwaitExponentialBackoffVerification());
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            GetClient(0)->service()->GetAuthError().state());
}

// Trigger an XMPP auth error, and make sure sync treats it like any
// other auth error.
IN_PROC_BROWSER_TEST_F(SyncErrorTest, XmppAuthErrorTest) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  TriggerXmppAuthError();

  ASSERT_FALSE(GetClient(0)->SetupSync());

  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            GetClient(0)->service()->GetAuthError().state());
}

// TODO(lipalani): Fix the typed_url dtc so this test case can pass.
IN_PROC_BROWSER_TEST_F(SyncErrorTest, DISABLED_DisableDatatypeWhileRunning) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  syncer::ModelTypeSet synced_datatypes =
      GetClient(0)->service()->GetPreferredDataTypes();
  ASSERT_TRUE(synced_datatypes.Has(syncer::TYPED_URLS));
  GetProfile(0)->GetPrefs()->SetBoolean(
      prefs::kSavingBrowserHistoryDisabled, true);

  synced_datatypes = GetClient(0)->service()->GetPreferredDataTypes();
  ASSERT_FALSE(synced_datatypes.Has(syncer::TYPED_URLS));

  const BookmarkNode* node1 = AddFolder(0, 0, L"title1");
  SetTitle(0, node1, L"new_title1");
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Sync."));
  // TODO(lipalani)" Verify initial sync ended for typed url is false.
}

