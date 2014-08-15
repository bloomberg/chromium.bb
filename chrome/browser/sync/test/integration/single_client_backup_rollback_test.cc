// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "sync/internal_api/public/util/sync_db_util.h"
#include "sync/test/fake_server/fake_server_verifier.h"
#include "sync/util/time.h"

using bookmarks_helper::AddFolder;
using bookmarks_helper::AddURL;
using bookmarks_helper::GetOtherNode;
using bookmarks_helper::ModelMatchesVerifier;
using bookmarks_helper::Move;
using bookmarks_helper::Remove;
using sync_integration_test_util::AwaitCommitActivityCompletion;

namespace {
const char kUrl1[] = "http://www.google.com";
const char kUrl2[] = "http://map.google.com";
const char kUrl3[] = "http://plus.google.com";
}  // anonymous namespace

class SingleClientBackupRollbackTest : public SyncTest {
 public:
  SingleClientBackupRollbackTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientBackupRollbackTest() {}

  void DisableBackup() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kSyncDisableBackup);
  }

  void DisableRollback() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kSyncDisableRollback);
  }

  base::Time GetBackupDbLastModified() {
    base::RunLoop run_loop;

    base::Time backup_time;
    syncer::CheckSyncDbLastModifiedTime(
        GetProfile(0)->GetPath().Append(FILE_PATH_LITERAL("Sync Data Backup")),
        base::MessageLoopProxy::current(),
        base::Bind(&SingleClientBackupRollbackTest::CheckDbCallback,
                   base::Unretained(this), &backup_time));
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
    return backup_time;
  }

 private:
  void CheckDbCallback(base::Time* time_out, base::Time time_in) {
    *time_out = syncer::ProtoTimeToTime(syncer::TimeToProtoTime(time_in));
  }

  DISALLOW_COPY_AND_ASSIGN(SingleClientBackupRollbackTest);
};

class SyncBackendStoppedChecker {
 public:
  explicit SyncBackendStoppedChecker(ProfileSyncService* service,
                                     base::TimeDelta timeout)
      : pss_(service),
        timeout_(timeout) {}

  bool Wait() {
    expiration_ = base::TimeTicks::Now() + timeout_;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&SyncBackendStoppedChecker::PeriodicCheck,
                   base::Unretained(this)),
        base::TimeDelta::FromSeconds(1));
    base::MessageLoop::current()->Run();
    return ProfileSyncService::IDLE == pss_->backend_mode();
  }

 private:
  void PeriodicCheck() {
    if (ProfileSyncService::IDLE == pss_->backend_mode() ||
        base::TimeTicks::Now() > expiration_) {
      base::MessageLoop::current()->Quit();
    } else {
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&SyncBackendStoppedChecker::PeriodicCheck,
                     base::Unretained(this)),
          base::TimeDelta::FromSeconds(1));
    }
  }

  ProfileSyncService* pss_;
  base::TimeDelta timeout_;
  base::TimeTicks expiration_;
};

#if defined(ENABLE_PRE_SYNC_BACKUP)
#define MAYBE_TestBackup TestBackup
#else
#define MAYBE_TestBackup DISABLED_TestBackup
#endif
IN_PROC_BROWSER_TEST_F(SingleClientBackupRollbackTest,
                       MAYBE_TestBackup) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService(0)));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Verify backup DB is created and backup time is set on device info.
  base::Time backup_time = GetBackupDbLastModified();
  ASSERT_FALSE(backup_time.is_null());
  ASSERT_EQ(backup_time, GetSyncService(0)->GetDeviceBackupTimeForTesting());
}

#if defined(ENABLE_PRE_SYNC_BACKUP)
#define MAYBE_TestBackupDisabled TestBackupDisabled
#else
#define MAYBE_TestBackupDisabled DISABLED_TestBackupDisabled
#endif
IN_PROC_BROWSER_TEST_F(SingleClientBackupRollbackTest,
                       MAYBE_TestBackupDisabled) {
  DisableBackup();

  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService(0)));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Verify backup DB is not created and backup time is not set on device info.
  ASSERT_FALSE(base::PathExists(
      GetProfile(0)->GetPath().Append(FILE_PATH_LITERAL("Sync Data Backup"))));
  ASSERT_TRUE(GetSyncService(0)->GetDeviceBackupTimeForTesting().is_null());
}

