// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/backend_migrator.h"

#include "chrome/browser/sync/glue/data_type_manager_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace browser_sync {

using sessions::ErrorCounters;
using sessions::SyncerStatus;
using sessions::SyncSessionSnapshot;

class BackendMigratorTest : public testing::Test {
 public:
  BackendMigratorTest() { }
  virtual ~BackendMigratorTest() { }

  virtual void SetUp() {
    Mock::VerifyAndClear(manager());
    Mock::VerifyAndClear(&service_);
    preferred_types_.insert(syncable::BOOKMARKS);
    preferred_types_.insert(syncable::PREFERENCES);
    preferred_types_.insert(syncable::AUTOFILL);

    ON_CALL(service_, GetPreferredDataTypes(_)).
        WillByDefault(SetArgumentPointee<0>(preferred_types_));
  }

  void ReturnEmptyProgressMarkersInSnapshot() {
    ReturnNonEmptyProgressMarkersInSnapshot(syncable::ModelTypeSet());
  }

  void ReturnNonEmptyProgressMarkersInSnapshot(
      const syncable::ModelTypeSet& for_types) {
    std::string download_progress_markers[syncable::MODEL_TYPE_COUNT];
    for (syncable::ModelTypeSet::const_iterator it = for_types.begin();
         it != for_types.end(); ++it) {
      download_progress_markers[*it] = "foobar";
    }

    snap_.reset(new SyncSessionSnapshot(SyncerStatus(), ErrorCounters(),
        0, false, syncable::ModelTypeBitSet(), download_progress_markers,
        false, false, 0, 0, false, sessions::SyncSourceInfo()));
    EXPECT_CALL(service_, GetLastSessionSnapshot())
        .WillOnce(Return(snap_.get()));
  }

  void SendConfigureDone(DataTypeManager::ConfigureResult result,
                         const syncable::ModelTypeSet& types) {
    DataTypeManager::ConfigureResultWithErrorLocation result_with_location(
        result, FROM_HERE, types);
    NotificationService::current()->Notify(
        NotificationType::SYNC_CONFIGURE_DONE,
        Source<DataTypeManager>(&manager_),
        Details<DataTypeManager::ConfigureResultWithErrorLocation>(
            &result_with_location));
  }

  ProfileSyncService* service() { return &service_; }
  DataTypeManagerMock* manager() { return &manager_; }
  const syncable::ModelTypeSet& preferred_types() { return preferred_types_; }
  void RemovePreferredType(syncable::ModelType type) {
    preferred_types_.erase(type);
    Mock::VerifyAndClear(&service_);
    ON_CALL(service_, GetPreferredDataTypes(_)).
        WillByDefault(SetArgumentPointee<0>(preferred_types_));
  }
 private:
  scoped_ptr<SyncSessionSnapshot> snap_;
  syncable::ModelTypeSet preferred_types_;
  NiceMock<ProfileSyncServiceMock> service_;
  NiceMock<DataTypeManagerMock> manager_;
};

// Test that in the normal case a migration does transition through each state
// and wind up back in IDLE.
TEST_F(BackendMigratorTest, Sanity) {
  BackendMigrator migrator(service(), manager());
  syncable::ModelTypeSet to_migrate, difference;
  to_migrate.insert(syncable::PREFERENCES);
  difference.insert(syncable::AUTOFILL);
  difference.insert(syncable::BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(_));

  migrator.MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator.state());

  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::WAITING_FOR_PURGE, migrator.state());

  ReturnEmptyProgressMarkersInSnapshot();
  EXPECT_CALL(*manager(), Configure(preferred_types()));
  migrator.OnStateChanged();
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator.state());

  SendConfigureDone(DataTypeManager::OK, preferred_types());
  EXPECT_EQ(BackendMigrator::IDLE, migrator.state());
}

// Test that the migrator waits for the data type manager to be idle before
// starting a migration.
TEST_F(BackendMigratorTest, WaitToStart) {
  BackendMigrator migrator(service(), manager());
  syncable::ModelTypeSet to_migrate;
  to_migrate.insert(syncable::PREFERENCES);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURING));
  EXPECT_CALL(*manager(), Configure(_)).Times(0);
  migrator.MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::WAITING_TO_START, migrator.state());

  Mock::VerifyAndClearExpectations(manager());
  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(_));
  SendConfigureDone(DataTypeManager::OK, syncable::ModelTypeSet());

  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator.state());
}

