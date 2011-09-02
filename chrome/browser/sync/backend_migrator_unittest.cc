// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/backend_migrator.h"

#include "base/message_loop.h"
#include "base/tracked_objects.h"
#include "chrome/browser/sync/glue/data_type_manager_mock.h"
#include "chrome/browser/sync/internal_api/write_transaction.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/test/engine/test_user_share.h"
#include "chrome/common/chrome_notification_types.h"
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
    test_user_share_.SetUp();
    Mock::VerifyAndClear(manager());
    Mock::VerifyAndClear(&service_);
    preferred_types_.insert(syncable::BOOKMARKS);
    preferred_types_.insert(syncable::PREFERENCES);
    preferred_types_.insert(syncable::AUTOFILL);

    ON_CALL(service_, GetPreferredDataTypes(_)).
        WillByDefault(SetArgumentPointee<0>(preferred_types_));

    migrator_.reset(
        new BackendMigrator(
            "Profile0", test_user_share_.user_share(), service(), manager()));
    SetUnsyncedTypes(syncable::ModelTypeSet());
  }

  virtual void TearDown() {
    migrator_.reset();
    test_user_share_.TearDown();
  }

  // Marks all types in |unsynced_types| as unsynced  and all other
  // types as synced.
  void SetUnsyncedTypes(const syncable::ModelTypeSet& unsynced_types) {
    sync_api::WriteTransaction trans(FROM_HERE,
                                     test_user_share_.user_share());
    for (int i = syncable::FIRST_REAL_MODEL_TYPE;
         i < syncable::MODEL_TYPE_COUNT; ++i) {
      syncable::ModelType type = syncable::ModelTypeFromInt(i);
      sync_pb::DataTypeProgressMarker progress_marker;
      if (unsynced_types.count(type) == 0) {
        progress_marker.set_token("dummy");
      }
      trans.GetLookup()->SetDownloadProgress(type, progress_marker);
    }
  }

  void SendConfigureDone(DataTypeManager::ConfigureStatus status,
                         const syncable::ModelTypeSet& requested_types) {
    if (status == DataTypeManager::OK) {
      DataTypeManager::ConfigureResult result(status, requested_types);
      NotificationService::current()->Notify(
          chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
          Source<DataTypeManager>(&manager_),
          Details<const DataTypeManager::ConfigureResult>(&result));
    } else {
        DataTypeManager::ConfigureResult result(
            status,
            requested_types,
            syncable::ModelTypeSet(),
            FROM_HERE);
        NotificationService::current()->Notify(
            chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
            Source<DataTypeManager>(&manager_),
            Details<const DataTypeManager::ConfigureResult>(&result));
    }
    message_loop_.RunAllPending();
  }

  ProfileSyncService* service() { return &service_; }
  DataTypeManagerMock* manager() { return &manager_; }
  const syncable::ModelTypeSet& preferred_types() { return preferred_types_; }
  BackendMigrator* migrator() { return migrator_.get(); }
  void RemovePreferredType(syncable::ModelType type) {
    preferred_types_.erase(type);
    Mock::VerifyAndClear(&service_);
    ON_CALL(service_, GetPreferredDataTypes(_)).
        WillByDefault(SetArgumentPointee<0>(preferred_types_));
  }

 private:
  scoped_ptr<SyncSessionSnapshot> snap_;
  MessageLoop message_loop_;
  syncable::ModelTypeSet preferred_types_;
  NiceMock<ProfileSyncServiceMock> service_;
  NiceMock<DataTypeManagerMock> manager_;
  TestUserShare test_user_share_;
  scoped_ptr<BackendMigrator> migrator_;
};

class MockMigrationObserver : public MigrationObserver {
 public:
  virtual ~MockMigrationObserver() {}

  MOCK_METHOD0(OnMigrationStateChange, void());
};

// Test that in the normal case a migration does transition through each state
// and wind up back in IDLE.
TEST_F(BackendMigratorTest, Sanity) {
  MockMigrationObserver migration_observer;
  migrator()->AddMigrationObserver(&migration_observer);
  EXPECT_CALL(migration_observer, OnMigrationStateChange()).Times(4);

  syncable::ModelTypeSet to_migrate, difference;
  to_migrate.insert(syncable::PREFERENCES);
  difference.insert(syncable::AUTOFILL);
  difference.insert(syncable::BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(_, sync_api::CONFIGURE_REASON_MIGRATION))
      .Times(2);

  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(to_migrate);
  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(syncable::ModelTypeSet());
  SendConfigureDone(DataTypeManager::OK, preferred_types());
  EXPECT_EQ(BackendMigrator::IDLE, migrator()->state());

  migrator()->RemoveMigrationObserver(&migration_observer);
}

// Test that in the normal case with Nigori a migration transitions through
// each state and wind up back in IDLE.
TEST_F(BackendMigratorTest, MigrateNigori) {
  syncable::ModelTypeSet to_migrate, difference;
  to_migrate.insert(syncable::NIGORI);
  difference.insert(syncable::AUTOFILL);
  difference.insert(syncable::BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));

  EXPECT_CALL(*manager(), ConfigureWithoutNigori(_,
      sync_api::CONFIGURE_REASON_MIGRATION));

  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(to_migrate);
  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(syncable::ModelTypeSet());
  SendConfigureDone(DataTypeManager::OK, preferred_types());
  EXPECT_EQ(BackendMigrator::IDLE, migrator()->state());
}


