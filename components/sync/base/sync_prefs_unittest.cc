// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_prefs.h"

#include <memory>

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_value_store.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

class SyncPrefsTest : public testing::Test {
 protected:
  SyncPrefsTest() {
    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    sync_prefs_ = std::make_unique<SyncPrefs>(&pref_service_);
  }

  base::MessageLoop loop_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<SyncPrefs> sync_prefs_;
};

// Verify that invalidation versions are persisted and loaded correctly.
TEST_F(SyncPrefsTest, InvalidationVersions) {
  std::map<ModelType, int64_t> versions;
  versions[BOOKMARKS] = 10;
  versions[SESSIONS] = 20;
  versions[PREFERENCES] = 30;

  sync_prefs_->UpdateInvalidationVersions(versions);

  std::map<ModelType, int64_t> versions2;
  sync_prefs_->GetInvalidationVersions(&versions2);

  EXPECT_EQ(versions.size(), versions2.size());
  for (auto map_iter : versions2) {
    EXPECT_EQ(versions[map_iter.first], map_iter.second);
  }
}

TEST_F(SyncPrefsTest, ShortPollInterval) {
  EXPECT_TRUE(sync_prefs_->GetShortPollInterval().is_zero());

  sync_prefs_->SetShortPollInterval(base::TimeDelta::FromMinutes(30));

  EXPECT_FALSE(sync_prefs_->GetShortPollInterval().is_zero());
  EXPECT_EQ(sync_prefs_->GetShortPollInterval().InMinutes(), 30);
}

TEST_F(SyncPrefsTest, LongPollInterval) {
  EXPECT_TRUE(sync_prefs_->GetLongPollInterval().is_zero());

  sync_prefs_->SetLongPollInterval(base::TimeDelta::FromMinutes(60));

  EXPECT_FALSE(sync_prefs_->GetLongPollInterval().is_zero());
  EXPECT_EQ(sync_prefs_->GetLongPollInterval().InMinutes(), 60);
}

class MockSyncPrefObserver : public SyncPrefObserver {
 public:
  MOCK_METHOD1(OnSyncManagedPrefChange, void(bool));
};

TEST_F(SyncPrefsTest, ObservedPrefs) {
  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  InSequence dummy;
  EXPECT_CALL(mock_sync_pref_observer, OnSyncManagedPrefChange(true));
  EXPECT_CALL(mock_sync_pref_observer, OnSyncManagedPrefChange(false));

  EXPECT_FALSE(sync_prefs_->IsManaged());

  sync_prefs_->AddSyncPrefObserver(&mock_sync_pref_observer);

  sync_prefs_->SetManagedForTest(true);
  EXPECT_TRUE(sync_prefs_->IsManaged());
  sync_prefs_->SetManagedForTest(false);
  EXPECT_FALSE(sync_prefs_->IsManaged());

  sync_prefs_->RemoveSyncPrefObserver(&mock_sync_pref_observer);
}

TEST_F(SyncPrefsTest, ClearPreferences) {
  EXPECT_FALSE(sync_prefs_->IsFirstSetupComplete());
  EXPECT_EQ(base::Time(), sync_prefs_->GetLastSyncedTime());
  EXPECT_TRUE(sync_prefs_->GetEncryptionBootstrapToken().empty());

  sync_prefs_->SetFirstSetupComplete();
  sync_prefs_->SetLastSyncedTime(base::Time::Now());
  sync_prefs_->SetEncryptionBootstrapToken("token");

  EXPECT_TRUE(sync_prefs_->IsFirstSetupComplete());
  EXPECT_NE(base::Time(), sync_prefs_->GetLastSyncedTime());
  EXPECT_EQ("token", sync_prefs_->GetEncryptionBootstrapToken());

  sync_prefs_->ClearPreferences();

  EXPECT_FALSE(sync_prefs_->IsFirstSetupComplete());
  EXPECT_EQ(base::Time(), sync_prefs_->GetLastSyncedTime());
  EXPECT_TRUE(sync_prefs_->GetEncryptionBootstrapToken().empty());
}

// -----------------------------------------------------------------------------
// Test that manipulate preferred data types.
// -----------------------------------------------------------------------------