// Test that the migrator can cope with a migration request while a migration
// is in progress.
TEST_F(BackendMigratorTest, RestartMigration) {
  BackendMigrator migrator(service(), manager());
  syncable::ModelTypeSet to_migrate1, to_migrate2, bookmarks;
  to_migrate1.insert(syncable::PREFERENCES);
  to_migrate2.insert(syncable::AUTOFILL);
  bookmarks.insert(syncable::BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(_)).Times(1);
  migrator.MigrateTypes(to_migrate1);

  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator.state());
  migrator.MigrateTypes(to_migrate2);

  syncable::ModelTypeSet difference1;
  std::set_difference(preferred_types().begin(), preferred_types().end(),
                      to_migrate1.begin(), to_migrate1.end(),
                      std::inserter(difference1, difference1.end()));

  Mock::VerifyAndClearExpectations(manager());
  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(bookmarks));
  SendConfigureDone(DataTypeManager::OK, difference1);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator.state());

  SendConfigureDone(DataTypeManager::OK, bookmarks);
  EXPECT_EQ(BackendMigrator::WAITING_FOR_PURGE, migrator.state());
}

// Test that an external invocation of Configure(...) during a migration results
// in a migration reattempt.
TEST_F(BackendMigratorTest, InterruptedWhileDisablingTypes) {
  BackendMigrator migrator(service(), manager());
  syncable::ModelTypeSet to_migrate;
  syncable::ModelTypeSet difference;
  to_migrate.insert(syncable::PREFERENCES);
  difference.insert(syncable::AUTOFILL);
  difference.insert(syncable::BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(difference));
  migrator.MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator.state());

  Mock::VerifyAndClearExpectations(manager());
  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(difference));
  SendConfigureDone(DataTypeManager::OK, preferred_types());

  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator.state());
}

// Test that spurious OnStateChanged events don't confuse the migrator while
// it's waiting for disabled types to have been purged from the sync db.
TEST_F(BackendMigratorTest, WaitingForPurge) {
  BackendMigrator migrator(service(), manager());
  syncable::ModelTypeSet to_migrate, difference;
  to_migrate.insert(syncable::PREFERENCES);
  to_migrate.insert(syncable::AUTOFILL);
  difference.insert(syncable::BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(_));
  migrator.MigrateTypes(to_migrate);
  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::WAITING_FOR_PURGE, migrator.state());

  ReturnNonEmptyProgressMarkersInSnapshot(to_migrate);
  migrator.OnStateChanged();
  EXPECT_EQ(BackendMigrator::WAITING_FOR_PURGE, migrator.state());

  syncable::ModelTypeSet prefs;
  prefs.insert(syncable::PREFERENCES);
  ReturnNonEmptyProgressMarkersInSnapshot(prefs);
  migrator.OnStateChanged();
  EXPECT_EQ(BackendMigrator::WAITING_FOR_PURGE, migrator.state());

  syncable::ModelTypeSet bookmarks;
  bookmarks.insert(syncable::BOOKMARKS);
  ReturnNonEmptyProgressMarkersInSnapshot(bookmarks);
  EXPECT_CALL(*manager(), Configure(preferred_types()));
  migrator.OnStateChanged();
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator.state());
}

TEST_F(BackendMigratorTest, MigratedTypeDisabledByUserDuringMigration) {
  BackendMigrator migrator(service(), manager());
  syncable::ModelTypeSet to_migrate;
  to_migrate.insert(syncable::PREFERENCES);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(_));
  migrator.MigrateTypes(to_migrate);

  RemovePreferredType(syncable::PREFERENCES);
  SendConfigureDone(DataTypeManager::OK, preferred_types());
  EXPECT_EQ(BackendMigrator::WAITING_FOR_PURGE, migrator.state());

  Mock::VerifyAndClearExpectations(manager());
  ReturnEmptyProgressMarkersInSnapshot();
  EXPECT_CALL(*manager(), Configure(preferred_types()));
  migrator.OnStateChanged();

  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator.state());
  SendConfigureDone(DataTypeManager::OK, preferred_types());
  EXPECT_EQ(BackendMigrator::IDLE, migrator.state());
}

TEST_F(BackendMigratorTest, ConfigureFailure) {
  BackendMigrator migrator(service(), manager());
  syncable::ModelTypeSet to_migrate;
  to_migrate.insert(syncable::PREFERENCES);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(_)).Times(1);
  migrator.MigrateTypes(to_migrate);
  SendConfigureDone(DataTypeManager::ABORTED, syncable::ModelTypeSet());
  EXPECT_EQ(BackendMigrator::IDLE, migrator.state());
}

};  // namespace browser_sync
