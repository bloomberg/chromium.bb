// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/sync_prefs.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_notifier_impl.h"
#include "base/prefs/pref_value_store.h"
#include "base/prefs/testing_pref_service.h"
#include "base/time/time.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "components/sync_driver/pref_names.h"
#include "sync/internal_api/public/base/model_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_driver {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

class SyncPrefsTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
  }

  user_prefs::TestingPrefServiceSyncable pref_service_;

 private:
  base::MessageLoop loop_;
};

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

TEST_F(SyncPrefsTest, DefaultTypes) {
  SyncPrefs sync_prefs(&pref_service_);
  sync_prefs.SetKeepEverythingSynced(false);

  // Only bookmarks are enabled by default.
  syncer::ModelTypeSet preferred_types =
      sync_prefs.GetPreferredDataTypes(syncer::UserTypes());
  EXPECT_TRUE(preferred_types.Equals(syncer::ModelTypeSet(syncer::BOOKMARKS)));

  // Simulate an upgrade to delete directives + proxy tabs support. None of the
  // new types or their pref group types should be registering, ensuring they
  // don't have pref values.
  syncer::ModelTypeSet registered_types = syncer::UserTypes();
  registered_types.Remove(syncer::PROXY_TABS);
  registered_types.Remove(syncer::TYPED_URLS);
  registered_types.Remove(syncer::SESSIONS);
  registered_types.Remove(syncer::HISTORY_DELETE_DIRECTIVES);

  // Enable all other types.
  sync_prefs.SetPreferredDataTypes(registered_types, registered_types);

  // Manually enable typed urls (to simulate the old world).
  pref_service_.SetBoolean(prefs::kSyncTypedUrls, true);

  // Proxy tabs should not be enabled (since sessions wasn't), but history
  // delete directives should (since typed urls was).
  preferred_types = sync_prefs.GetPreferredDataTypes(syncer::UserTypes());
  EXPECT_FALSE(preferred_types.Has(syncer::PROXY_TABS));
  EXPECT_TRUE(preferred_types.Has(syncer::HISTORY_DELETE_DIRECTIVES));

  // Now manually enable sessions, which should result in proxy tabs also being
  // enabled. Also, manually disable typed urls, which should mean that history
  // delete directives are not enabled.
  pref_service_.SetBoolean(prefs::kSyncTypedUrls, false);
  pref_service_.SetBoolean(prefs::kSyncSessions, true);
  preferred_types = sync_prefs.GetPreferredDataTypes(syncer::UserTypes());
  EXPECT_TRUE(preferred_types.Has(syncer::PROXY_TABS));
  EXPECT_FALSE(preferred_types.Has(syncer::HISTORY_DELETE_DIRECTIVES));
}

TEST_F(SyncPrefsTest, PreferredTypesKeepEverythingSynced) {
  SyncPrefs sync_prefs(&pref_service_);

  EXPECT_TRUE(sync_prefs.HasKeepEverythingSynced());

  const syncer::ModelTypeSet user_types = syncer::UserTypes();
  EXPECT_TRUE(user_types.Equals(sync_prefs.GetPreferredDataTypes(user_types)));
  const syncer::ModelTypeSet user_visible_types = syncer::UserSelectableTypes();
  for (syncer::ModelTypeSet::Iterator it = user_visible_types.First();
       it.Good();
       it.Inc()) {
    syncer::ModelTypeSet preferred_types;
    preferred_types.Put(it.Get());
    sync_prefs.SetPreferredDataTypes(user_types, preferred_types);
    EXPECT_TRUE(
        user_types.Equals(sync_prefs.GetPreferredDataTypes(user_types)));
  }
}

