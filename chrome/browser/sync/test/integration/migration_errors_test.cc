// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(akalin): Rename this file to migration_test.cc.

#include "base/compiler_specific.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"

using bookmarks_helper::AddURL;
using bookmarks_helper::IndexedURL;
using bookmarks_helper::IndexedURLTitle;

using preferences_helper::BooleanPrefMatches;
using preferences_helper::ChangeBooleanPref;

namespace {

// Utility functions to make a model type set out of a small number of
// model types.

syncer::ModelTypeSet MakeSet(syncer::ModelType type) {
  return syncer::ModelTypeSet(type);
}

syncer::ModelTypeSet MakeSet(syncer::ModelType type1,
                             syncer::ModelType type2) {
  return syncer::ModelTypeSet(type1, type2);
}

// An ordered list of model types sets to migrate.  Used by
// RunMigrationTest().
typedef std::deque<syncer::ModelTypeSet> MigrationList;

// Utility functions to make a MigrationList out of a small number of
// model types / model type sets.

MigrationList MakeList(syncer::ModelTypeSet model_types) {
  return MigrationList(1, model_types);
}

MigrationList MakeList(syncer::ModelTypeSet model_types1,
                       syncer::ModelTypeSet model_types2) {
  MigrationList migration_list;
  migration_list.push_back(model_types1);
  migration_list.push_back(model_types2);
  return migration_list;
}

MigrationList MakeList(syncer::ModelType type) {
  return MakeList(MakeSet(type));
}

MigrationList MakeList(syncer::ModelType type1,
                       syncer::ModelType type2) {
  return MakeList(MakeSet(type1), MakeSet(type2));
}

class MigrationTest : public SyncTest {
 public:
  explicit MigrationTest(TestType test_type) : SyncTest(test_type) {}
  virtual ~MigrationTest() {}

  // TODO(akalin): Add more MODIFY_(data type) trigger methods, as
  // well as a poll-based trigger method.
  enum TriggerMethod { MODIFY_PREF, MODIFY_BOOKMARK, TRIGGER_NOTIFICATION };

  syncer::ModelTypeSet GetPreferredDataTypes() {
    // ProfileSyncService must already have been created before we can call
    // GetPreferredDataTypes().
    DCHECK(GetClient(0)->IsSyncAlreadySetup());
    syncer::ModelTypeSet preferred_data_types =
        GetClient(0)->service()->GetPreferredDataTypes();
    preferred_data_types.RemoveAll(syncer::ProxyTypes());
    // Make sure all clients have the same preferred data types.
    for (int i = 1; i < num_clients(); ++i) {
      const syncer::ModelTypeSet other_preferred_data_types =
          GetClient(i)->service()->GetPreferredDataTypes();
      EXPECT_TRUE(preferred_data_types.Equals(other_preferred_data_types));
    }
    return preferred_data_types;
  }

  // Returns a MigrationList with every enabled data type in its own
  // set.
  MigrationList GetPreferredDataTypesList() {
    MigrationList migration_list;
    const syncer::ModelTypeSet preferred_data_types =
        GetPreferredDataTypes();
    for (syncer::ModelTypeSet::Iterator it =
             preferred_data_types.First(); it.Good(); it.Inc()) {
      migration_list.push_back(MakeSet(it.Get()));
    }
    return migration_list;
  }

