// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
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

  void EnableRollback() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kSyncEnableRollback);
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

class BackupModeChecker {
 public:
  explicit BackupModeChecker(ProfileSyncService* service,
                             base::TimeDelta timeout)
      : pss_(service),
        timeout_(timeout) {}

  bool Wait() {
    expiration_ = base::TimeTicks::Now() + timeout_;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&BackupModeChecker::PeriodicCheck, base::Unretained(this)),
        base::TimeDelta::FromSeconds(1));
    base::MessageLoop::current()->Run();
    return IsBackupComplete();
  }

 private:
  void PeriodicCheck() {
    if (IsBackupComplete() || base::TimeTicks::Now() > expiration_) {
      base::MessageLoop::current()->Quit();
    } else {
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&BackupModeChecker::PeriodicCheck, base::Unretained(this)),
          base::TimeDelta::FromSeconds(1));
    }
  }

  bool IsBackupComplete() {
    return pss_->backend_mode() == ProfileSyncService::BACKUP &&
        pss_->ShouldPushChanges();
  }

  ProfileSyncService* pss_;
  base::TimeDelta timeout_;
  base::TimeTicks expiration_;
};

#if defined(ENABLE_PRE_SYNC_BACKUP)
#define MAYBE_TestBackupDisabled TestBackupDisabled
#else
#define MAYBE_TestBackupDisabled DISABLED_TestBackupDisabled
#endif
IN_PROC_BROWSER_TEST_F(SingleClientBackupRollbackTest,
                       MAYBE_TestBackupDisabled) {
  DisableBackup();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  BackupModeChecker checker(GetSyncService(0),
                            base::TimeDelta::FromSeconds(15));
  ASSERT_FALSE(checker.Wait());

  ASSERT_EQ(ProfileSyncService::IDLE, GetSyncService(0)->backend_mode());
}

#if defined(ENABLE_PRE_SYNC_BACKUP)
#define MAYBE_TestBackupOnly TestBackupOnly
#else
#define MAYBE_TestBackupOnly DISABLED_TestBackupOnly
#endif
IN_PROC_BROWSER_TEST_F(SingleClientBackupRollbackTest,
                       MAYBE_TestBackupOnly) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Starting state:
  // other_node
  //    -> http://mail.google.com  "url0"
  //    -> http://www.nhl.com "url1"
  ASSERT_TRUE(AddURL(0, GetOtherNode(0), 0, "url0",
                     GURL("http://mail.google.com")));
  ASSERT_TRUE(AddURL(0, GetOtherNode(0), 1, "url1",
                     GURL("http://www.nhl.com")));

  BackupModeChecker checker(GetSyncService(0),
                            base::TimeDelta::FromSeconds(15));
  ASSERT_TRUE(checker.Wait());

  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
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

  // Wait for sync to switch to backup mode.
  ASSERT_TRUE(checker.Wait());

  // With rollback disabled, bookmarks in backup DB should not be restored.
  // Only bookmark added during sync is present.
  ASSERT_EQ(1, GetOtherNode(0)->child_count());
  ASSERT_EQ(GURL("http://www.yahoo.com"),
            GetOtherNode(0)->GetChild(0)->url());
}

#if defined(ENABLE_PRE_SYNC_BACKUP)
#define MAYBE_TestBackupRollback TestBackupRollback
#else
#define MAYBE_TestBackupRollback DISABLED_TestBackupRollback
#endif
IN_PROC_BROWSER_TEST_F(SingleClientBackupRollbackTest,
                       MAYBE_TestBackupRollback) {
  EnableRollback();

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

  BackupModeChecker checker(GetSyncService(0),
                            base::TimeDelta::FromSeconds(15));
  ASSERT_TRUE(checker.Wait());

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

  // Verify backup time is set on device info.
  base::Time backup_time = GetBackupDbLastModified();
  ASSERT_FALSE(backup_time.is_null());
  ASSERT_EQ(backup_time, GetSyncService(0)->GetDeviceBackupTimeForTesting());

  // Let server to return rollback command on next sync request.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::USER_ROLLBACK);

  // Make another change to trigger downloading of rollback command.
  Remove(0, tier1_b, 0);

  // Wait for sync to switch to backup mode after finishing rollback.
  ASSERT_TRUE(checker.Wait());

  // Verify bookmarks are restored.
  ASSERT_EQ(1, tier1_a->child_count());
  const BookmarkNode* url1 = tier1_a->GetChild(0);
  ASSERT_EQ(GURL("http://mail.google.com"), url1->url());

  ASSERT_EQ(1, tier1_b->child_count());
  const BookmarkNode* url2 = tier1_b->GetChild(0);
  ASSERT_EQ(GURL("http://www.nhl.com"), url2->url());
}