TEST_F(SyncPrefsTest, PreferredTypesNotKeepEverythingSynced) {
  SyncPrefs sync_prefs(&pref_service_);

  sync_prefs.SetKeepEverythingSynced(false);

  const syncer::ModelTypeSet user_types = syncer::UserTypes();
  EXPECT_FALSE(user_types.Equals(sync_prefs.GetPreferredDataTypes(user_types)));
  const syncer::ModelTypeSet user_visible_types = syncer::UserSelectableTypes();
  for (syncer::ModelTypeSet::Iterator it = user_visible_types.First();
       it.Good();
       it.Inc()) {
    syncer::ModelTypeSet preferred_types;
    preferred_types.Put(it.Get());
    syncer::ModelTypeSet expected_preferred_types(preferred_types);
    if (it.Get() == syncer::AUTOFILL) {
      expected_preferred_types.Put(syncer::AUTOFILL_PROFILE);
    }
    if (it.Get() == syncer::PREFERENCES) {
      expected_preferred_types.Put(syncer::DICTIONARY);
      expected_preferred_types.Put(syncer::PRIORITY_PREFERENCES);
      expected_preferred_types.Put(syncer::SEARCH_ENGINES);
    }
    if (it.Get() == syncer::APPS) {
      expected_preferred_types.Put(syncer::APP_LIST);
      expected_preferred_types.Put(syncer::APP_NOTIFICATIONS);
      expected_preferred_types.Put(syncer::APP_SETTINGS);
    }
    if (it.Get() == syncer::EXTENSIONS) {
      expected_preferred_types.Put(syncer::EXTENSION_SETTINGS);
    }
    if (it.Get() == syncer::TYPED_URLS) {
      expected_preferred_types.Put(syncer::HISTORY_DELETE_DIRECTIVES);
      expected_preferred_types.Put(syncer::SESSIONS);
      expected_preferred_types.Put(syncer::FAVICON_IMAGES);
      expected_preferred_types.Put(syncer::FAVICON_TRACKING);
    }
    if (it.Get() == syncer::PROXY_TABS) {
      expected_preferred_types.Put(syncer::SESSIONS);
      expected_preferred_types.Put(syncer::FAVICON_IMAGES);
      expected_preferred_types.Put(syncer::FAVICON_TRACKING);
    }
    sync_prefs.SetPreferredDataTypes(user_types, preferred_types);
    EXPECT_TRUE(expected_preferred_types.Equals(
        sync_prefs.GetPreferredDataTypes(user_types)));
  }
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

  syncer::ModelTypeSet expected_acknowledge_synced_types =
      sync_prefs.GetAcknowledgeSyncedTypesForTest();
  for (int i = syncer::EXTENSION_SETTINGS; i < syncer::MODEL_TYPE_COUNT; ++i) {
    const syncer::ModelType type = syncer::ModelTypeFromInt(i);
    syncer::ModelTypeSet acknowledge_synced_types(type);
    expected_acknowledge_synced_types.Put(type);
    sync_prefs.AcknowledgeSyncedTypes(acknowledge_synced_types);
    EXPECT_TRUE(expected_acknowledge_synced_types.Equals(
        sync_prefs.GetAcknowledgeSyncedTypesForTest()));
  }
}

TEST_F(SyncPrefsTest, ClearPreferences) {
  SyncPrefs sync_prefs(&pref_service_);

  EXPECT_FALSE(sync_prefs.HasSyncSetupCompleted());
  EXPECT_EQ(base::Time(), sync_prefs.GetLastSyncedTime());
  EXPECT_TRUE(sync_prefs.GetEncryptionBootstrapToken().empty());

  sync_prefs.SetSyncSetupCompleted();
  sync_prefs.SetLastSyncedTime(base::Time::Now());
  sync_prefs.SetEncryptionBootstrapToken("token");

  EXPECT_TRUE(sync_prefs.HasSyncSetupCompleted());
  EXPECT_NE(base::Time(), sync_prefs.GetLastSyncedTime());
  EXPECT_EQ("token", sync_prefs.GetEncryptionBootstrapToken());

  sync_prefs.ClearPreferences();

  EXPECT_FALSE(sync_prefs.HasSyncSetupCompleted());
  EXPECT_EQ(base::Time(), sync_prefs.GetLastSyncedTime());
  EXPECT_TRUE(sync_prefs.GetEncryptionBootstrapToken().empty());
}

}  // namespace

}  // namespace sync_driver
