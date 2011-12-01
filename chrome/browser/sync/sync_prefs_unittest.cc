// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_prefs.h"

#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/test/base/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

class SyncPrefsTest : public testing::Test {
 protected:
  TestingPrefService pref_service_;

 private:
  MessageLoop loop_;
};

// Get all types with a user-facing component.
syncable::ModelTypeSet GetNonPassiveTypes() {
  syncable::ModelTypeSet all_types;
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    const syncable::ModelType type = syncable::ModelTypeFromInt(i);
    all_types.insert(type);
  }
  all_types.erase(syncable::NIGORI);
  return all_types;
}

// Returns all types visible from the setup UI.
syncable::ModelTypeSet GetUserVisibleTypes() {
  syncable::ModelTypeSet user_visible_types(GetNonPassiveTypes());
  user_visible_types.erase(syncable::AUTOFILL_PROFILE);
  user_visible_types.erase(syncable::SEARCH_ENGINES);
  user_visible_types.erase(syncable::APP_NOTIFICATIONS);
  return user_visible_types;
}

TEST_F(SyncPrefsTest, Basic) {
  SyncPrefs sync_prefs(&pref_service_);

  EXPECT_FALSE(sync_prefs.HasSyncSetupCompleted());
  sync_prefs.SetSyncSetupCompleted();
  EXPECT_TRUE(sync_prefs.HasSyncSetupCompleted());

  EXPECT_FALSE(sync_prefs.IsStartSuppressed());
  sync_prefs.SetStartSuppressed(true);
  EXPECT_TRUE(sync_prefs.IsStartSuppressed());
  sync_prefs.SetStartSuppressed(false);
  EXPECT_FALSE(sync_prefs.IsStartSuppressed());

  EXPECT_EQ(base::Time(), sync_prefs.GetLastSyncedTime());
  const base::Time& now = base::Time::Now();
  sync_prefs.SetLastSyncedTime(now);
  EXPECT_EQ(now, sync_prefs.GetLastSyncedTime());

  EXPECT_TRUE(sync_prefs.HasKeepEverythingSynced());
  sync_prefs.SetKeepEverythingSynced(false);
  EXPECT_FALSE(sync_prefs.HasKeepEverythingSynced());
  sync_prefs.SetKeepEverythingSynced(true);
  EXPECT_TRUE(sync_prefs.HasKeepEverythingSynced());

  EXPECT_TRUE(sync_prefs.GetEncryptionBootstrapToken().empty());
  sync_prefs.SetEncryptionBootstrapToken("token");
  EXPECT_EQ("token", sync_prefs.GetEncryptionBootstrapToken());
}

TEST_F(SyncPrefsTest, PreferredTypesKeepEverythingSynced) {
  SyncPrefs sync_prefs(&pref_service_);

  EXPECT_TRUE(sync_prefs.HasKeepEverythingSynced());

  const syncable::ModelTypeSet& non_passive_types = GetNonPassiveTypes();
  EXPECT_EQ(non_passive_types,
            sync_prefs.GetPreferredDataTypes(non_passive_types));
  const syncable::ModelTypeSet& user_visible_types = GetUserVisibleTypes();
  for (syncable::ModelTypeSet::const_iterator it = user_visible_types.begin();
       it != user_visible_types.end(); ++it) {
    syncable::ModelTypeSet preferred_types;
    preferred_types.insert(*it);
    sync_prefs.SetPreferredDataTypes(non_passive_types, preferred_types);
    EXPECT_EQ(non_passive_types,
              sync_prefs.GetPreferredDataTypes(non_passive_types));
  }
}

TEST_F(SyncPrefsTest, PreferredTypesNotKeepEverythingSynced) {
  SyncPrefs sync_prefs(&pref_service_);

  sync_prefs.SetKeepEverythingSynced(false);

  const syncable::ModelTypeSet& non_passive_types = GetNonPassiveTypes();
  EXPECT_EQ(non_passive_types,
            sync_prefs.GetPreferredDataTypes(non_passive_types));
  const syncable::ModelTypeSet& user_visible_types = GetUserVisibleTypes();
  for (syncable::ModelTypeSet::const_iterator it = user_visible_types.begin();
       it != user_visible_types.end(); ++it) {
    syncable::ModelTypeSet preferred_types;
    preferred_types.insert(*it);
    syncable::ModelTypeSet expected_preferred_types(preferred_types);
    if (*it == syncable::AUTOFILL) {
      expected_preferred_types.insert(syncable::AUTOFILL_PROFILE);
    }
    if (*it == syncable::PREFERENCES) {
      expected_preferred_types.insert(syncable::SEARCH_ENGINES);
    }
    if (*it == syncable::APPS) {
      expected_preferred_types.insert(syncable::APP_NOTIFICATIONS);
    }
    sync_prefs.SetPreferredDataTypes(non_passive_types, preferred_types);
    EXPECT_EQ(expected_preferred_types,
              sync_prefs.GetPreferredDataTypes(non_passive_types));
  }
}

