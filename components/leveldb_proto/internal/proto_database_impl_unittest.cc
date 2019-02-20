// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/proto_database_impl.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "components/leveldb_proto/internal/shared_proto_database_provider.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/testing/proto/test_db.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace leveldb_proto {

namespace {

const std::string kDefaultClientName = "client";

}  // namespace

// Class used to shortcut the Init method so it returns a specified InitStatus.
class TestSharedProtoDatabase : public SharedProtoDatabase {
 public:
  TestSharedProtoDatabase(const std::string& client_db_id,
                          const base::FilePath& db_dir,
                          Enums::InitStatus use_status)
      : SharedProtoDatabase::SharedProtoDatabase(client_db_id, db_dir) {
    use_status_ = use_status;
  }

  void Init(
      bool create_if_missing,
      const std::string& client_db_id,
      SharedClientInitCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {
    init_status_ = use_status_;

    CheckCorruptionAndRunInitCallback(client_db_id, std::move(callback),
                                      std::move(callback_task_runner),
                                      use_status_);
  }

 private:
  ~TestSharedProtoDatabase() override {}

  Enums::InitStatus use_status_;
};

class TestSharedProtoDatabaseClient : public SharedProtoDatabaseClient {
 public:
  using SharedProtoDatabaseClient::set_migration_status;
  using SharedProtoDatabaseClient::SharedProtoDatabaseClient;

  TestSharedProtoDatabaseClient(scoped_refptr<SharedProtoDatabase> shared_db)
      : SharedProtoDatabaseClient::SharedProtoDatabaseClient(
            nullptr,
            ProtoDbType::TEST_DATABASE1,
            shared_db) {}

  void UpdateClientInitMetadata(
      SharedDBMetadataProto::MigrationStatus migration_status) override {
    set_migration_status(migration_status);
  }
};

class TestProtoDatabaseProvider : public ProtoDatabaseProvider {
 public:
  TestProtoDatabaseProvider(const base::FilePath& profile_dir)
      : ProtoDatabaseProvider(profile_dir) {}
  TestProtoDatabaseProvider(const base::FilePath& profile_dir,
                            const scoped_refptr<SharedProtoDatabase>& shared_db)
      : ProtoDatabaseProvider(profile_dir) {
    shared_db_ = shared_db;
  }

  void GetSharedDBInstance(
      ProtoDatabaseProvider::GetSharedDBInstanceCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), shared_db_));
  }

 private:
  scoped_refptr<SharedProtoDatabase> shared_db_;
};

class TestSharedProtoDatabaseProvider : public SharedProtoDatabaseProvider {
 public:
  TestSharedProtoDatabaseProvider(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::WeakPtr<ProtoDatabaseProvider> provider_weak_ptr)
      : SharedProtoDatabaseProvider(std::move(task_runner),
                                    std::move(provider_weak_ptr)) {}
};

class ProtoDatabaseImplTest : public testing::Test {
 public:
  void SetUp() override {
    temp_dir_ = std::make_unique<base::ScopedTempDir>();
    ASSERT_TRUE(temp_dir_->CreateUniqueTempDir());
    shared_db_temp_dir_ = std::make_unique<base::ScopedTempDir>();
    ASSERT_TRUE(shared_db_temp_dir_->CreateUniqueTempDir());
    test_thread_ = std::make_unique<base::Thread>("test_thread");
    ASSERT_TRUE(test_thread_->Start());
    shared_db_ = base::WrapRefCounted(new SharedProtoDatabase(
        kDefaultClientName, shared_db_temp_dir_->GetPath()));
  }

  void TearDown() override {
    temp_dir_.reset();
    shared_db_temp_dir_.reset();
  }

  std::unique_ptr<ProtoDatabaseImpl<TestProto>> CreateWrapper(
      ProtoDbType db_type,
      const base::FilePath& db_dir,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      std::unique_ptr<SharedProtoDatabaseProvider> db_provider) {
    return std::make_unique<ProtoDatabaseImpl<TestProto>>(
        db_type, db_dir, task_runner, std::move(db_provider));
  }

  std::unique_ptr<TestProtoDatabaseProvider> CreateProviderNoSharedDB() {
    // Create a test provider with a test shared DB that will return invalid
    // operation when a client is requested, level_db returns invalid operation
    // when a database doesn't exist.
    return CreateProviderWithTestSharedDB(Enums::InitStatus::kInvalidOperation);
  }

