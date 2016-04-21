// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/backend_migrator.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/tracked_objects.h"
#include "components/sync_driver/data_type_manager_mock.h"
#include "components/sync_driver/fake_sync_service.h"
#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/directory.h"  // TODO(tim): Remove. Bug 131130.
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using sync_driver::DataTypeManager;
using sync_driver::DataTypeManagerMock;

namespace browser_sync {

using syncer::sessions::SyncSessionSnapshot;

class SyncBackendMigratorTest : public testing::Test {
 public:
  SyncBackendMigratorTest() { }
  virtual ~SyncBackendMigratorTest() { }

  virtual void SetUp() {
    test_user_share_.SetUp();
    Mock::VerifyAndClear(manager());
    preferred_types_.Put(syncer::BOOKMARKS);
    preferred_types_.Put(syncer::PREFERENCES);
    preferred_types_.Put(syncer::AUTOFILL);

    migrator_.reset(
        new BackendMigrator(
            "Profile0", test_user_share_.user_share(), service(), manager(),
            base::Closure()));
    SetUnsyncedTypes(syncer::ModelTypeSet());
  }

  virtual void TearDown() {
    migrator_.reset();
    test_user_share_.TearDown();
  }

  // Marks all types in |unsynced_types| as unsynced  and all other
  // types as synced.
  void SetUnsyncedTypes(syncer::ModelTypeSet unsynced_types) {
    syncer::WriteTransaction trans(FROM_HERE,
                                     test_user_share_.user_share());
    for (int i = syncer::FIRST_REAL_MODEL_TYPE;
         i < syncer::MODEL_TYPE_COUNT; ++i) {
      syncer::ModelType type = syncer::ModelTypeFromInt(i);
      sync_pb::DataTypeProgressMarker progress_marker;
      if (!unsynced_types.Has(type)) {
        progress_marker.set_token("dummy");
      }
      trans.GetDirectory()->SetDownloadProgress(type, progress_marker);
    }
  }

  void SendConfigureDone(DataTypeManager::ConfigureStatus status,
                         syncer::ModelTypeSet requested_types) {
    if (status == DataTypeManager::OK) {
      DataTypeManager::ConfigureResult result(status, requested_types);
      migrator_->OnConfigureDone(result);
    } else {
      DataTypeManager::ConfigureResult result(
          status,
          requested_types);
      migrator_->OnConfigureDone(result);
    }
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  sync_driver::SyncService* service() { return &service_; }
  DataTypeManagerMock* manager() { return &manager_; }
  syncer::ModelTypeSet preferred_types() { return preferred_types_; }
  BackendMigrator* migrator() { return migrator_.get(); }
  void RemovePreferredType(syncer::ModelType type) {
    preferred_types_.Remove(type);
  }

 private:
  base::MessageLoop message_loop_;
  syncer::ModelTypeSet preferred_types_;
  sync_driver::FakeSyncService service_;
  NiceMock<DataTypeManagerMock> manager_;
  syncer::TestUserShare test_user_share_;
  std::unique_ptr<BackendMigrator> migrator_;
};

class MockMigrationObserver : public MigrationObserver {
 public:
  virtual ~MockMigrationObserver() {}

  MOCK_METHOD0(OnMigrationStateChange, void());
};

// Test that in the normal case a migration does transition through each state
// and wind up back in IDLE.
TEST_F(SyncBackendMigratorTest, Sanity) {
  MockMigrationObserver migration_observer;
  migrator()->AddMigrationObserver(&migration_observer);
  EXPECT_CALL(migration_observer, OnMigrationStateChange()).Times(4);

  syncer::ModelTypeSet to_migrate, difference;
  to_migrate.Put(syncer::PREFERENCES);
  difference.Put(syncer::AUTOFILL);
  difference.Put(syncer::BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(
      *manager(),
      PurgeForMigration(_, syncer::CONFIGURE_REASON_MIGRATION)).Times(1);
  EXPECT_CALL(
      *manager(),
      Configure(_, syncer::CONFIGURE_REASON_MIGRATION)).Times(1);

  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(to_migrate);
  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(syncer::ModelTypeSet());
  SendConfigureDone(DataTypeManager::OK, preferred_types());
  EXPECT_EQ(BackendMigrator::IDLE, migrator()->state());

  migrator()->RemoveMigrationObserver(&migration_observer);
}

// Test that in the normal case with Nigori a migration transitions through
// each state and wind up back in IDLE.
TEST_F(SyncBackendMigratorTest, MigrateNigori) {
  syncer::ModelTypeSet to_migrate, difference;
  to_migrate.Put(syncer::NIGORI);
  difference.Put(syncer::AUTOFILL);
  difference.Put(syncer::BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));

  EXPECT_CALL(*manager(), PurgeForMigration(_,
      syncer::CONFIGURE_REASON_MIGRATION));

  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(to_migrate);
  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(syncer::ModelTypeSet());
  SendConfigureDone(DataTypeManager::OK, preferred_types());
  EXPECT_EQ(BackendMigrator::IDLE, migrator()->state());
}


// Test that the migrator waits for the data type manager to be idle before
// starting a migration.
TEST_F(SyncBackendMigratorTest, WaitToStart) {
  syncer::ModelTypeSet to_migrate;
  to_migrate.Put(syncer::PREFERENCES);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURING));
  EXPECT_CALL(*manager(), Configure(_, _)).Times(0);
  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::WAITING_TO_START, migrator()->state());

  Mock::VerifyAndClearExpectations(manager());
  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(),
              PurgeForMigration(_, syncer::CONFIGURE_REASON_MIGRATION));
  SetUnsyncedTypes(syncer::ModelTypeSet());
  SendConfigureDone(DataTypeManager::OK, syncer::ModelTypeSet());

  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());
}