TEST_F(SyncPrefsTest, MaxInvalidationVersions) {
  SyncPrefs sync_prefs(&pref_service_);

  sync_notifier::InvalidationVersionMap expected_max_versions;
  EXPECT_EQ(expected_max_versions, sync_prefs.GetAllMaxVersions());

  expected_max_versions[syncable::BOOKMARKS] = 2;
  sync_prefs.SetMaxVersion(syncable::BOOKMARKS, 2);
  EXPECT_EQ(expected_max_versions, sync_prefs.GetAllMaxVersions());

  expected_max_versions[syncable::PREFERENCES] = 5;
  sync_prefs.SetMaxVersion(syncable::PREFERENCES, 5);
  EXPECT_EQ(expected_max_versions, sync_prefs.GetAllMaxVersions());

  expected_max_versions[syncable::APP_NOTIFICATIONS] = 3;
  sync_prefs.SetMaxVersion(syncable::APP_NOTIFICATIONS, 3);
  EXPECT_EQ(expected_max_versions, sync_prefs.GetAllMaxVersions());

  expected_max_versions[syncable::APP_NOTIFICATIONS] = 4;
  sync_prefs.SetMaxVersion(syncable::APP_NOTIFICATIONS, 4);
  EXPECT_EQ(expected_max_versions, sync_prefs.GetAllMaxVersions());
}

class MockSyncPrefObserver : public SyncPrefObserver {
 public:
  MOCK_METHOD1(OnSyncManagedPrefChange, void(bool));
};

TEST_F(SyncPrefsTest, ObservedPrefs) {
  SyncPrefs sync_prefs(&pref_service_);

  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  InSequence dummy;
  EXPECT_CALL(mock_sync_pref_observer, OnSyncManagedPrefChange(true));
  EXPECT_CALL(mock_sync_pref_observer, OnSyncManagedPrefChange(false));

  EXPECT_FALSE(sync_prefs.IsManaged());

  sync_prefs.AddSyncPrefObserver(&mock_sync_pref_observer);

  sync_prefs.SetManagedForTest(true);
  EXPECT_TRUE(sync_prefs.IsManaged());
  sync_prefs.SetManagedForTest(false);
  EXPECT_FALSE(sync_prefs.IsManaged());

  sync_prefs.RemoveSyncPrefObserver(&mock_sync_pref_observer);
}

TEST_F(SyncPrefsTest, AcknowledgeSyncedTypes) {
  SyncPrefs sync_prefs(&pref_service_);

  syncable::ModelTypeSet expected_acknowledge_synced_types =
      sync_prefs.GetAcknowledgeSyncedTypesForTest();
  for (int i = syncable::EXTENSION_SETTINGS;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    const syncable::ModelType type = syncable::ModelTypeFromInt(i);
    syncable::ModelTypeSet acknowledge_synced_types;
    acknowledge_synced_types.insert(type);
    expected_acknowledge_synced_types.insert(type);
    sync_prefs.AcknowledgeSyncedTypes(acknowledge_synced_types);
    EXPECT_EQ(expected_acknowledge_synced_types,
              sync_prefs.GetAcknowledgeSyncedTypesForTest());
  }
}

TEST_F(SyncPrefsTest, ClearPreferences) {
  SyncPrefs sync_prefs(&pref_service_);

  EXPECT_FALSE(sync_prefs.HasSyncSetupCompleted());
  EXPECT_EQ(base::Time(), sync_prefs.GetLastSyncedTime());
  EXPECT_TRUE(sync_prefs.GetEncryptionBootstrapToken().empty());
  EXPECT_TRUE(sync_prefs.GetAllMaxVersions().empty());

  sync_prefs.SetSyncSetupCompleted();
  sync_prefs.SetLastSyncedTime(base::Time::Now());
  sync_prefs.SetEncryptionBootstrapToken("token");

  EXPECT_TRUE(sync_prefs.HasSyncSetupCompleted());
  EXPECT_NE(base::Time(), sync_prefs.GetLastSyncedTime());
  EXPECT_EQ("token", sync_prefs.GetEncryptionBootstrapToken());
  {
    sync_notifier::InvalidationVersionMap expected_max_versions;
    expected_max_versions[syncable::APP_NOTIFICATIONS] = 3;
    sync_prefs.SetMaxVersion(syncable::APP_NOTIFICATIONS, 3);
    EXPECT_EQ(expected_max_versions, sync_prefs.GetAllMaxVersions());
  }

  sync_prefs.ClearPreferences();

  EXPECT_FALSE(sync_prefs.HasSyncSetupCompleted());
  EXPECT_EQ(base::Time(), sync_prefs.GetLastSyncedTime());
  EXPECT_TRUE(sync_prefs.GetEncryptionBootstrapToken().empty());
  EXPECT_TRUE(sync_prefs.GetAllMaxVersions().empty());
}

TEST_F(SyncPrefsTest, NullPrefService) {
  SyncPrefs sync_prefs(NULL);

  EXPECT_FALSE(sync_prefs.HasSyncSetupCompleted());
  EXPECT_FALSE(sync_prefs.IsStartSuppressed());
  EXPECT_EQ(base::Time(), sync_prefs.GetLastSyncedTime());
  EXPECT_FALSE(sync_prefs.HasKeepEverythingSynced());
  const syncable::ModelTypeSet& non_passive_types = GetNonPassiveTypes();
  EXPECT_TRUE(sync_prefs.GetPreferredDataTypes(non_passive_types).empty());
  EXPECT_FALSE(sync_prefs.IsManaged());
  EXPECT_TRUE(sync_prefs.GetEncryptionBootstrapToken().empty());
  EXPECT_TRUE(sync_prefs.GetAllMaxVersions().empty());
}

}  // namespace

}  // namespace browser_sync