  // Trigger a migration for the given types with the given method.
  void TriggerMigration(syncer::ModelTypeSet model_types,
                        TriggerMethod trigger_method) {
    switch (trigger_method) {
      case MODIFY_PREF:
        // Unlike MODIFY_BOOKMARK, MODIFY_PREF doesn't cause a
        // notification to happen (since model association on a
        // boolean pref clobbers the local value), so it doesn't work
        // for anything but single-client tests.
        ASSERT_EQ(1, num_clients());
        ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
        ChangeBooleanPref(0, prefs::kShowHomeButton);
        break;
      case MODIFY_BOOKMARK:
        ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))));
        break;
      case TRIGGER_NOTIFICATION:
        TriggerNotification(model_types);
        break;
      default:
        ADD_FAILURE();
    }
  }

  // Block until all clients have completed migration for the given
  // types.
  void AwaitMigration(syncer::ModelTypeSet migrate_types) {
    for (int i = 0; i < num_clients(); ++i) {
      ASSERT_TRUE(GetClient(i)->AwaitMigration(migrate_types));
    }
  }

  bool ShouldRunMigrationTest() const {
    if (!ServerSupportsNotificationControl() ||
        !ServerSupportsErrorTriggering()) {
      LOG(WARNING) << "Test skipped in this server environment.";
      return false;
    }
    return true;
  }

  // Makes sure migration works with the given migration list and
  // trigger method.
  void RunMigrationTest(const MigrationList& migration_list,
                       TriggerMethod trigger_method) {
    ASSERT_TRUE(ShouldRunMigrationTest());

    // If we have only one client, turn off notifications to avoid the
    // possibility of spurious sync cycles.
    bool do_test_without_notifications =
        (trigger_method != TRIGGER_NOTIFICATION && num_clients() == 1);

    if (do_test_without_notifications) {
      DisableNotifications();
    }

    // Phase 1: Trigger the migrations on the server.
    for (MigrationList::const_iterator it = migration_list.begin();
         it != migration_list.end(); ++it) {
      TriggerMigrationDoneError(*it);
    }

    // Phase 2: Trigger each migration individually and wait for it to
    // complete.  (Multiple migrations may be handled by each
    // migration cycle, but there's no guarantee of that, so we have
    // to trigger each migration individually.)
    for (MigrationList::const_iterator it = migration_list.begin();
         it != migration_list.end(); ++it) {
      TriggerMigration(*it, trigger_method);
      AwaitMigration(*it);
    }

    // Phase 3: Wait for all clients to catch up.
    //
    // AwaitQuiescence() will not succeed when notifications are disabled.  We
    // can safely avoid calling it because we know that, in the single client
    // case, there is no one else to wait for.
    //
    // TODO(rlarocque, 97780): Remove the if condition when the test harness
    // supports calling AwaitQuiescence() when notifications are disabled.
    if (!do_test_without_notifications) {
      AwaitQuiescence();
    }

    // TODO(rlarocque): It should be possible to re-enable notifications
    // here, but doing so makes some windows tests flaky.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MigrationTest);
};

class MigrationSingleClientTest : public MigrationTest {
 public:
  MigrationSingleClientTest() : MigrationTest(SINGLE_CLIENT) {}
  virtual ~MigrationSingleClientTest() {}

  void RunSingleClientMigrationTest(const MigrationList& migration_list,
                                    TriggerMethod trigger_method) {
    if (!ShouldRunMigrationTest()) {
      return;
    }
    ASSERT_TRUE(SetupSync());
    RunMigrationTest(migration_list, trigger_method);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MigrationSingleClientTest);
};

// The simplest possible migration tests -- a single data type.

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, PrefsOnlyModifyPref) {
  RunSingleClientMigrationTest(MakeList(syncer::PREFERENCES), MODIFY_PREF);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, PrefsOnlyModifyBookmark) {
  RunSingleClientMigrationTest(MakeList(syncer::PREFERENCES),
                               MODIFY_BOOKMARK);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest,
                       PrefsOnlyTriggerNotification) {
  RunSingleClientMigrationTest(MakeList(syncer::PREFERENCES),
                               TRIGGER_NOTIFICATION);
}

// Nigori is handled specially, so we test that separately.

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, NigoriOnly) {
  RunSingleClientMigrationTest(MakeList(syncer::PREFERENCES),
                               TRIGGER_NOTIFICATION);
}

// A little more complicated -- two data types.

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest,
                       BookmarksPrefsIndividually) {
  RunSingleClientMigrationTest(
      MakeList(syncer::BOOKMARKS, syncer::PREFERENCES),
      MODIFY_PREF);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, BookmarksPrefsBoth) {
  RunSingleClientMigrationTest(
      MakeList(MakeSet(syncer::BOOKMARKS, syncer::PREFERENCES)),
      MODIFY_BOOKMARK);
}

// Two data types with one being nigori.

// See crbug.com/124480.
IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest,
                       DISABLED_PrefsNigoriIndividiaully) {
  RunSingleClientMigrationTest(
      MakeList(syncer::PREFERENCES, syncer::NIGORI),
      TRIGGER_NOTIFICATION);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, PrefsNigoriBoth) {
  RunSingleClientMigrationTest(
      MakeList(MakeSet(syncer::PREFERENCES, syncer::NIGORI)),
      MODIFY_PREF);
}

// The whole shebang -- all data types.

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, AllTypesIndividually) {
  ASSERT_TRUE(SetupClients());
  RunSingleClientMigrationTest(GetPreferredDataTypesList(), MODIFY_BOOKMARK);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest,
                       AllTypesIndividuallyTriggerNotification) {
  ASSERT_TRUE(SetupClients());
  RunSingleClientMigrationTest(GetPreferredDataTypesList(),
                               TRIGGER_NOTIFICATION);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, AllTypesAtOnce) {
  ASSERT_TRUE(SetupClients());
  RunSingleClientMigrationTest(MakeList(GetPreferredDataTypes()),
                               MODIFY_PREF);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest,
                       AllTypesAtOnceTriggerNotification) {
  ASSERT_TRUE(SetupClients());
  RunSingleClientMigrationTest(MakeList(GetPreferredDataTypes()),
                               TRIGGER_NOTIFICATION);
}