// Test that the migrator can cope with a migration request while a migration
// is in progress.
TEST_F(SyncBackendMigratorTest, RestartMigration) {
  syncer::ModelTypeSet to_migrate1, to_migrate2, to_migrate_union, bookmarks;
  to_migrate1.Put(syncer::PREFERENCES);
  to_migrate2.Put(syncer::AUTOFILL);
  to_migrate_union.Put(syncer::PREFERENCES);
  to_migrate_union.Put(syncer::AUTOFILL);
  bookmarks.Put(syncer::BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(
      *manager(),
      PurgeForMigration(_, syncer::CONFIGURE_REASON_MIGRATION)).Times(2);
  migrator()->MigrateTypes(to_migrate1);

  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());
  migrator()->MigrateTypes(to_migrate2);

  const syncer::ModelTypeSet difference1 =
      Difference(preferred_types(), to_migrate1);

  Mock::VerifyAndClearExpectations(manager());
  EXPECT_CALL(
      *manager(),
      PurgeForMigration(_, syncer::CONFIGURE_REASON_MIGRATION)).Times(1);
  EXPECT_CALL(*manager(), Configure(_, syncer::CONFIGURE_REASON_MIGRATION))
      .Times(1);
  SetUnsyncedTypes(to_migrate1);
  SendConfigureDone(DataTypeManager::OK, difference1);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(to_migrate_union);
  SendConfigureDone(DataTypeManager::OK, bookmarks);
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator()->state());
}

// Test that an external invocation of Configure(...) during a migration results
// in a migration reattempt.
TEST_F(SyncBackendMigratorTest, InterruptedWhileDisablingTypes) {
  syncer::ModelTypeSet to_migrate;
  syncer::ModelTypeSet difference;
  to_migrate.Put(syncer::PREFERENCES);
  difference.Put(syncer::AUTOFILL);
  difference.Put(syncer::BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*manager(), PurgeForMigration(HasModelTypes(to_migrate),
      syncer::CONFIGURE_REASON_MIGRATION));
  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  Mock::VerifyAndClearExpectations(manager());
  EXPECT_CALL(*manager(), PurgeForMigration(HasModelTypes(to_migrate),
      syncer::CONFIGURE_REASON_MIGRATION));
  SetUnsyncedTypes(syncer::ModelTypeSet());
  SendConfigureDone(DataTypeManager::OK, preferred_types());

  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());
}

// Test that spurious OnConfigureDone events don't confuse the
// migrator while it's waiting for disabled types to have been purged
// from the sync db.
TEST_F(SyncBackendMigratorTest, WaitingForPurge) {
  syncer::ModelTypeSet to_migrate, difference;
  to_migrate.Put(syncer::PREFERENCES);
  to_migrate.Put(syncer::AUTOFILL);
  difference.Put(syncer::BOOKMARKS);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(
      *manager(),
      PurgeForMigration(_, syncer::CONFIGURE_REASON_MIGRATION)).Times(1);
  EXPECT_CALL(
      *manager(),
      Configure(_, syncer::CONFIGURE_REASON_MIGRATION)).Times(1);

  migrator()->MigrateTypes(to_migrate);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  syncer::ModelTypeSet prefs;
  prefs.Put(syncer::PREFERENCES);
  SetUnsyncedTypes(prefs);
  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::DISABLING_TYPES, migrator()->state());

  SetUnsyncedTypes(to_migrate);
  SendConfigureDone(DataTypeManager::OK, difference);
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator()->state());
}

TEST_F(SyncBackendMigratorTest, MigratedTypeDisabledByUserDuringMigration) {
  syncer::ModelTypeSet to_migrate;
  to_migrate.Put(syncer::PREFERENCES);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(
      *manager(),
      PurgeForMigration(_, syncer::CONFIGURE_REASON_MIGRATION)).Times(1);
  EXPECT_CALL(
      *manager(),
      Configure(_, syncer::CONFIGURE_REASON_MIGRATION)).Times(1);
  migrator()->MigrateTypes(to_migrate);

  RemovePreferredType(syncer::PREFERENCES);
  SetUnsyncedTypes(to_migrate);
  SendConfigureDone(DataTypeManager::OK, preferred_types());
  EXPECT_EQ(BackendMigrator::REENABLING_TYPES, migrator()->state());
  SetUnsyncedTypes(syncer::ModelTypeSet());
  SendConfigureDone(DataTypeManager::OK, preferred_types());
  EXPECT_EQ(BackendMigrator::IDLE, migrator()->state());
}

TEST_F(SyncBackendMigratorTest, ConfigureFailure) {
  syncer::ModelTypeSet to_migrate;
  to_migrate.Put(syncer::PREFERENCES);

  EXPECT_CALL(*manager(), state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(
      *manager(),
      PurgeForMigration(_, syncer::CONFIGURE_REASON_MIGRATION)).Times(1);
  migrator()->MigrateTypes(to_migrate);
  SetUnsyncedTypes(syncer::ModelTypeSet());
  SendConfigureDone(DataTypeManager::ABORTED, syncer::ModelTypeSet());
  EXPECT_EQ(BackendMigrator::IDLE, migrator()->state());
}

};  // namespace browser_sync