class SyncPrefsDataTypesTest : public SyncPrefsTest,
                               public testing::WithParamInterface<bool> {
 protected:
  SyncPrefsDataTypesTest() : user_events_separate_pref_group_(GetParam()) {}

  ModelTypeSet GetPreferredDataTypes(ModelTypeSet registered_types) {
    return sync_prefs_->GetPreferredDataTypes(registered_types,
                                              user_events_separate_pref_group_);
  }

  void SetPreferredDataTypes(ModelTypeSet registered_types,
                             ModelTypeSet preferred_types) {
    return sync_prefs_->SetPreferredDataTypes(registered_types, preferred_types,
                                              user_events_separate_pref_group_);
  }

  const bool user_events_separate_pref_group_;
};

TEST_P(SyncPrefsDataTypesTest, Basic) {
  EXPECT_FALSE(sync_prefs_->IsFirstSetupComplete());
  sync_prefs_->SetFirstSetupComplete();
  EXPECT_TRUE(sync_prefs_->IsFirstSetupComplete());

  EXPECT_TRUE(sync_prefs_->IsSyncRequested());
  sync_prefs_->SetSyncRequested(false);
  EXPECT_FALSE(sync_prefs_->IsSyncRequested());
  sync_prefs_->SetSyncRequested(true);
  EXPECT_TRUE(sync_prefs_->IsSyncRequested());

  EXPECT_EQ(base::Time(), sync_prefs_->GetLastSyncedTime());
  const base::Time& now = base::Time::Now();
  sync_prefs_->SetLastSyncedTime(now);
  EXPECT_EQ(now, sync_prefs_->GetLastSyncedTime());

  EXPECT_TRUE(sync_prefs_->HasKeepEverythingSynced());
  sync_prefs_->SetKeepEverythingSynced(false);
  EXPECT_FALSE(sync_prefs_->HasKeepEverythingSynced());
  sync_prefs_->SetKeepEverythingSynced(true);
  EXPECT_TRUE(sync_prefs_->HasKeepEverythingSynced());

  EXPECT_TRUE(sync_prefs_->GetEncryptionBootstrapToken().empty());
  sync_prefs_->SetEncryptionBootstrapToken("token");
  EXPECT_EQ("token", sync_prefs_->GetEncryptionBootstrapToken());
}

TEST_P(SyncPrefsDataTypesTest, DefaultTypes) {
  sync_prefs_->SetKeepEverythingSynced(false);

  ModelTypeSet preferred_types = GetPreferredDataTypes(UserTypes());
  EXPECT_EQ(AlwaysPreferredUserTypes(), preferred_types);

  // Simulate an upgrade to delete directives + proxy tabs support. None of the
  // new types or their pref group types should be registering, ensuring they
  // don't have pref values.
  ModelTypeSet registered_types = UserTypes();
  registered_types.Remove(PROXY_TABS);
  registered_types.Remove(TYPED_URLS);
  registered_types.Remove(SESSIONS);
  registered_types.Remove(HISTORY_DELETE_DIRECTIVES);

  // Enable all other types.
  SetPreferredDataTypes(registered_types, registered_types);

  // Manually enable typed urls (to simulate the old world).
  pref_service_.SetBoolean(prefs::kSyncTypedUrls, true);

  // Proxy tabs should not be enabled (since sessions wasn't), but history
  // delete directives should (since typed urls was).
  preferred_types = GetPreferredDataTypes(UserTypes());
  EXPECT_FALSE(preferred_types.Has(PROXY_TABS));
  EXPECT_TRUE(preferred_types.Has(HISTORY_DELETE_DIRECTIVES));

  // Now manually enable sessions, which should result in proxy tabs also being
  // enabled. Also, manually disable typed urls, which should mean that history
  // delete directives are not enabled.
  pref_service_.SetBoolean(prefs::kSyncTypedUrls, false);
  pref_service_.SetBoolean(prefs::kSyncSessions, true);
  preferred_types = GetPreferredDataTypes(UserTypes());
  EXPECT_TRUE(preferred_types.Has(PROXY_TABS));
  EXPECT_FALSE(preferred_types.Has(HISTORY_DELETE_DIRECTIVES));
}

TEST_P(SyncPrefsDataTypesTest, PreferredTypesKeepEverythingSynced) {
  EXPECT_TRUE(sync_prefs_->HasKeepEverythingSynced());

  const ModelTypeSet user_types = UserTypes();
  EXPECT_EQ(user_types, GetPreferredDataTypes(user_types));
  const ModelTypeSet user_visible_types = UserSelectableTypes();
  for (ModelType type : user_visible_types) {
    ModelTypeSet preferred_types;
    preferred_types.Put(type);
    SetPreferredDataTypes(user_types, preferred_types);
    EXPECT_EQ(user_types, GetPreferredDataTypes(user_types));
  }
}

