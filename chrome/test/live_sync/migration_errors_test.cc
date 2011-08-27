// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "chrome/test/live_sync/bookmarks_helper.h"
#include "chrome/test/live_sync/preferences_helper.h"

using bookmarks_helper::AddURL;
using bookmarks_helper::IndexedURL;
using bookmarks_helper::IndexedURLTitle;

using preferences_helper::BooleanPrefMatches;
using preferences_helper::ChangeBooleanPref;

// Tests to make sure that the migration cycle works properly,
// i.e. doesn't stall.

class MigrationCycleTest : public LiveSyncTest {
 public:
  MigrationCycleTest() : LiveSyncTest(SINGLE_CLIENT) {}
  virtual ~MigrationCycleTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MigrationCycleTest);
};

IN_PROC_BROWSER_TEST_F(MigrationCycleTest, PrefsOnly) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  DisableNotifications();

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Phase 1: Trigger a preference migration on the server.
  syncable::ModelTypeSet migrate_types;
  migrate_types.insert(syncable::PREFERENCES);
  TriggerMigrationDoneError(migrate_types);

  // Phase 2: Modify a pref (to trigger migration) and wait for a sync
  // cycle.
  // TODO(akalin): Shouldn't need to wait for full sync cycle; see
  // 93167.
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Migration"));
}

// TODO(akalin): Fails (times out) due to http://crbug.com/92928.
IN_PROC_BROWSER_TEST_F(MigrationCycleTest, DISABLED_PrefsNigori) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  DisableNotifications();

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Phase 1: Trigger a preference and nigori migration on the server.
  {
    syncable::ModelTypeSet migrate_types;
    migrate_types.insert(syncable::PREFERENCES);
    TriggerMigrationDoneError(migrate_types);
  }
  {
    syncable::ModelTypeSet migrate_types;
    migrate_types.insert(syncable::NIGORI);
    TriggerMigrationDoneError(migrate_types);
  }

  // Phase 2: Modify a pref (to trigger migration) and wait for a sync
  // cycle.
  // TODO(akalin): Shouldn't need to wait for full sync cycle; see
  // 93167.
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Migration"));
}

// TODO(akalin): Fails (times out) due to http://crbug.com/92928.
IN_PROC_BROWSER_TEST_F(MigrationCycleTest, DISABLED_BookmarksPrefs) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  DisableNotifications();

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Phase 1: Trigger a bookmark and preference migration on the
  // server.
  {
    syncable::ModelTypeSet migrate_types;
    migrate_types.insert(syncable::BOOKMARKS);
    TriggerMigrationDoneError(migrate_types);
  }
  {
    syncable::ModelTypeSet migrate_types;
    migrate_types.insert(syncable::PREFERENCES);
    TriggerMigrationDoneError(migrate_types);
  }

  // Phase 2: Modify a bookmark (to trigger migration) and wait for a
  // sync cycle.
  // TODO(akalin): Shouldn't need to wait for full sync cycle; see
  // 93167.
  ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Migration"));
}

// TODO(akalin): Add tests where the migration trigger is a poll or a
// nudge from notifications.

class MigrationErrorsTest : public LiveSyncTest {
 public:
  MigrationErrorsTest() : LiveSyncTest(TWO_CLIENT) {}
  virtual ~MigrationErrorsTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MigrationErrorsTest);
};

// Easiest possible test of migration errors: triggers a server migration on
// one datatype, then modifies some other datatype.
// TODO(akalin): Fails (times out) due to http://crbug.com/92928.
IN_PROC_BROWSER_TEST_F(MigrationErrorsTest,
                       DISABLED_MigratePrefsThenModifyBookmark) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Phase 1: Before migrating anything, create & sync a preference.
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));

  // Phase 2: Trigger a preference migration on the server.
  syncable::ModelTypeSet migrate_types;
  migrate_types.insert(syncable::PREFERENCES);
  TriggerMigrationDoneError(migrate_types);

  // Phase 3: Modify a bookmark and wait for it to sync.
  ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Phase 4: Verify that preferences can still be synchronized.
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
}

// Triggers a server migration on two datatypes, then makes a local
// modification to one of them.
// TODO(akalin): Fails (times out) due to http://crbug.com/92928.
IN_PROC_BROWSER_TEST_F(MigrationErrorsTest,
                       DISABLED_MigratePrefsAndBookmarksThenModifyBookmark) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Phase 1: Before migrating anything, create & sync a preference.
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));

  // Phase 2: Trigger a migration on the server.
  syncable::ModelTypeSet migrate_types;
  migrate_types.insert(syncable::PREFERENCES);
  migrate_types.insert(syncable::BOOKMARKS);
  TriggerMigrationDoneError(migrate_types);

  // Phase 3: Modify a bookmark and wait for it to sync.
  ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Phase 4: Verify that preferences can still be synchronized.
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
}