  std::unique_ptr<TestProtoDatabaseProvider> CreateProviderWithSharedDB() {
    return std::make_unique<TestProtoDatabaseProvider>(
        shared_db_temp_dir_->GetPath(), shared_db_);
  }

  std::unique_ptr<TestProtoDatabaseProvider> CreateProviderWithTestSharedDB(
      Enums::InitStatus shared_client_init_status) {
    auto test_shared_db = base::WrapRefCounted(new TestSharedProtoDatabase(
        kDefaultClientName, shared_db_temp_dir_->GetPath(),
        shared_client_init_status));

    return std::make_unique<TestProtoDatabaseProvider>(
        shared_db_temp_dir_->GetPath(), test_shared_db);
  }

  std::unique_ptr<TestSharedProtoDatabaseProvider> CreateSharedProvider(
      TestProtoDatabaseProvider* db_provider) {
    return std::make_unique<TestSharedProtoDatabaseProvider>(
        GetTestThreadTaskRunner(), db_provider->weak_factory_.GetWeakPtr());
  }

  // Uses ProtoDatabaseImpl's 3 parameter Init to bypass the check that gets
  // |use_shared_db|'s value.
  void InitWrapper(ProtoDatabaseImpl<TestProto>* wrapper,
                   const std::string& client_name,
                   bool use_shared_db,
                   Callbacks::InitStatusCallback callback) {
    wrapper->Init(client_name, use_shared_db, std::move(callback));
  }

  void InitWrapperAndWait(ProtoDatabaseImpl<TestProto>* wrapper,
                          const std::string& client_name,
                          bool use_shared_db,
                          Enums::InitStatus expect_status) {
    base::RunLoop init_loop;
    InitWrapper(
        wrapper, client_name, use_shared_db,
        base::BindOnce(
            [](base::OnceClosure closure, Enums::InitStatus expect_status,
               Enums::InitStatus status) {
              ASSERT_EQ(status, expect_status);
              std::move(closure).Run();
            },
            init_loop.QuitClosure(), expect_status));
    init_loop.Run();
  }

  std::unique_ptr<TestSharedProtoDatabaseClient> GetSharedClient() {
    return std::make_unique<TestSharedProtoDatabaseClient>(shared_db_);
  }

  void CallOnGetSharedDBClientAndWait(
      std::unique_ptr<UniqueProtoDatabase> unique_db,
      Enums::InitStatus unique_db_status,
      bool use_shared_db,
      std::unique_ptr<SharedProtoDatabaseClient> shared_db_client,
      Enums::InitStatus shared_db_status,
      Enums::InitStatus expect_status) {
    base::RunLoop init_loop;

    scoped_refptr<ProtoDatabaseSelector> selector(new ProtoDatabaseSelector(
        ProtoDbType::TEST_DATABASE1, GetTestThreadTaskRunner(), nullptr));

    selector->OnGetSharedDBClient(
        std::move(unique_db), unique_db_status, use_shared_db,
        base::BindOnce(
            [](base::OnceClosure closure, Enums::InitStatus expect_status,
               Enums::InitStatus status) {
              ASSERT_EQ(status, expect_status);
              std::move(closure).Run();
            },
            init_loop.QuitClosure(), expect_status),
        std::move(shared_db_client), shared_db_status);

    init_loop.Run();

    // If the process succeeded then check that the selector has a database set.
    if (expect_status == Enums::InitStatus::kOK) {
      ASSERT_NE(selector->db_, nullptr);
    }
  }

  // Just uses each entry's key to fill out the id/data fields in TestProto as
  // well.
  void AddDataToWrapper(ProtoDatabaseImpl<TestProto>* wrapper,
                        std::vector<std::string>* entry_keys) {
    auto data_set =
        std::make_unique<std::vector<std::pair<std::string, TestProto>>>();
    for (const auto& key : *entry_keys) {
      TestProto proto;
      proto.set_id(key);
      proto.set_data(key);
      data_set->emplace_back(std::make_pair(key, proto));
    }

    base::RunLoop data_loop;
    wrapper->UpdateEntries(std::move(data_set),
                           std::make_unique<std::vector<std::string>>(),
                           base::BindOnce(
                               [](base::OnceClosure closure, bool success) {
                                 ASSERT_TRUE(success);
                                 std::move(closure).Run();
                               },
                               data_loop.QuitClosure()));
    data_loop.Run();
  }