TEST_P(SyncPrefsDataTypesTest, PreferredTypesNotKeepEverythingSynced) {
  sync_prefs_->SetKeepEverythingSynced(false);

  const ModelTypeSet user_types = UserTypes();
  EXPECT_NE(user_types, GetPreferredDataTypes(user_types));
  const ModelTypeSet user_visible_types = UserSelectableTypes();
  for (ModelType type : user_visible_types) {
    ModelTypeSet preferred_types;
    preferred_types.Put(type);
    ModelTypeSet expected_preferred_types(preferred_types);
    if (type == AUTOFILL) {
      expected_preferred_types.Put(AUTOFILL_PROFILE);
      expected_preferred_types.Put(AUTOFILL_WALLET_DATA);
      expected_preferred_types.Put(AUTOFILL_WALLET_METADATA);
    }
    if (type == PREFERENCES) {
      expected_preferred_types.Put(DICTIONARY);
      expected_preferred_types.Put(PRIORITY_PREFERENCES);
      expected_preferred_types.Put(SEARCH_ENGINES);
    }
    if (type == APPS) {
      expected_preferred_types.Put(APP_LIST);
      expected_preferred_types.Put(APP_NOTIFICATIONS);
      expected_preferred_types.Put(APP_SETTINGS);
      expected_preferred_types.Put(ARC_PACKAGE);
      expected_preferred_types.Put(READING_LIST);
    }
    if (type == EXTENSIONS) {
      expected_preferred_types.Put(EXTENSION_SETTINGS);
    }
    if (type == TYPED_URLS) {
      expected_preferred_types.Put(HISTORY_DELETE_DIRECTIVES);
      expected_preferred_types.Put(SESSIONS);
      expected_preferred_types.Put(FAVICON_IMAGES);
      expected_preferred_types.Put(FAVICON_TRACKING);
      if (!user_events_separate_pref_group_) {
        expected_preferred_types.Put(USER_EVENTS);
      }
    }
    if (type == PROXY_TABS) {
      expected_preferred_types.Put(SESSIONS);
      expected_preferred_types.Put(FAVICON_IMAGES);
      expected_preferred_types.Put(FAVICON_TRACKING);
    }

    expected_preferred_types.PutAll(AlwaysPreferredUserTypes());

    SetPreferredDataTypes(user_types, preferred_types);
    EXPECT_EQ(expected_preferred_types, GetPreferredDataTypes(user_types));
  }
}

// Device info should always be enabled.
TEST_P(SyncPrefsDataTypesTest, DeviceInfo) {
  EXPECT_TRUE(GetPreferredDataTypes(UserTypes()).Has(DEVICE_INFO));
  sync_prefs_->SetKeepEverythingSynced(true);
  EXPECT_TRUE(GetPreferredDataTypes(UserTypes()).Has(DEVICE_INFO));
  sync_prefs_->SetKeepEverythingSynced(false);
  EXPECT_TRUE(GetPreferredDataTypes(UserTypes()).Has(DEVICE_INFO));
  SetPreferredDataTypes(
      /*registered_types=*/ModelTypeSet(DEVICE_INFO),
      /*preferred_types=*/ModelTypeSet());
  EXPECT_TRUE(GetPreferredDataTypes(UserTypes()).Has(DEVICE_INFO));
}

// User Consents should always be enabled.
TEST_P(SyncPrefsDataTypesTest, UserConsents) {
  EXPECT_TRUE(GetPreferredDataTypes(UserTypes()).Has(USER_CONSENTS));
  sync_prefs_->SetKeepEverythingSynced(true);
  EXPECT_TRUE(GetPreferredDataTypes(UserTypes()).Has(USER_CONSENTS));
  sync_prefs_->SetKeepEverythingSynced(false);
  EXPECT_TRUE(GetPreferredDataTypes(UserTypes()).Has(USER_CONSENTS));
  SetPreferredDataTypes(
      /*registered_types=*/ModelTypeSet(USER_CONSENTS),
      /*preferred_types=*/ModelTypeSet());
  EXPECT_TRUE(GetPreferredDataTypes(UserTypes()).Has(USER_CONSENTS));
}

INSTANTIATE_TEST_CASE_P(,
                        SyncPrefsDataTypesTest,
                        ::testing::Values(false, true));

}  // namespace

}  // namespace syncer