#if defined(ENABLE_PRE_SYNC_BACKUP)
#define MAYBE_TestRollback TestRollback
#else
#define MAYBE_TestRollback DISABLED_TestRollback
#endif
IN_PROC_BROWSER_TEST_F(SingleClientBackupRollbackTest,
                       MAYBE_TestRollback) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Starting state:
  // other_node
  //    -> top
  //      -> tier1_a
  //        -> http://mail.google.com  "tier1_a_url0"
  //      -> tier1_b
  //        -> http://www.nhl.com "tier1_b_url0"
  const BookmarkNode* top = AddFolder(0, GetOtherNode(0), 0, "top");
  const BookmarkNode* tier1_a = AddFolder(0, top, 0, "tier1_a");
  const BookmarkNode* tier1_b = AddFolder(0, top, 1, "tier1_b");
  ASSERT_TRUE(AddURL(0, tier1_a, 0, "tier1_a_url0",
                     GURL("http://mail.google.com")));
  ASSERT_TRUE(AddURL(0, tier1_b, 0, "tier1_b_url0",
                     GURL("http://www.nhl.com")));

  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService(0)));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Made bookmark changes while sync is on.
  Move(0, tier1_a->GetChild(0), tier1_b, 1);
  Remove(0, tier1_b, 0);
  ASSERT_TRUE(AddFolder(0, tier1_b, 1, "tier2_c"));
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService(0)));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Let server to return rollback command on next sync request.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::USER_ROLLBACK);

  // Make another change to trigger downloading of rollback command.
  Remove(0, tier1_b, 0);

  // Wait for rollback to finish and sync backend is completely shut down.
  SyncBackendStoppedChecker checker(GetSyncService(0),
                                    base::TimeDelta::FromSeconds(5));
  ASSERT_TRUE(checker.Wait());

  // Verify bookmarks are restored.
  ASSERT_EQ(1, tier1_a->child_count());
  const BookmarkNode* url1 = tier1_a->GetChild(0);
  ASSERT_EQ(GURL("http://mail.google.com"), url1->url());

  ASSERT_EQ(1, tier1_b->child_count());
  const BookmarkNode* url2 = tier1_b->GetChild(0);
  ASSERT_EQ(GURL("http://www.nhl.com"), url2->url());

  // Backup DB should be deleted after rollback is done.
  ASSERT_FALSE(base::PathExists(
      GetProfile(0)->GetPath().Append(FILE_PATH_LITERAL("Sync Data Backup"))));
}

#if defined(ENABLE_PRE_SYNC_BACKUP)
#define MAYBE_TestRollbackDisabled TestRollbackDisabled
#else
#define MAYBE_TestRollbackDisabled DISABLED_TestRollbackDisabled
#endif
IN_PROC_BROWSER_TEST_F(SingleClientBackupRollbackTest,
                       MAYBE_TestRollbackDisabled) {
  DisableRollback();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Starting state:
  // other_node
  //    -> http://mail.google.com  "url0"
  //    -> http://www.nhl.com "url1"
  ASSERT_TRUE(AddURL(0, GetOtherNode(0), 0, "url0",
                     GURL("http://mail.google.com")));
  ASSERT_TRUE(AddURL(0, GetOtherNode(0), 1, "url1",
                     GURL("http://www.nhl.com")));

  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService(0)));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Made bookmark changes while sync is on.
  Remove(0, GetOtherNode(0), 1);
  ASSERT_TRUE(AddURL(0, GetOtherNode(0), 1, "url2",
                     GURL("http://www.yahoo.com")));
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Let server to return rollback command on next sync request.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::USER_ROLLBACK);

  // Make another change to trigger downloading of rollback command.
  Remove(0, GetOtherNode(0), 0);

  // Wait for rollback to finish and sync backend is completely shut down.
  SyncBackendStoppedChecker checker(GetSyncService(0),
                                    base::TimeDelta::FromSeconds(5));
  ASSERT_TRUE(checker.Wait());

  // With rollback disabled, bookmarks in backup DB should not be restored.
  // Only bookmark added during sync is present.
  ASSERT_EQ(1, GetOtherNode(0)->child_count());
  ASSERT_EQ(GURL("http://www.yahoo.com"),
            GetOtherNode(0)->GetChild(0)->url());

  // Backup DB should be deleted after.
  ASSERT_FALSE(base::PathExists(
      GetProfile(0)->GetPath().Append(FILE_PATH_LITERAL("Sync Data Backup"))));
}

