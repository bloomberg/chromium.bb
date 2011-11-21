// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(akalin): Rename this file to migration_test.cc.

#include "base/compiler_specific.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
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

syncable::ModelTypeSet MakeSet(syncable::ModelType type) {
  syncable::ModelTypeSet model_types;
  model_types.insert(type);
  return model_types;
}

syncable::ModelTypeSet MakeSet(syncable::ModelType type1,
                               syncable::ModelType type2) {
  syncable::ModelTypeSet model_types;
  model_types.insert(type1);
  model_types.insert(type2);
  return model_types;
}

// An ordered list of model types sets to migrate.  Used by
// RunMigrationTest().
typedef std::deque<syncable::ModelTypeSet> MigrationList;

// Utility functions to make a MigrationList out of a small number of
// model types / model type sets.

MigrationList MakeList(const syncable::ModelTypeSet& model_types) {
  return MigrationList(1, model_types);
}

MigrationList MakeList(const syncable::ModelTypeSet& model_types1,
                       const syncable::ModelTypeSet& model_types2) {
  MigrationList migration_list;
  migration_list.push_back(model_types1);
  migration_list.push_back(model_types2);
  return migration_list;
}

MigrationList MakeList(syncable::ModelType type) {
  return MakeList(MakeSet(type));
}

MigrationList MakeList(syncable::ModelType type1,
                       syncable::ModelType type2) {
  return MakeList(MakeSet(type1), MakeSet(type2));
}

class MigrationTest : public SyncTest {
 public:
  explicit MigrationTest(TestType test_type) : SyncTest(test_type) {}
  virtual ~MigrationTest() {}

  // TODO(akalin): Add more MODIFY_(data type) trigger methods, as
  // well as a poll-based trigger method.
  enum TriggerMethod { MODIFY_PREF, MODIFY_BOOKMARK, TRIGGER_NOTIFICATION };

  syncable::ModelTypeSet GetPreferredDataTypes() {
    syncable::ModelTypeSet preferred_data_types;
    GetClient(0)->service()->GetPreferredDataTypes(&preferred_data_types);
    // Make sure all clients have the same preferred data types.
    for (int i = 1; i < num_clients(); ++i) {
      syncable::ModelTypeSet other_preferred_data_types;
      GetClient(i)->service()->GetPreferredDataTypes(
          &other_preferred_data_types);
      EXPECT_EQ(preferred_data_types, other_preferred_data_types);
    }
    return preferred_data_types;
  }

  // Returns a MigrationList with every enabled data type in its own
  // set.
  MigrationList GetPreferredDataTypesList() {
    MigrationList migration_list;
    const syncable::ModelTypeSet& preferred_data_types =
        GetPreferredDataTypes();
    for (syncable::ModelTypeSet::const_iterator it =
             preferred_data_types.begin();
         it != preferred_data_types.end(); ++it) {
      migration_list.push_back(MakeSet(*it));
    }
    return migration_list;
  }

  // Trigger a migration for the given types with the given method.
  void TriggerMigration(const syncable::ModelTypeSet& model_types,
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
  void AwaitMigration(const syncable::ModelTypeSet& migrate_types) {
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
    ASSERT_TRUE(AwaitQuiescence());

    // Re-enable notifications if we disabled it.
    if (do_test_without_notifications) {
      EnableNotifications();
    }
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
  RunSingleClientMigrationTest(MakeList(syncable::PREFERENCES), MODIFY_PREF);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, PrefsOnlyModifyBookmark) {
  RunSingleClientMigrationTest(MakeList(syncable::PREFERENCES),
                               MODIFY_BOOKMARK);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest,
                       PrefsOnlyTriggerNotification) {
  RunSingleClientMigrationTest(MakeList(syncable::PREFERENCES),
                               TRIGGER_NOTIFICATION);
}

// Nigori is handled specially, so we test that separately.

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, NigoriOnly) {
  RunSingleClientMigrationTest(MakeList(syncable::PREFERENCES),
                               TRIGGER_NOTIFICATION);
}

// A little more complicated -- two data types.

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest,
                       BookmarksPrefsIndividually) {
  RunSingleClientMigrationTest(
      MakeList(syncable::BOOKMARKS, syncable::PREFERENCES),
      MODIFY_PREF);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, BookmarksPrefsBoth) {
  RunSingleClientMigrationTest(
      MakeList(MakeSet(syncable::BOOKMARKS, syncable::PREFERENCES)),
      MODIFY_BOOKMARK);
}