  void VerifyDataInWrapper(ProtoDatabaseImpl<TestProto>* wrapper,
                           std::vector<std::string>* entry_keys) {
    base::RunLoop load_loop;
    wrapper->LoadKeysAndEntries(base::BindOnce(
        [](base::OnceClosure closure, std::vector<std::string>* entry_keys,
           bool success,
           std::unique_ptr<std::map<std::string, TestProto>> keys_entries) {
          ASSERT_TRUE(success);
          ASSERT_EQ(entry_keys->size(), keys_entries->size());

          for (const auto& key : *entry_keys) {
            ASSERT_TRUE(keys_entries->find(key) != keys_entries->end());
          }
          std::move(closure).Run();
        },
        load_loop.QuitClosure(), entry_keys));
    load_loop.Run();
  }

  void UpdateClientMetadata(
      SharedDBMetadataProto::MigrationStatus migration_status) {
    base::RunLoop init_wait;
    auto client = shared_db_->GetClientForTesting(
        ProtoDbType::TEST_DATABASE1, /*create_if_missing=*/true,
        base::BindOnce(
            [](base::OnceClosure closure, Enums::InitStatus status,
               SharedDBMetadataProto::MigrationStatus migration_status) {
              EXPECT_EQ(Enums::kOK, status);
              std::move(closure).Run();
            },
            init_wait.QuitClosure()));
    init_wait.Run();

    base::RunLoop wait_loop;
    shared_db_->UpdateClientMetadataAsync(
        client->client_db_id(), migration_status,
        base::BindOnce(
            [](base::OnceClosure closure, bool success) {
              EXPECT_TRUE(success);
              std::move(closure).Run();
            },
            wait_loop.QuitClosure()));
    wait_loop.Run();
  }

  SharedDBMetadataProto::MigrationStatus GetClientMigrationStatus() {
    SharedDBMetadataProto::MigrationStatus migration_status;
    base::RunLoop init_wait;
    auto client = shared_db_->GetClientForTesting(
        ProtoDbType::TEST_DATABASE1, /*create_if_missing=*/true,
        base::BindOnce(
            [](base::OnceClosure closure,
               SharedDBMetadataProto::MigrationStatus* output,
               Enums::InitStatus status,
               SharedDBMetadataProto::MigrationStatus migration_status) {
              EXPECT_EQ(Enums::kOK, status);
              *output = migration_status;
              std::move(closure).Run();
            },
            init_wait.QuitClosure(), &migration_status));
    init_wait.Run();

    return migration_status;
  }

  scoped_refptr<base::SequencedTaskRunner> GetTestThreadTaskRunner() {
    return test_thread_->task_runner();
  }

  base::FilePath temp_dir() { return temp_dir_->GetPath(); }

 private:
  std::unique_ptr<base::ScopedTempDir> temp_dir_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // Shared database.
  std::unique_ptr<base::Thread> test_thread_;
  std::unique_ptr<base::Thread> shared_db_thread_;
  scoped_refptr<SharedProtoDatabase> shared_db_;
  std::unique_ptr<base::ScopedTempDir> shared_db_temp_dir_;
};

TEST_F(ProtoDatabaseImplTest, FailsBothDatabases) {
  auto db_provider = CreateProviderNoSharedDB();
  auto shared_db_provider = CreateSharedProvider(db_provider.get());
  auto wrapper = CreateWrapper(ProtoDbType::TEST_DATABASE1, temp_dir(),
                               GetTestThreadTaskRunner(),
                               CreateSharedProvider(db_provider.get()));
  InitWrapperAndWait(wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kError);
}

TEST_F(ProtoDatabaseImplTest, Fails_UseShared_NoSharedDB) {
  auto unique_db =
      std::make_unique<UniqueProtoDatabase>(GetTestThreadTaskRunner());

  // If a shared DB is requested, and it fails to open for any reason then we
  // return a failure, the shared DB is opened using create_if_missing = true,
  // so we shouldn't get a missing DB.
  CallOnGetSharedDBClientAndWait(
      std::move(unique_db),  // Unique DB opens fine.
      Enums::InitStatus::kOK,
      true,                        // We should be using a shared DB.
      nullptr,                     // Shared DB failed to open.
      Enums::InitStatus::kError,   // Shared DB had an IO error.
      Enums::InitStatus::kError);  // Then the wrapper should return an error.

  CallOnGetSharedDBClientAndWait(
      std::move(unique_db),  // Unique DB opens fine.
      Enums::InitStatus::kOK,
      true,                                  // We should be using a shared DB.
      nullptr,                               // Shared DB failed to open.
      Enums::InitStatus::kInvalidOperation,  // Shared DB doesn't exist.
      Enums::InitStatus::kError);  // Then the wrapper should return an error.
}