// Test that the migrator waits for the data type manager to be idle before
// starting a migration.
TEST_F(BackendMigratorTest, WaitToStart) {
  syncable::ModelTypeSet to_migrate;
  to_migrate.insert(syncable::PREFERENCES);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURING));
  EXPECT_CALL(*manager(), Configure(_, _)).Times(0);
  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::WAITING_TO_START, migrator()->state());

  Mock::VerifyAndClearExpectations(manager());
  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(_, sync_api::CONFIGURE_REASON_MIGRATION));
  SetUnsyncedTypes(syncable::ModelTypeSet());
  SendConfigureDone(DataTypeManager::OK, syncable::ModelTypeSet());

  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());
}

// Test that the migrator can cope with a migration request while a migration
// is in progress.
TEST_F(BackendMigratorTest, RestartMigration) {
  syncable::ModelTypeSet to_migrate1, to_migrate2, to_migrate_union, bookmarks;
  to_migrate1.insert(syncable::PREFERENCES);
  to_migrate2.insert(syncable::AUTOFILL);
  to_migrate_union.insert(syncable::PREFERENCES);
  to_migrate_union.insert(syncable::AUTOFILL);
  bookmarks.insert(syncable::BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(_, sync_api::CONFIGURE_REASON_MIGRATION))
              .Times(2);
  migrator()->MigrateTypes(to_migrate1);

  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());
  migrator()->MigrateTypes(to_migrate2);

  syncable::ModelTypeSet difference1;
  std::set_difference(preferred_types().begin(), preferred_types().end(),
                      to_migrate1.begin(), to_migrate1.end(),
                      std::inserter(difference1, difference1.end()));

  Mock::VerifyAndClearExpectations(manager());
  EXPECT_CALL(*manager(), Configure(_, sync_api::CONFIGURE_REASON_MIGRATION))
      .Times(2);
  SetUnsyncedTypes(to_migrate1);
  SendConfigureDone(DataTypeManager::OK, difference1);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(to_migrate_union);
  SendConfigureDone(DataTypeManager::OK, bookmarks);
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator()->state());
}

// Test that an external invocation of Configure(...) during a migration results
// in a migration reattempt.
TEST_F(BackendMigratorTest, InterruptedWhileDisablingTypes) {
  syncable::ModelTypeSet to_migrate;
  syncable::ModelTypeSet difference;
  to_migrate.insert(syncable::PREFERENCES);
  difference.insert(syncable::AUTOFILL);
  difference.insert(syncable::BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(difference,
      sync_api::CONFIGURE_REASON_MIGRATION));
  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  Mock::VerifyAndClearExpectations(manager());
  EXPECT_CALL(*manager(), Configure(difference,
      sync_api::CONFIGURE_REASON_MIGRATION));
  SetUnsyncedTypes(syncable::ModelTypeSet());
  SendConfigureDone(DataTypeManager::OK, preferred_types());

  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());
}

// Test that spurious OnConfigureDone events don't confuse the
// migrator while it's waiting for disabled types to have been purged
// from the sync db.
TEST_F(BackendMigratorTest, WaitingForPurge) {
  syncable::ModelTypeSet to_migrate, difference;
  to_migrate.insert(syncable::PREFERENCES);
  to_migrate.insert(syncable::AUTOFILL);
  difference.insert(syncable::BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(_, sync_api::CONFIGURE_REASON_MIGRATION))
      .Times(2);

  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  syncable::ModelTypeSet prefs;
  prefs.insert(syncable::PREFERENCES);
  SetUnsyncedTypes(prefs);
  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(to_migrate);
  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator()->state());
}

TEST_F(BackendMigratorTest, MigratedTypeDisabledByUserDuringMigration) {
  syncable::ModelTypeSet to_migrate;
  to_migrate.insert(syncable::PREFERENCES);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(_, sync_api::CONFIGURE_REASON_MIGRATION))
      .Times(2);
  migrator()->MigrateTypes(to_migrate);

  RemovePreferredType(syncable::PREFERENCES);
  SetUnsyncedTypes(to_migrate);
  SendConfigureDone(DataTypeManager::OK, preferred_types());
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator()->state());
  SetUnsyncedTypes(syncable::ModelTypeSet());
  SendConfigureDone(DataTypeManager::OK, preferred_types());
  EXPECT_EQ(BackendMigrator::IDLE, migrator()->state());
}

TEST_F(BackendMigratorTest, ConfigureFailure) {
  syncable::ModelTypeSet to_migrate;
  to_migrate.insert(syncable::PREFERENCES);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), Configure(_, sync_api::CONFIGURE_REASON_MIGRATION))
              .Times(1);
  migrator()->MigrateTypes(to_migrate);
  SetUnsyncedTypes(syncable::ModelTypeSet());
  SendConfigureDone(DataTypeManager::ABORTED, syncable::ModelTypeSet());
  EXPECT_EQ(BackendMigrator::IDLE, migrator()->state());
}

};  // namespace browser_sync