#if defined(ENABLE_PRE_SYNC_BACKUP)
#define MAYBE_TestPrefBackupRollback TestPrefBackupRollback
#else
#define MAYBE_TestPrefBackupRollback DISABLED_TestPrefBackupRollback
#endif
// Verify local preferences are not affected by preferences in backup DB under
// backup mode.
IN_PROC_BROWSER_TEST_F(SingleClientBackupRollbackTest,
                       MAYBE_TestPrefBackupRollback) {
  EnableRollback();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  preferences_helper::ChangeStringPref(0, prefs::kHomePage, kUrl1);

  BackupModeChecker checker(GetSyncService(0),
                            base::TimeDelta::FromSeconds(15));
  ASSERT_TRUE(checker.Wait());

  // Shut down backup, then change preference.
  GetSyncService(0)->StartStopBackupForTesting();
  preferences_helper::ChangeStringPref(0, prefs::kHomePage, kUrl2);

  // Restart backup. Preference shouldn't change after backup starts.
  GetSyncService(0)->StartStopBackupForTesting();
  ASSERT_TRUE(checker.Wait());
  ASSERT_EQ(kUrl2,
            preferences_helper::GetPrefs(0)->GetString(prefs::kHomePage));

  // Start sync and change preference.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  preferences_helper::ChangeStringPref(0, prefs::kHomePage, kUrl3);
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Let server return rollback command on next sync request.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::USER_ROLLBACK);

  // Make another change to trigger downloading of rollback command.
  preferences_helper::ChangeStringPref(0, prefs::kHomePage, "");

  // Wait for sync to switch to backup mode after finishing rollback.
  ASSERT_TRUE(checker.Wait());

  // Verify preference is restored.
  ASSERT_EQ(kUrl2,
            preferences_helper::GetPrefs(0)->GetString(prefs::kHomePage));
}

#if defined(ENABLE_PRE_SYNC_BACKUP)
#define MAYBE_RollbackNoBackup RollbackNoBackup
#else
#define MAYBE_RollbackNoBackup DISABLED_RollbackNoBackup
#endif
IN_PROC_BROWSER_TEST_F(SingleClientBackupRollbackTest,
                       MAYBE_RollbackNoBackup) {
  EnableRollback();

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Starting state:
  // other_node
  //    -> http://mail.google.com  "url0"
  //    -> http://www.nhl.com "url1"
  ASSERT_TRUE(AddURL(0, GetOtherNode(0), 0, "url0",
                     GURL("http://mail.google.com")));
  ASSERT_TRUE(AddURL(0, GetOtherNode(0), 1, "url1",
                     GURL("http://www.nhl.com")));

  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Let server to return rollback command on next sync request.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::USER_ROLLBACK);

  // Make another change to trigger downloading of rollback command.
  Remove(0, GetOtherNode(0), 0);

  // Wait for sync to switch to backup mode after finishing rollback.
  BackupModeChecker checker(GetSyncService(0),
                            base::TimeDelta::FromSeconds(15));
  ASSERT_TRUE(checker.Wait());

  // Without backup DB, bookmarks added during sync shouldn't be removed.
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

  BackupModeChecker checker(GetSyncService(0),
                            base::TimeDelta::FromSeconds(15));
  ASSERT_TRUE(checker.Wait());

  // Restart backup.
  GetSyncService(0)->StartStopBackupForTesting();
  GetSyncService(0)->StartStopBackupForTesting();
  ASSERT_TRUE(checker.Wait());

  // Verify bookmarks are unchanged.
  ASSERT_EQ(3, sub_folder->child_count());
  ASSERT_EQ(GURL(kUrl1), sub_folder->GetChild(0)->url());
  ASSERT_EQ(GURL(kUrl2), sub_folder->GetChild(1)->url());
  ASSERT_EQ(GURL(kUrl3), sub_folder->GetChild(2)->url());
}
