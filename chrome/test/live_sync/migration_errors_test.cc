// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/live_sync/live_preferences_sync_test.h"
#include "chrome/test/ui_test_utils.h"

// TODO(nick): In this file, we want to test multiple datatypes, but the test
// framework only allows us access to the helper methods of whichever parent
// class we pick.  We should move away from inheritance, and make this a
// TwoClientLiveSyncTest that uses methods currently found in PreferencesSync
// and BookmarksSync code.  We ought to be able to check for profile equality
// on all datatypes.
class MigrationErrorsTest : public TwoClientLivePreferencesSyncTest {

};

// Easiest possible test of migration errors: triggers a server migration on
// one datatype, then modifies some other datatype.
IN_PROC_BROWSER_TEST_F(MigrationErrorsTest, MigratePrefsThenModifyBookmark) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));

  // Phase 1: Before migrating anything, create & sync a preference.
  bool new_kShowHomeButton = !GetVerifierPrefs()->GetBoolean(
      prefs::kShowHomeButton);
  GetVerifierPrefs()->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  GetPrefs(0)->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(0)->GetBoolean(prefs::kShowHomeButton));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));

  // Phase 2: Trigger a preference migration on the server.
  syncable::ModelTypeSet migrate_types;
  migrate_types.insert(syncable::PREFERENCES);
  TriggerMigrationDoneError(migrate_types);

  // Phase 3: Modify a bookmark and await quiescence.
  Profile* p = GetProfile(0);
  p->GetBookmarkModel()->AddURL(p->GetBookmarkModel()->other_node(),
      0,  ASCIIToUTF16("nudge"), GURL("http://nudge.net"));

  ASSERT_TRUE(AwaitQuiescence());

  // Phase 4: Verify that preferences can still be synchronized.
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(0)->GetBoolean(prefs::kShowHomeButton));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));

  new_kShowHomeButton = !new_kShowHomeButton;
  GetVerifierPrefs()->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  GetPrefs(0)->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(0)->GetBoolean(prefs::kShowHomeButton));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));
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
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));

  // Phase 1: Before migrating anything, create & sync a preference.
  bool new_kShowHomeButton = !GetVerifierPrefs()->GetBoolean(
      prefs::kShowHomeButton);
  GetVerifierPrefs()->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  GetPrefs(0)->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(0)->GetBoolean(prefs::kShowHomeButton));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));

  // Phase 2: Trigger a migration on the server.
  syncable::ModelTypeSet migrate_types;
  migrate_types.insert(syncable::PREFERENCES);
  migrate_types.insert(syncable::BOOKMARKS);
  TriggerMigrationDoneError(migrate_types);

  // Phase 3: Modify a bookmark and await quiescence.
  Profile* p = GetProfile(0);
  p->GetBookmarkModel()->AddURL(p->GetBookmarkModel()->other_node(),
      0,  ASCIIToUTF16("nudge"), GURL("http://nudge.net"));
  ASSERT_TRUE(AwaitQuiescence());

  // Phase 4: Verify that preferences can still be synchronized.
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(0)->GetBoolean(prefs::kShowHomeButton));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));

  new_kShowHomeButton = !new_kShowHomeButton;
  GetVerifierPrefs()->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  GetPrefs(0)->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(0)->GetBoolean(prefs::kShowHomeButton));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));
}

// Migrate every datatype in sequence; the catch being that the server
// will only tell the client about the migrations one at a time.
IN_PROC_BROWSER_TEST_F(MigrationErrorsTest, MigrationHellWithoutNigori) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));

  // Phase 1: Before migrating anything, create & sync a preference.
  bool new_kShowHomeButton = !GetVerifierPrefs()->GetBoolean(
      prefs::kShowHomeButton);
  GetVerifierPrefs()->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  GetPrefs(0)->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(0)->GetBoolean(prefs::kShowHomeButton));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));

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

  // Phase 3: Modify a bookmark and await quiescence.
  Profile* p = GetProfile(0);
  p->GetBookmarkModel()->AddURL(p->GetBookmarkModel()->other_node(),
      0,  ASCIIToUTF16("nudge"), GURL("http://nudge.net"));

  ASSERT_TRUE(AwaitQuiescence());

  // Phase 4: Verify that preferences can still be synchronized.
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(0)->GetBoolean(prefs::kShowHomeButton));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));

  new_kShowHomeButton = !new_kShowHomeButton;
  GetVerifierPrefs()->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  GetPrefs(0)->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(0)->GetBoolean(prefs::kShowHomeButton));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       MigrationHellWithNigori) {
  if (!ServerSupportsErrorTriggering()) {
    LOG(WARNING) << "Test skipped in this server environment.";
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));

  // Phase 1: Before migrating anything, create & sync a preference.
  bool new_kShowHomeButton = !GetVerifierPrefs()->GetBoolean(
      prefs::kShowHomeButton);
  GetVerifierPrefs()->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  GetPrefs(0)->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(0)->GetBoolean(prefs::kShowHomeButton));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));

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

  // Phase 3: modify a bookmark and wait for quiescence.
  Profile* p = GetProfile(0);
  p->GetBookmarkModel()->AddURL(p->GetBookmarkModel()->other_node(),
      0,  ASCIIToUTF16("nudge"), GURL("http://nudge.net"));

  ASSERT_TRUE(AwaitQuiescence());

  // Phase 4: Verify that preferences can still be synchronized.
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(0)->GetBoolean(prefs::kShowHomeButton));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));

  new_kShowHomeButton = !new_kShowHomeButton;
  GetVerifierPrefs()->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  GetPrefs(0)->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(0)->GetBoolean(prefs::kShowHomeButton));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));
}

