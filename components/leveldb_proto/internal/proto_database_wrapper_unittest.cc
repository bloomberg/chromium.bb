// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/proto_database_wrapper.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "components/leveldb_proto/internal/shared_proto_database_provider.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/testing/proto/test_db.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace leveldb_proto {

namespace {

const std::string kDefaultNamespace = "namespace";
const std::string kDefaultTypePrefix = "prefix";
const std::string kDefaultClientName = "client";

}  // namespace

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

class ProtoDatabaseWrapperTest : public testing::Test {
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

  std::unique_ptr<ProtoDatabaseWrapper<TestProto>> CreateWrapper(
      const std::string& client_namespace,
      const std::string& type_prefix,
      const base::FilePath& db_dir,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      std::unique_ptr<SharedProtoDatabaseProvider> db_provider) {
    return std::make_unique<ProtoDatabaseWrapper<TestProto>>(
        client_namespace, type_prefix, db_dir, task_runner,
        std::move(db_provider));
  }

  std::unique_ptr<TestProtoDatabaseProvider> CreateProviderNoSharedDB() {
    return std::make_unique<TestProtoDatabaseProvider>(
        shared_db_temp_dir_->GetPath());
  }

  std::unique_ptr<TestProtoDatabaseProvider> CreateProviderWithSharedDB() {
    return std::make_unique<TestProtoDatabaseProvider>(
        shared_db_temp_dir_->GetPath(), shared_db_);
  }

  std::unique_ptr<TestSharedProtoDatabaseProvider> CreateSharedProvider(
      TestProtoDatabaseProvider* db_provider) {
    return std::make_unique<TestSharedProtoDatabaseProvider>(
        GetTestThreadTaskRunner(), db_provider->weak_factory_.GetWeakPtr());
  }

  // Uses ProtoDatabaseWrapper's 3 parameter Init to bypass the check that gets
  // |use_shared_db|'s value.
  void InitWrapper(ProtoDatabaseWrapper<TestProto>* wrapper,
                   const std::string& client_name,
                   bool use_shared_db,
                   Callbacks::InitStatusCallback callback) {
    wrapper->Init(client_name, use_shared_db, std::move(callback));
  }