TEST_F(ProtoDatabaseImplTest,
       SucceedsWithShared_UseShared_HasSharedDB_UniqueNotFound) {
  auto shared_db_client = GetSharedClient();

  // Migration status is not attempted.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED);

  // If we request a shared DB, the unique DB fails to open because it doesn't
  // exist and a migration hasn't been attempted then we return the shared DB
  // and we set the migration status to migrated to shared.
  CallOnGetSharedDBClientAndWait(
      nullptr,                               // Unique DB fails to open.
      Enums::InitStatus::kInvalidOperation,  // Unique DB doesn't exist.
      true,                                  // We should be using a shared DB.
      std::move(shared_db_client),           // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kOK);  // Then the wrapper should return the shared DB.
}

TEST_F(ProtoDatabaseImplTest, Fails_UseShared_HasSharedDB_UniqueHadIOError) {
  auto shared_db_client = GetSharedClient();

  // Migration status is not attempted.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED);

  // If we request a shared DB, the unique DB fails to open because of an IO
  // error and a migration hasn't been attempted then we throw an error, as the
  // unique DB could contain data yet to be migrated.
  CallOnGetSharedDBClientAndWait(
      nullptr,                      // Unique DB fails to open.
      Enums::InitStatus::kError,    // Unique DB had an IO error.
      true,                         // We should be using a shared DB.
      std::move(shared_db_client),  // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kError);  // Then the wrapper should return an error.
}

TEST_F(ProtoDatabaseImplTest,
       SuccedsWithShared_UseShared_HasSharedDB_DataWasMigratedToShared) {
  auto shared_db_client = GetSharedClient();

  // Database has been migrated to Shared.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL);

  // If we request a shared DB, the unique DB fails to open for any reason and
  // the data has been migrated to the shared DB then we can return the shared
  // DB safely.
  CallOnGetSharedDBClientAndWait(
      nullptr,                               // Unique DB fails to open.
      Enums::InitStatus::kInvalidOperation,  // Unique DB doesn't exist.
      true,                                  // We should be using a shared DB.
      std::move(shared_db_client),           // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kOK);  // Then the wrapper should use the shared DB.

  shared_db_client = GetSharedClient();

  // Data has been migrated to Shared, Unique DB still exists and should be
  // removed.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED);

  // This second scenario occurs when the unique DB is marked to be deleted, but
  // it fails to open, we should also return the unique DB without throwing an
  // error.
  CallOnGetSharedDBClientAndWait(
      nullptr,                      // Unique DB fails to open.
      Enums::InitStatus::kError,    // Unique DB had an IO error.
      true,                         // We should be using a shared DB.
      std::move(shared_db_client),  // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kOK);  // Then the wrapper should use the shared DB.
}

TEST_F(ProtoDatabaseImplTest,
       Fails_UseShared_HasSharedDB_DataWasMigratedToUnique) {
  auto shared_db_client = GetSharedClient();

  // Database has been migrated to Unique.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL);

  // If we request a shared DB, the unique DB fails to open for any reason and
  // the data has been migrated to the unique DB then we throw an error, as the
  // unique database may contain data.
  CallOnGetSharedDBClientAndWait(
      nullptr,                               // Unique DB fails to open.
      Enums::InitStatus::kInvalidOperation,  // Unique DB doesn't exist.
      true,                                  // We should be using a shared DB.
      std::move(shared_db_client),           // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kError);  // Then the wrapper should throw an error.

  shared_db_client = GetSharedClient();

  // Data has been migrated to Unique, but data still exists in Shared DB that
  // should be removed.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED);

  // This second scenario occurs when the Shared DB still contains data, we
  // should still throw an error.
  CallOnGetSharedDBClientAndWait(
      nullptr,                      // Unique DB fails to open.
      Enums::InitStatus::kError,    // Unique DB had an IO error.
      true,                         // We should be using a shared DB.
      std::move(shared_db_client),  // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kError);  // Then the wrapper should throw an error.
}

TEST_F(ProtoDatabaseImplTest,
       SucceedsWithShared_DontUseShared_HasSharedDB_DataWasMigratedToShared) {
  auto shared_db_client = GetSharedClient();

  // Database has been migrated to Shared.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL);

  // If we request a unique DB, the unique DB fails to open for any reason and
  // the data has been migrated to the shared DB then we use the Shared DB.
  CallOnGetSharedDBClientAndWait(
      nullptr,                               // Unique DB fails to open.
      Enums::InitStatus::kInvalidOperation,  // Unique DB doesn't exist.
      false,                                 // We should be using a unique DB.
      std::move(shared_db_client),           // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kOK);  // Then the wrapper should use the shared DB.

  shared_db_client = GetSharedClient();

  // Data has been migrated to Shared, but the Unique DB still exists and needs
  // to be deleted.
  shared_db_client->set_migration_status(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED);

  // This second scenario occurs when the unique database is marked to be
  // deleted, we should still use the shared DB.
  CallOnGetSharedDBClientAndWait(
      nullptr,                      // Unique DB fails to open.
      Enums::InitStatus::kError,    // Unique DB had an IO error.
      true,                         // We should be using a shared DB.
      std::move(shared_db_client),  // Shared DB opens fine.
      Enums::InitStatus::kOK,
      Enums::InitStatus::kOK);  // Then the wrapper should use the shared DB.
}

TEST_F(ProtoDatabaseImplTest,
       SucceedsWithUnique_DontUseShared_SharedDBNotFound) {
  auto unique_db =
      std::make_unique<UniqueProtoDatabase>(GetTestThreadTaskRunner());

  // If the shared DB client fails to open because it doesn't exist then we can
  // return the unique DB safely.
  CallOnGetSharedDBClientAndWait(
      std::move(unique_db),  // Unique DB opens fine.
      Enums::InitStatus::kOK,
      false,                                 // We should be using a unique DB.
      nullptr,                               // Shared DB failed to open.
      Enums::InitStatus::kInvalidOperation,  // Shared DB doesn't exist.
      Enums::InitStatus::kOK);  // Then the wrapper should return the unique DB.
}

TEST_F(ProtoDatabaseImplTest, Fails_DontUseShared_SharedDBFailed) {
  auto unique_db =
      std::make_unique<UniqueProtoDatabase>(GetTestThreadTaskRunner());

  // If the shared DB client fails to open because of an IO error then we
  // shouldn't return a database, as the shared DB could contain data not yet
  // migrated.
  CallOnGetSharedDBClientAndWait(
      std::move(unique_db),  // Unique DB opens fine.
      Enums::InitStatus::kOK,
      false,                       // We should be using a unique DB.
      nullptr,                     // Shared DB failed to open.
      Enums::InitStatus::kError,   // Shared DB had an IO error.
      Enums::InitStatus::kError);  // Then the wrapper should return an error.
}

TEST_F(ProtoDatabaseImplTest, Fails_UseShared_NoSharedDB_NoUniqueDB) {
  auto db_provider = CreateProviderNoSharedDB();
  auto wrapper = CreateWrapper(ProtoDbType::TEST_DATABASE1, temp_dir(),
                               GetTestThreadTaskRunner(),
                               CreateSharedProvider(db_provider.get()));
  InitWrapperAndWait(wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kError);
}

// Migration tests:
TEST_F(ProtoDatabaseImplTest, Migration_EmptyDBs_UniqueToShared) {
  // First we create a unique DB so our second pass has a unique DB available.
  auto db_provider_noshared = CreateProviderNoSharedDB();
  auto unique_wrapper = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_noshared.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  // Kill the wrapper so it doesn't have a lock on the DB anymore.
  unique_wrapper.reset();

  auto db_provider_withshared = CreateProviderWithSharedDB();
  auto shared_wrapper = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);

  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            GetClientMigrationStatus());
}

TEST_F(ProtoDatabaseImplTest, Migration_EmptyDBs_SharedToUnique) {
  // First we create a unique DB so our second pass has a unique DB available.
  auto db_provider = CreateProviderWithSharedDB();
  auto shared_wrapper = CreateWrapper(ProtoDbType::TEST_DATABASE1, temp_dir(),
                                      GetTestThreadTaskRunner(),
                                      CreateSharedProvider(db_provider.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);

  // As the unique DB doesn't exist then the wrapper sets the migration status
  // to migrated to shared.
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            GetClientMigrationStatus());

  auto unique_wrapper = CreateWrapper(ProtoDbType::TEST_DATABASE1, temp_dir(),
                                      GetTestThreadTaskRunner(),
                                      CreateSharedProvider(db_provider.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL,
            GetClientMigrationStatus());
}

TEST_F(ProtoDatabaseImplTest, Migration_UniqueToShared) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a unique DB so our second pass has a unique DB available.
  auto db_provider_noshared = CreateProviderNoSharedDB();
  auto unique_wrapper = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_noshared.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  AddDataToWrapper(unique_wrapper.get(), data_set.get());
  // Kill the wrapper so it doesn't have a lock on the DB anymore.
  unique_wrapper.reset();

  auto db_provider_withshared = CreateProviderWithSharedDB();
  auto shared_wrapper = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);
  VerifyDataInWrapper(shared_wrapper.get(), data_set.get());

  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            GetClientMigrationStatus());
}

TEST_F(ProtoDatabaseImplTest, Migration_SharedToUnique) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a shared DB so our second pass has a shared DB available.
  auto db_provider_withshared = CreateProviderWithSharedDB();
  auto shared_wrapper = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);
  AddDataToWrapper(shared_wrapper.get(), data_set.get());

  // As the unique DB doesn't exist then the wrapper sets the migration status
  // is set to migrated to shared.
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            GetClientMigrationStatus());

  auto unique_wrapper = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  VerifyDataInWrapper(unique_wrapper.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL,
            GetClientMigrationStatus());
}