#if defined(ENABLE_PRE_SYNC_BACKUP)
#define MAYBE_TestSyncDisabled TestSyncDisabled
#else
#define MAYBE_TestSyncDisabled DISABLED_TestSyncDisabled
#endif
IN_PROC_BROWSER_TEST_F(SingleClientBackupRollbackTest,
                       MAYBE_TestSyncDisabled) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Starting state:
  // other_node
  //    -> http://mail.google.com  "url0"
  //    -> http://www.nhl.com "url1"
  ASSERT_TRUE(AddURL(0, GetOtherNode(0), 0, "url0",
                     GURL("http://mail.google.com")));
  ASSERT_TRUE(AddURL(0, GetOtherNode(0), 1, "url1",
                     GURL("http://www.nhl.com")));

  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService(0)));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Made bookmark changes while sync is on.
  Remove(0, GetOtherNode(0), 1);
  ASSERT_TRUE(AddURL(0, GetOtherNode(0), 1, "url2",
                     GURL("http://www.yahoo.com")));
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Let server to return birthday error on next sync request.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::NOT_MY_BIRTHDAY);

  // Make another change to trigger downloading of rollback command.
  Remove(0, GetOtherNode(0), 0);

  // Wait for rollback to finish and sync backend is completely shut down.
  SyncBackendStoppedChecker checker(GetSyncService(0),
                                    base::TimeDelta::FromSeconds(5));
  ASSERT_TRUE(checker.Wait());

  // Shouldn't restore bookmarks with sign-out only.
  ASSERT_EQ(1, GetOtherNode(0)->child_count());
  ASSERT_EQ(GURL("http://www.yahoo.com"),
            GetOtherNode(0)->GetChild(0)->url());

  // Backup DB should be deleted after.
  ASSERT_FALSE(base::PathExists(
      GetProfile(0)->GetPath().Append(FILE_PATH_LITERAL("Sync Data Backup"))));
}

#if defined(ENABLE_PRE_SYNC_BACKUP)
#define MAYBE_RollbackNoBackup RollbackNoBackup
#else
#define MAYBE_RollbackNoBackup DISABLED_RollbackNoBackup
#endif
IN_PROC_BROWSER_TEST_F(SingleClientBackupRollbackTest,
                       MAYBE_RollbackNoBackup) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Starting state:
  // other_node
  //    -> http://mail.google.com  "url0"
  //    -> http://www.nhl.com "url1"
  ASSERT_TRUE(AddURL(0, GetOtherNode(0), 0, "url0",
                     GURL("http://mail.google.com")));

  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  ASSERT_TRUE(AddURL(0, GetOtherNode(0), 1, "url1",
                     GURL("http://www.nhl.com")));

  // Delete backup DB.
  base::DeleteFile(
      GetProfile(0)->GetPath().Append(FILE_PATH_LITERAL("Sync Data Backup")),
      true);

  // Let server to return rollback command on next sync request.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::USER_ROLLBACK);

  // Make another change to trigger downloading of rollback command.
  Remove(0, GetOtherNode(0), 0);

  // Wait for backend to completely shut down.
  SyncBackendStoppedChecker checker(GetSyncService(0),
                                    base::TimeDelta::FromSeconds(15));
  ASSERT_TRUE(checker.Wait());

  // Without backup DB, bookmarks remain at the state when sync stops.
  ASSERT_EQ(1, GetOtherNode(0)->child_count());
  ASSERT_EQ(GURL("http://www.nhl.com"),
            GetOtherNode(0)->GetChild(0)->url());
}

#if defined(ENABLE_PRE_SYNC_BACKUP)
#define MAYBE_DontChangeBookmarkOrdering DontChangeBookmarkOrdering
#else
#define MAYBE_DontChangeBookmarkOrdering DISABLED_DontChangeBookmarkOrdering
#endif
IN_PROC_BROWSER_TEST_F(SingleClientBackupRollbackTest,
                       MAYBE_DontChangeBookmarkOrdering) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const BookmarkNode* sub_folder = AddFolder(0, GetOtherNode(0), 0, "test");
  ASSERT_TRUE(AddURL(0, sub_folder, 0, "", GURL(kUrl1)));
  ASSERT_TRUE(AddURL(0, sub_folder, 1, "", GURL(kUrl2)));
  ASSERT_TRUE(AddURL(0, sub_folder, 2, "", GURL(kUrl3)));

  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService(0)));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Made bookmark changes while sync is on.
  Remove(0, sub_folder, 0);
  Remove(0, sub_folder, 0);
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService(0)));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Let server to return rollback command on next sync request.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::USER_ROLLBACK);

  // Make another change to trigger downloading of rollback command.
  Remove(0, sub_folder, 0);

  // Wait for rollback to finish and sync backend is completely shut down.
  SyncBackendStoppedChecker checker(GetSyncService(0),
                                    base::TimeDelta::FromSeconds(5));
  ASSERT_TRUE(checker.Wait());

  // Verify bookmarks are unchanged.
  ASSERT_EQ(3, sub_folder->child_count());
  ASSERT_EQ(GURL(kUrl1), sub_folder->GetChild(0)->url());
  ASSERT_EQ(GURL(kUrl2), sub_folder->GetChild(1)->url());
  ASSERT_EQ(GURL(kUrl3), sub_folder->GetChild(2)->url());
}