  void InitWrapperAndWait(ProtoDatabaseWrapper<TestProto>* wrapper,
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

  // Just uses each entry's key to fill out the id/data fields in TestProto as
  // well.
  void AddDataToWrapper(ProtoDatabaseWrapper<TestProto>* wrapper,
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

  void VerifyDataInWrapper(ProtoDatabaseWrapper<TestProto>* wrapper,
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
    auto client = shared_db_->GetClientForTesting<TestProto>(
        kDefaultNamespace, kDefaultTypePrefix, /*create_if_missing=*/true,
        base::BindOnce(
            [](base::OnceClosure closure, Enums::InitStatus status,
               SharedDBMetadataProto::MigrationStatus migration_status) {
              EXPECT_EQ(Enums::kOK, status);
              EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
                        migration_status);
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
    auto client = shared_db_->GetClientForTesting<TestProto>(
        kDefaultNamespace, kDefaultTypePrefix, /*create_if_missing=*/true,
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

TEST_F(ProtoDatabaseWrapperTest, FailsBothDatabases) {
  auto db_provider = CreateProviderNoSharedDB();
  auto shared_db_provider = CreateSharedProvider(db_provider.get());
  auto wrapper = CreateWrapper(kDefaultNamespace, kDefaultTypePrefix,
                               temp_dir(), GetTestThreadTaskRunner(),
                               CreateSharedProvider(db_provider.get()));
  InitWrapperAndWait(wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kError);
}

TEST_F(ProtoDatabaseWrapperTest, SucceedsWithUnique_DontUseShared_NoSharedDB) {
  auto db_provider = CreateProviderNoSharedDB();
  auto shared_db_provider = CreateSharedProvider(db_provider.get());
  auto wrapper = CreateWrapper(kDefaultNamespace, kDefaultTypePrefix,
                               temp_dir(), GetTestThreadTaskRunner(),
                               CreateSharedProvider(db_provider.get()));
  InitWrapperAndWait(wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
}

TEST_F(ProtoDatabaseWrapperTest, Fails_UseShared_NoSharedDB_NoUniqueDB) {
  auto db_provider = CreateProviderNoSharedDB();
  auto wrapper = CreateWrapper(kDefaultNamespace, kDefaultTypePrefix,
                               temp_dir(), GetTestThreadTaskRunner(),
                               CreateSharedProvider(db_provider.get()));
  InitWrapperAndWait(wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kError);
}

TEST_F(ProtoDatabaseWrapperTest, SucceedsWithUnique_UseShared_NoSharedDB) {
  // First we create a unique DB so our second pass has a unique DB available.
  auto db_provider = CreateProviderNoSharedDB();
  auto unique_wrapper = CreateWrapper(kDefaultNamespace, kDefaultTypePrefix,
                                      temp_dir(), GetTestThreadTaskRunner(),
                                      CreateSharedProvider(db_provider.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  // Kill the wrapper so it doesn't have a lock on the DB anymore.
  unique_wrapper.reset();

  auto shared_wrapper = CreateWrapper(kDefaultNamespace, kDefaultTypePrefix,
                                      temp_dir(), GetTestThreadTaskRunner(),
                                      CreateSharedProvider(db_provider.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);
}

TEST_F(ProtoDatabaseWrapperTest, SucceedsWithShared_UseShared_HasSharedDB) {
  auto db_provider = CreateProviderWithSharedDB();
  auto wrapper = CreateWrapper(kDefaultNamespace, kDefaultTypePrefix,
                               temp_dir(), GetTestThreadTaskRunner(),
                               CreateSharedProvider(db_provider.get()));
  InitWrapperAndWait(wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);
}

TEST_F(ProtoDatabaseWrapperTest, SucceedsWithUnique_DontUseShared_HasSharedDB) {
  auto db_provider = CreateProviderWithSharedDB();
  auto wrapper = CreateWrapper(kDefaultNamespace, kDefaultTypePrefix,
                               temp_dir(), GetTestThreadTaskRunner(),
                               CreateSharedProvider(db_provider.get()));
  InitWrapperAndWait(wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
}

// Migration tests:
TEST_F(ProtoDatabaseWrapperTest, Migration_EmptyDBs_UniqueToShared) {
  // First we create a unique DB so our second pass has a unique DB available.
  auto db_provider_noshared = CreateProviderNoSharedDB();
  auto unique_wrapper =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_noshared.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  // Kill the wrapper so it doesn't have a lock on the DB anymore.
  unique_wrapper.reset();

  auto db_provider_withshared = CreateProviderWithSharedDB();
  auto shared_wrapper =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);

  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            GetClientMigrationStatus());
}

TEST_F(ProtoDatabaseWrapperTest, Migration_EmptyDBs_SharedToUnique) {
  // First we create a unique DB so our second pass has a unique DB available.
  auto db_provider = CreateProviderWithSharedDB();
  auto shared_wrapper = CreateWrapper(kDefaultNamespace, kDefaultTypePrefix,
                                      temp_dir(), GetTestThreadTaskRunner(),
                                      CreateSharedProvider(db_provider.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);
  EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
            GetClientMigrationStatus());

  auto unique_wrapper = CreateWrapper(kDefaultNamespace, kDefaultTypePrefix,
                                      temp_dir(), GetTestThreadTaskRunner(),
                                      CreateSharedProvider(db_provider.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL,
            GetClientMigrationStatus());
}

TEST_F(ProtoDatabaseWrapperTest, Migration_UniqueToShared) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a unique DB so our second pass has a unique DB available.
  auto db_provider_noshared = CreateProviderNoSharedDB();
  auto unique_wrapper =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_noshared.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  AddDataToWrapper(unique_wrapper.get(), data_set.get());
  // Kill the wrapper so it doesn't have a lock on the DB anymore.
  unique_wrapper.reset();

  auto db_provider_withshared = CreateProviderWithSharedDB();
  auto shared_wrapper =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);
  VerifyDataInWrapper(shared_wrapper.get(), data_set.get());

  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            GetClientMigrationStatus());
}

TEST_F(ProtoDatabaseWrapperTest, Migration_SharedToUnique) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a shared DB so our second pass has a shared DB available.
  auto db_provider_withshared = CreateProviderWithSharedDB();
  auto shared_wrapper =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);
  AddDataToWrapper(shared_wrapper.get(), data_set.get());

  EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
            GetClientMigrationStatus());

  auto unique_wrapper =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  VerifyDataInWrapper(unique_wrapper.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL,
            GetClientMigrationStatus());
}

TEST_F(ProtoDatabaseWrapperTest, Migration_UniqueToShared_UniqueObsolete) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a unique DB so our second pass has a unique DB available.
  auto db_provider_noshared = CreateProviderNoSharedDB();
  auto unique_wrapper =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_noshared.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  AddDataToWrapper(unique_wrapper.get(), data_set.get());
  // Kill the wrapper so it doesn't have a lock on the DB anymore.
  unique_wrapper.reset();

  UpdateClientMetadata(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED);

  auto db_provider_withshared = CreateProviderWithSharedDB();
  auto shared_wrapper =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);

  // Unique db should be deleted in migration. So, shared db should be clean.
  data_set->clear();
  VerifyDataInWrapper(shared_wrapper.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            GetClientMigrationStatus());
}

TEST_F(ProtoDatabaseWrapperTest, Migration_UniqueToShared_SharedObsolete) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a shared DB so our second pass has a shared DB available.
  auto db_provider_withshared = CreateProviderWithSharedDB();
  auto shared_wrapper =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);
  AddDataToWrapper(shared_wrapper.get(), data_set.get());

  // Force create an uniquedb, which was deleted by migration.
  auto db_provider_noshared = CreateProviderNoSharedDB();
  auto unique_wrapper =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_noshared.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  unique_wrapper.reset();
  EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
            GetClientMigrationStatus());