// Migrate every datatype in sequence; the catch being that the server
// will only tell the client about the migrations one at a time.
// TODO(akalin): Fails (times out) due to http://crbug.com/92928.
IN_PROC_BROWSER_TEST_F(MigrationErrorsTest,
                       DISABLED_MigrationHellWithoutNigori) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Phase 1: Before migrating anything, create & sync a preference.
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));

  // Phase 2: Queue up a horrendous number of migrations on the server.
  // Let the first nudge be a datatype that's neither prefs nor bookmarks.
  syncable::ModelTypeSet migrate_themes;
  migrate_themes.insert(syncable::THEMES);
  TriggerMigrationDoneError(migrate_themes);
  for (int i = syncable::FIRST_REAL_MODEL_TYPE; i < syncable::MODEL_TYPE_COUNT;
       ++i) {
    if (i == syncable::NIGORI) {
      continue;
    }
    syncable::ModelTypeSet migrate_types;
    migrate_types.insert(syncable::ModelTypeFromInt(i));
    TriggerMigrationDoneError(migrate_types);
  }

  // Phase 3: Modify a bookmark and wait for it to sync.
  ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Phase 4: Verify that preferences can still be synchronized.
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
}

// TODO(akalin): Fails (times out) due to http://crbug.com/92928.
IN_PROC_BROWSER_TEST_F(MigrationErrorsTest,
                       DISABLED_MigrationHellWithNigori) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Phase 1: Before migrating anything, create & sync a preference.
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));

  // Phase 2: Queue up a horrendous number of migrations on the server.
  // Let the first nudge be a datatype that's neither prefs nor bookmarks.
  syncable::ModelTypeSet migrate_themes;
  migrate_themes.insert(syncable::THEMES);
  TriggerMigrationDoneError(migrate_themes);
  for (int i = syncable::FIRST_REAL_MODEL_TYPE; i < syncable::MODEL_TYPE_COUNT;
       ++i) {
    // TODO(lipalani): If all types are disabled syncer freaks out. Fix it.
    if (i == syncable::BOOKMARKS) {
      continue;
    }
    syncable::ModelTypeSet migrate_types;
    migrate_types.insert(syncable::ModelTypeFromInt(i));
    TriggerMigrationDoneError(migrate_types);
  }

  // Phase 3: Modify a bookmark and wait for it to sync.
  ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Phase 4: Verify that preferences can still be synchronized.
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
}

class MigrationReconfigureTest : public LiveSyncTest {
 public:
  MigrationReconfigureTest() : LiveSyncTest(TWO_CLIENT) {}

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
  ASSERT_FALSE(GetClient(0)->IsTypeRegistered(syncable::SESSIONS));
  ASSERT_FALSE(GetClient(0)->IsTypePreferred(syncable::SESSIONS));

  // Phase 1: Before migrating anything, create & sync a preference.
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));

  // Phase 2: Trigger setting the sync_tabs field.
  TriggerSetSyncTabs();

  // Phase 3: Modify a bookmark and wait for it to sync.
  ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Phase 4: Verify that preferences can still be synchronized.
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));

  // Phase 5: Verify that sessions are registered and enabled.
  ASSERT_TRUE(GetClient(0)->IsTypeRegistered(syncable::SESSIONS));
  ASSERT_TRUE(GetClient(0)->IsTypePreferred(syncable::SESSIONS));
}

// TODO(akalin): Fails (times out) due to http://crbug.com/92928.
IN_PROC_BROWSER_TEST_F(MigrationReconfigureTest,
                       DISABLED_SetSyncTabsAndMigrate) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_FALSE(GetClient(0)->IsTypeRegistered(syncable::SESSIONS));
  ASSERT_FALSE(GetClient(0)->IsTypePreferred(syncable::SESSIONS));

  // Phase 1: Before migrating anything, create & sync a preference.
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));

  // Phase 2: Trigger setting the sync_tabs field.
  TriggerSetSyncTabs();

  // Phase 3: Trigger a preference migration on the server.
  syncable::ModelTypeSet migrate_types;
  migrate_types.insert(syncable::PREFERENCES);
  TriggerMigrationDoneError(migrate_types);

  // Phase 4: Modify a bookmark and wait for it to sync.
  ASSERT_TRUE(AddURL(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Phase 5: Verify that preferences can still be synchronized.
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));
  ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(BooleanPrefMatches(prefs::kShowHomeButton));

  // Phase 6: Verify that sessions are registered and enabled.
  ASSERT_TRUE(GetClient(0)->IsTypeRegistered(syncable::SESSIONS));
  ASSERT_TRUE(GetClient(0)->IsTypePreferred(syncable::SESSIONS));
}