// Two data types with one being nigori.

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, PrefsNigoriIndividiaully) {
  RunSingleClientMigrationTest(
      MakeList(syncable::PREFERENCES, syncable::NIGORI),
      TRIGGER_NOTIFICATION);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, PrefsNigoriBoth) {
  RunSingleClientMigrationTest(
      MakeList(MakeSet(syncable::PREFERENCES, syncable::NIGORI)),
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

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest,
                       AllTypesWithNigoriIndividually) {
  ASSERT_TRUE(SetupClients());
  MigrationList migration_list = GetPreferredDataTypesList();
  migration_list.push_front(MakeSet(syncable::NIGORI));
  RunSingleClientMigrationTest(migration_list, MODIFY_BOOKMARK);
}

IN_PROC_BROWSER_TEST_F(MigrationSingleClientTest, AllTypesWithNigoriAtOnce) {
  ASSERT_TRUE(SetupClients());
  syncable::ModelTypeSet all_types = GetPreferredDataTypes();
  all_types.insert(syncable::NIGORI);
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
// Flaky. crbug.com/100382.
IN_PROC_BROWSER_TEST_F(MigrationTwoClientTest,
                       FLAKY_MigratePrefsThenModifyBookmark) {
  RunTwoClientMigrationTest(MakeList(syncable::PREFERENCES),
                            MODIFY_BOOKMARK);
}

// Triggers a server migration on two datatypes, then makes a local
// modification to one of them.
// Flaky. crbug.com/100382.
IN_PROC_BROWSER_TEST_F(MigrationTwoClientTest,
                       FLAKY_MigratePrefsAndBookmarksThenModifyBookmark) {
  RunTwoClientMigrationTest(
      MakeList(syncable::PREFERENCES, syncable::BOOKMARKS),
      MODIFY_BOOKMARK);
}

// Migrate every datatype in sequence; the catch being that the server
// will only tell the client about the migrations one at a time.
IN_PROC_BROWSER_TEST_F(MigrationTwoClientTest, MigrationHellWithoutNigori) {
  ASSERT_TRUE(SetupClients());
  MigrationList migration_list = GetPreferredDataTypesList();
  // Let the first nudge be a datatype that's neither prefs nor
  // bookmarks.
  migration_list.push_front(MakeSet(syncable::THEMES));
  RunTwoClientMigrationTest(migration_list, MODIFY_BOOKMARK);
}

IN_PROC_BROWSER_TEST_F(MigrationTwoClientTest, MigrationHellWithNigori) {
  ASSERT_TRUE(SetupClients());
  MigrationList migration_list = GetPreferredDataTypesList();
  // Let the first nudge be a datatype that's neither prefs nor
  // bookmarks.
  migration_list.push_front(MakeSet(syncable::THEMES));
  // Pop off one so that we don't migrate all data types; the syncer
  // freaks out if we do that (see http://crbug.com/94882).
  ASSERT_GE(migration_list.size(), 2u);
  ASSERT_NE(migration_list.back(), MakeSet(syncable::NIGORI));
  migration_list.back() = MakeSet(syncable::NIGORI);
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

IN_PROC_BROWSER_TEST_F(MigrationReconfigureTest, SetSyncTabs) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_FALSE(GetClient(0)->IsTypeRunning(syncable::SESSIONS));
  ASSERT_FALSE(GetClient(0)->IsTypePreferred(syncable::SESSIONS));

  // Phase 1: Before migrating anything, create & sync a preference.
  VerifyPrefSync();

  // Phase 2: Trigger setting the sync_tabs field.
  TriggerSetSyncTabs();

  // Phase 3: Modify a bookmark and wait for it to sync.
  ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Phase 4: Verify that preferences can still be synchronized.
  VerifyPrefSync();

  // Phase 5: Verify that sessions are registered and enabled.
  ASSERT_TRUE(GetClient(0)->IsTypeRunning(syncable::SESSIONS));
  ASSERT_TRUE(GetClient(0)->IsTypePreferred(syncable::SESSIONS));
}

// Flaky. crbug.com/100382.
IN_PROC_BROWSER_TEST_F(MigrationReconfigureTest, FLAKY_SetSyncTabsAndMigrate) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_FALSE(GetClient(0)->IsTypeRunning(syncable::SESSIONS));
  ASSERT_FALSE(GetClient(0)->IsTypePreferred(syncable::SESSIONS));

  // Phase 1: Before migrating anything, create & sync a preference.
  VerifyPrefSync();

  // Phase 2: Trigger setting the sync_tabs field.
  TriggerSetSyncTabs();

  // Phase 3: Trigger a preference migration on the server.
  RunMigrationTest(MakeList(syncable::PREFERENCES), MODIFY_BOOKMARK);

  // Phase 5: Verify that preferences can still be synchronized.
  VerifyPrefSync();

  // Phase 6: Verify that sessions are registered and enabled.
  ASSERT_TRUE(GetClient(0)->IsTypeRunning(syncable::SESSIONS));
  ASSERT_TRUE(GetClient(0)->IsTypePreferred(syncable::SESSIONS));
}

}  // namespace