// All data types plus nigori.

// See crbug.com/124480.
IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest,
                       DISABLED_AllTypesWithNigoriIndividually) {
  ASSERT_TRUE(SetupClients());
  MigrationList migration_list = GetPreferredDataTypesList();
  migration_list.push_front(MakeSet(syncer::NIGORI));
  RunSingleClientMigrationTest(migration_list, MODIFY_BOOKMARK);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest,
                       AllTypesWithNigoriAtOnce) {
  ASSERT_TRUE(SetupClients());
  syncer::ModelTypeSet all_types = GetPreferredDataTypes();
  all_types.Put(syncer::NIGORI);
  RunSingleClientMigrationTest(MakeList(all_types), MODIFY_PREF);
}

class MigrationTwoClientTest : public MigrationTest {
 public:
  MigrationTwoClientTest() : MigrationTest(TWO_CLIENT) {}
  virtual ~MigrationTwoClientTest() {}

  // Helper function that verifies that preferences sync still works.
  void VerifyPrefSync() {
    ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
    ChangeBooleanPref(0, prefs::kShowHomeButton);
    ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
    ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  }

  void RunTwoClientMigrationTest(const MigrationList& migration_list,
                                    TriggerMethod trigger_method) {
    if (!ShouldRunMigrationTest()) {
      return;
    }
    ASSERT_TRUE(SetupSync());

    // Make sure pref sync works before running the migration test.
    VerifyPrefSync();

    RunMigrationTest(migration_list, trigger_method);

    // Make sure pref sync still works after running the migration
    // test.
    VerifyPrefSync();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MigrationTwoClientTest);
};

// Easiest possible test of migration errors: triggers a server
// migration on one datatype, then modifies some other datatype.
IN_PROC_BROWSER_TEST_F(MigrationTwoClientTest, MigratePrefsThenModifyBookmark) {
  RunTwoClientMigrationTest(MakeList(syncer::PREFERENCES),
                            MODIFY_BOOKMARK);
}

// Triggers a server migration on two datatypes, then makes a local
// modification to one of them.
IN_PROC_BROWSER_TEST_F(MigrationTwoClientTest,
                       MigratePrefsAndBookmarksThenModifyBookmark) {
  RunTwoClientMigrationTest(
      MakeList(syncer::PREFERENCES, syncer::BOOKMARKS),
      MODIFY_BOOKMARK);
}

// Migrate every datatype in sequence; the catch being that the server
// will only tell the client about the migrations one at a time.
// TODO(rsimha): This test takes longer than 60 seconds, and will cause tree
// redness due to sharding.
// Re-enable this test after syncer::kInitialBackoffShortRetrySeconds is reduced
// to zero.
IN_PROC_BROWSER_TEST_F(MigrationTwoClientTest,
                       DISABLED_MigrationHellWithoutNigori) {
  ASSERT_TRUE(SetupClients());
  MigrationList migration_list = GetPreferredDataTypesList();
  // Let the first nudge be a datatype that's neither prefs nor
  // bookmarks.
  migration_list.push_front(MakeSet(syncer::THEMES));
  RunTwoClientMigrationTest(migration_list, MODIFY_BOOKMARK);
}

// See crbug.com/124480.
IN_PROC_BROWSER_TEST_F(MigrationTwoClientTest,
                       DISABLED_MigrationHellWithNigori) {
  ASSERT_TRUE(SetupClients());
  MigrationList migration_list = GetPreferredDataTypesList();
  // Let the first nudge be a datatype that's neither prefs nor
  // bookmarks.
  migration_list.push_front(MakeSet(syncer::THEMES));
  // Pop off one so that we don't migrate all data types; the syncer
  // freaks out if we do that (see http://crbug.com/94882).
  ASSERT_GE(migration_list.size(), 2u);
  ASSERT_FALSE(migration_list.back().Equals(MakeSet(syncer::NIGORI)));
  migration_list.back() = MakeSet(syncer::NIGORI);
  RunTwoClientMigrationTest(migration_list, MODIFY_BOOKMARK);
}

class MigrationReconfigureTest : public MigrationTwoClientTest {
 public:
  MigrationReconfigureTest() {}

  virtual void SetUpCommandLine(CommandLine* cl) OVERRIDE {
    AddTestSwitches(cl);
    // Do not add optional datatypes.
  }

  virtual ~MigrationReconfigureTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MigrationReconfigureTest);
};

}  // namespace