  UpdateClientMetadata(
      SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED);

  shared_wrapper.reset();
  db_provider_withshared = CreateProviderWithSharedDB();

  auto shared_wrapper1 =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper1.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);

  // Shared db should be deleted in migration. So, shared db should be clean.
  data_set->clear();
  VerifyDataInWrapper(shared_wrapper1.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL,
            GetClientMigrationStatus());
}

TEST_F(ProtoDatabaseWrapperTest, Migration_SharedToUnique_SharedObsolete) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a shared DB so our second pass has a shared DB available.
  auto db_provider_withshared = CreateProviderWithSharedDB();
  auto shared_wrapper =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, true,
                     Enums::InitStatus::kOK);
  AddDataToWrapper(shared_wrapper.get(), data_set.get());

  EXPECT_EQ(SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED,
            GetClientMigrationStatus());

  UpdateClientMetadata(
      SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED);

  auto unique_wrapper =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);

  // Shared db should be deleted in migration. So, unique db should be clean.
  data_set->clear();
  VerifyDataInWrapper(unique_wrapper.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL,
            GetClientMigrationStatus());
}

TEST_F(ProtoDatabaseWrapperTest, Migration_SharedToUnique_UniqueObsolete) {
  auto data_set = std::make_unique<std::vector<std::string>>();
  data_set->emplace_back("entry1");
  data_set->emplace_back("entry2");
  data_set->emplace_back("entry3");

  // First we create a shared DB so our second pass has a shared DB available.
  auto db_provider_noshared = CreateProviderNoSharedDB();
  auto unique_wrapper =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_noshared.get()));
  InitWrapperAndWait(unique_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);
  AddDataToWrapper(unique_wrapper.get(), data_set.get());

  UpdateClientMetadata(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED);

  unique_wrapper.reset();

  auto db_provider_withshared = CreateProviderWithSharedDB();
  auto shared_wrapper =
      CreateWrapper(kDefaultNamespace, kDefaultTypePrefix, temp_dir(),
                    GetTestThreadTaskRunner(),
                    CreateSharedProvider(db_provider_withshared.get()));
  InitWrapperAndWait(shared_wrapper.get(), kDefaultClientName, false,
                     Enums::InitStatus::kOK);

  // Unique db should be deleted in migration. So, unique db should be clean.
  data_set->clear();
  VerifyDataInWrapper(shared_wrapper.get(), data_set.get());
  EXPECT_EQ(SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL,
            GetClientMigrationStatus());
}

}  // namespace leveldb_proto