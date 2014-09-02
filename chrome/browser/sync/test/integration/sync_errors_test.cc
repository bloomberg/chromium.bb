// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "sync/protocol/sync_protocol_error.h"

using bookmarks_helper::AddFolder;
using bookmarks_helper::SetTitle;
using sync_integration_test_util::AwaitCommitActivityCompletion;

namespace {

class SyncDisabledChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncDisabledChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  virtual bool IsExitConditionSatisfied() OVERRIDE {
    return !service()->setup_in_progress() &&
           !service()->HasSyncSetupCompleted();
  }

  virtual std::string GetDebugMessage() const OVERRIDE {
    return "Sync Disabled";
  }
};

bool AwaitSyncDisabled(ProfileSyncService* service) {
  SyncDisabledChecker checker(service);
  checker.Wait();
  return !checker.TimedOut();
}

class SyncErrorTest : public SyncTest {
 public:
  SyncErrorTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SyncErrorTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncErrorTest);
};

// TODO(pvalenzuela): Remove this class when all tests here are converted to
// use FakeServer.
class LegacySyncErrorTest : public SyncTest {
 public:
  LegacySyncErrorTest() : SyncTest(SINGLE_CLIENT_LEGACY) {}
  virtual ~LegacySyncErrorTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LegacySyncErrorTest);
};

// Helper class that waits until the sync engine has hit an actionable error.
class ActionableErrorChecker : public SingleClientStatusChangeChecker {
 public:
  explicit ActionableErrorChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  virtual ~ActionableErrorChecker() {}

  // Checks if an actionable error has been hit. Called repeatedly each time PSS
  // notifies observers of a state change.
  virtual bool IsExitConditionSatisfied() OVERRIDE {
    ProfileSyncService::Status status;
    service()->QueryDetailedSyncStatus(&status);
    return (status.sync_protocol_error.action != syncer::UNKNOWN_ACTION &&
            service()->HasUnrecoverableError());
  }

  virtual std::string GetDebugMessage() const OVERRIDE {
    return "ActionableErrorChecker";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ActionableErrorChecker);
};

IN_PROC_BROWSER_TEST_F(SyncErrorTest, BirthdayErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an item, wait for sync, and trigger a birthday error on the server.
  const BookmarkNode* node1 = AddFolder(0, 0, "title1");
  SetTitle(0, node1, "new_title1");
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(GetFakeServer()->SetNewStoreBirthday("new store birthday"));

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, 0, "title2");
  SetTitle(0, node2, "new_title2");
  ASSERT_TRUE(AwaitSyncDisabled(GetSyncService((0))));
}

IN_PROC_BROWSER_TEST_F(LegacySyncErrorTest, ActionableErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = AddFolder(0, 0, "title1");
  SetTitle(0, node1, "new_title1");
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));

  syncer::SyncProtocolError protocol_error;
  protocol_error.error_type = syncer::TRANSIENT_ERROR;
  protocol_error.action = syncer::UPGRADE_CLIENT;
  protocol_error.error_description = "Not My Fault";
  protocol_error.url = "www.google.com";
  TriggerSyncError(protocol_error, SyncTest::ERROR_FREQUENCY_ALWAYS);

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, 0, "title2");
  SetTitle(0, node2, "new_title2");

  // Wait until an actionable error is encountered.
  ActionableErrorChecker actionable_error_checker(GetSyncService((0)));
  actionable_error_checker.Wait();
  ASSERT_FALSE(actionable_error_checker.TimedOut());

  ProfileSyncService::Status status;
  GetSyncService((0))->QueryDetailedSyncStatus(&status);
  ASSERT_EQ(status.sync_protocol_error.error_type, protocol_error.error_type);
  ASSERT_EQ(status.sync_protocol_error.action, protocol_error.action);
  ASSERT_EQ(status.sync_protocol_error.url, protocol_error.url);
  ASSERT_EQ(status.sync_protocol_error.error_description,
      protocol_error.error_description);
}

// Disabled, http://crbug.com/351160 .
IN_PROC_BROWSER_TEST_F(LegacySyncErrorTest, DISABLED_ErrorWhileSettingUp) {
  ASSERT_TRUE(SetupClients());

  syncer::SyncProtocolError protocol_error;
  protocol_error.error_type = syncer::TRANSIENT_ERROR;
  protocol_error.error_description = "Not My Fault";
  protocol_error.url = "www.google.com";

  if (GetSyncService(0)->auto_start_enabled()) {
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
    ASSERT_TRUE(GetClient(0)->DisableSyncForDatatype(syncer::AUTOFILL));

    // Trigger error on every 2 out of 3 requests.
    TriggerSyncError(protocol_error, SyncTest::ERROR_FREQUENCY_TWO_THIRDS);

    // Now enable a datatype, whose first 2 syncs would fail, but we should
    // recover and setup succesfully on the third attempt.
    ASSERT_TRUE(GetClient(0)->EnableSyncForDatatype(syncer::AUTOFILL));
  }
}

IN_PROC_BROWSER_TEST_F(LegacySyncErrorTest,
    DISABLED_BirthdayErrorUsingActionableErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = AddFolder(0, 0, "title1");
  SetTitle(0, node1, "new_title1");
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));

  syncer::SyncProtocolError protocol_error;
  protocol_error.error_type = syncer::NOT_MY_BIRTHDAY;
  protocol_error.action = syncer::DISABLE_SYNC_ON_CLIENT;
  protocol_error.error_description = "Not My Fault";
  protocol_error.url = "www.google.com";
  TriggerSyncError(protocol_error, SyncTest::ERROR_FREQUENCY_ALWAYS);

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, 0, "title2");
  SetTitle(0, node2, "new_title2");
  ASSERT_TRUE(AwaitSyncDisabled(GetSyncService((0))));
  ProfileSyncService::Status status;
  GetSyncService((0))->QueryDetailedSyncStatus(&status);
  ASSERT_EQ(status.sync_protocol_error.error_type, protocol_error.error_type);
  ASSERT_EQ(status.sync_protocol_error.action, protocol_error.action);
  ASSERT_EQ(status.sync_protocol_error.url, protocol_error.url);
  ASSERT_EQ(status.sync_protocol_error.error_description,
      protocol_error.error_description);
}

IN_PROC_BROWSER_TEST_F(LegacySyncErrorTest, DisableDatatypeWhileRunning) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  syncer::ModelTypeSet synced_datatypes =
      GetSyncService((0))->GetActiveDataTypes();
  ASSERT_TRUE(synced_datatypes.Has(syncer::TYPED_URLS));
  ASSERT_TRUE(synced_datatypes.Has(syncer::SESSIONS));
  GetProfile(0)->GetPrefs()->SetBoolean(
      prefs::kSavingBrowserHistoryDisabled, true);

  // Flush any tasks posted by observers of the pref change.
  base::RunLoop().RunUntilIdle();

  synced_datatypes = GetSyncService((0))->GetActiveDataTypes();
  ASSERT_FALSE(synced_datatypes.Has(syncer::TYPED_URLS));
  ASSERT_FALSE(synced_datatypes.Has(syncer::SESSIONS));

  const BookmarkNode* node1 = AddFolder(0, 0, "title1");
  SetTitle(0, node1, "new_title1");
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  // TODO(lipalani)" Verify initial sync ended for typed url is false.
}

}  // namespace