TEST_F(ProtoDatabaseImplTest, Migration_UniqueToShared_UniqueObsolete) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a unique DB so our second pass has a unique DB available.
  auto db_provider_noshared = CreateProviderNoSharedDB();
  auto unique_wrapper = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_noshared.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  AddDataToWrapper(unique_wrapper.get(), data_set.get());
  // Kill the wrapper so it doesn't have a lock on the DB anymore.
  unique_wrapper.reset();

  UpdateClientMetadata(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED);

  auto db_provider_withshared = CreateProviderWithSharedDB();
  auto shared_wrapper = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);

  // Unique DB should be deleted in migration. So, shared DB should be clean.
  data_set->clear();
  VerifyDataInWrapper(shared_wrapper.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            GetClientMigrationStatus());
}

TEST_F(ProtoDatabaseImplTest, Migration_UniqueToShared_SharedObsolete) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a shared DB so our second pass has a shared DB available.
  auto db_provider_withshared = CreateProviderWithSharedDB();
  auto shared_wrapper = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);
  AddDataToWrapper(shared_wrapper.get(), data_set.get());

  // As there's no unique DB, the wrapper is going to set the state to migrated
  // to shared.
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            GetClientMigrationStatus());

  // Force create an unique DB, which was deleted by migration.
  auto db_provider_noshared = CreateProviderNoSharedDB();
  auto unique_wrapper = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_noshared.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  unique_wrapper.reset();

  UpdateClientMetadata(
      SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED);

  shared_wrapper.reset();
  db_provider_withshared = CreateProviderWithSharedDB();

  auto shared_wrapper1 = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper1.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);

  // Shared DB should be deleted in migration. So, shared DB should be clean.
  data_set->clear();
  VerifyDataInWrapper(shared_wrapper1.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            GetClientMigrationStatus());
}

