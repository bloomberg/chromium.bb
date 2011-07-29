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

class MigrationErrorsTest : public LiveSyncTest {
 public:
  MigrationErrorsTest() : LiveSyncTest(TWO_CLIENT) {}
  virtual ~MigrationErrorsTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MigrationErrorsTest);
};

// Easiest possible test of migration errors: triggers a server migration on
// one datatype, then modifies some other datatype.
IN_PROC_BROWSER_TEST_F(MigrationErrorsTest, MigratePrefsThenModifyBookmark) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Phase 1: Before migrating anything, create & sync a preference.
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
  PreferencesHelper::ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));

  // Phase 2: Trigger a preference migration on the server.
  syncable::ModelTypeSet migrate_types;
  migrate_types.insert(syncable::PREFERENCES);
  TriggerMigrationDoneError(migrate_types);

  // Phase 3: Modify a bookmark and wait for it to sync.
  ASSERT_TRUE(BookmarksHelper::AddURL(0, BookmarksHelper::IndexedURLTitle(0),
      GURL(BookmarksHelper::IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Phase 4: Verify that preferences can still be synchronized.
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
  PreferencesHelper::ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
}

// Triggers a server migration on two datatypes, then makes a local
// modification to one of them.
IN_PROC_BROWSER_TEST_F(MigrationErrorsTest,
    MigratePrefsAndBookmarksThenModifyBookmark) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Phase 1: Before migrating anything, create & sync a preference.
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
  PreferencesHelper::ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));

  // Phase 2: Trigger a migration on the server.
  syncable::ModelTypeSet migrate_types;
  migrate_types.insert(syncable::PREFERENCES);
  migrate_types.insert(syncable::BOOKMARKS);
  TriggerMigrationDoneError(migrate_types);

  // Phase 3: Modify a bookmark and wait for it to sync.
  ASSERT_TRUE(BookmarksHelper::AddURL(0, BookmarksHelper::IndexedURLTitle(0),
      GURL(BookmarksHelper::IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Phase 4: Verify that preferences can still be synchronized.
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
  PreferencesHelper::ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
}

// Migrate every datatype in sequence; the catch being that the server
// will only tell the client about the migrations one at a time.
IN_PROC_BROWSER_TEST_F(MigrationErrorsTest, MigrationHellWithoutNigori) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Phase 1: Before migrating anything, create & sync a preference.
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
  PreferencesHelper::ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));

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
  ASSERT_TRUE(BookmarksHelper::AddURL(0, BookmarksHelper::IndexedURLTitle(0),
      GURL(BookmarksHelper::IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Phase 4: Verify that preferences can still be synchronized.
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
  PreferencesHelper::ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
}

IN_PROC_BROWSER_TEST_F(MigrationErrorsTest, MigrationHellWithNigori) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Phase 1: Before migrating anything, create & sync a preference.
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
  PreferencesHelper::ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));

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
  ASSERT_TRUE(BookmarksHelper::AddURL(0, BookmarksHelper::IndexedURLTitle(0),
      GURL(BookmarksHelper::IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // Phase 4: Verify that preferences can still be synchronized.
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
  PreferencesHelper::ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
}