TEST_F(ProtoDatabaseImplTest, Migration_SharedToUnique_SharedObsolete) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a shared DB so our second pass has a shared DB available.
  auto db_provider_withshared = CreateProviderWithSharedDB();
  auto shared_wrapper = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);
  AddDataToWrapper(shared_wrapper.get(), data_set.get());

  // As there's no Unique DB, the wrapper changes the migration status to
  // migrated to shared.
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            GetClientMigrationStatus());

  UpdateClientMetadata(
      SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED);

  auto unique_wrapper = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);

  // Shared DB should be deleted in migration. So, unique DB should be clean.
  data_set->clear();
  VerifyDataInWrapper(unique_wrapper.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL,
            GetClientMigrationStatus());
}

TEST_F(ProtoDatabaseImplTest, Migration_SharedToUnique_UniqueObsolete) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a shared DB so our second pass has a shared DB available.
  auto db_provider_noshared = CreateProviderNoSharedDB();
  auto unique_wrapper = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_noshared.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  AddDataToWrapper(unique_wrapper.get(), data_set.get());

  UpdateClientMetadata(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED);

  unique_wrapper.reset();

  auto db_provider_withshared = CreateProviderWithSharedDB();
  auto shared_wrapper = CreateWrapper(
      ProtoDbType::TEST_DATABASE1, temp_dir(), GetTestThreadTaskRunner(),
      CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);

  // Unique DB should be deleted in migration. So, unique DB should be clean.
  data_set->clear();
  VerifyDataInWrapper(shared_wrapper.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL,
            GetClientMigrationStatus());
}

}  // namespace leveldb_proto