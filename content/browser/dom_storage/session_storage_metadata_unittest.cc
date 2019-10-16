// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_metadata.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/services/leveldb/leveldb_database_impl.h"
#include "components/services/leveldb/public/cpp/util.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "content/browser/dom_storage/dom_storage_types.h"
#include "content/browser/dom_storage/session_storage_database.h"
#include "content/browser/indexed_db/leveldb/leveldb_env.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {
using leveldb::StdStringToUint8Vector;
using leveldb::Uint8VectorToStdString;

void ErrorCallback(leveldb::Status* status_out, leveldb::Status status) {
  *status_out = status;
}

class SessionStorageMetadataTest : public testing::Test {
 public:
  SessionStorageMetadataTest()
      : test_namespace1_id_(base::GenerateGUID()),
        test_namespace2_id_(base::GenerateGUID()),
        test_namespace3_id_(base::GenerateGUID()),
        test_origin1_(url::Origin::Create(GURL("http://host1:1/"))),
        test_origin2_(url::Origin::Create(GURL("http://host2:2/"))) {
    base::RunLoop loop;
    database_ = leveldb::LevelDBDatabaseImpl::OpenInMemory(
        base::nullopt, "SessionStorageMetadataTest",
        base::CreateSequencedTaskRunner({base::MayBlock(), base::ThreadPool()}),
        base::BindLambdaForTesting([&](leveldb::Status) { loop.Quit(); }));
    loop.Run();

    next_map_id_key_ = std::vector<uint8_t>(
        std::begin(SessionStorageMetadata::kNextMapIdKeyBytes),
        std::end(SessionStorageMetadata::kNextMapIdKeyBytes));
    database_version_key_ = std::vector<uint8_t>(
        std::begin(SessionStorageMetadata::kDatabaseVersionBytes),
        std::end(SessionStorageMetadata::kDatabaseVersionBytes));
    namespaces_prefix_key_ = std::vector<uint8_t>(
        std::begin(SessionStorageMetadata::kNamespacePrefixBytes),
        std::end(SessionStorageMetadata::kNamespacePrefixBytes));
  }
  ~SessionStorageMetadataTest() override {}

  void ReadMetadataFromDatabase(SessionStorageMetadata* metadata) {
    std::vector<uint8_t> version_value;
    std::vector<uint8_t> next_map_id_value;
    std::vector<storage::DomStorageDatabase::KeyValuePair> namespace_entries;

    base::RunLoop loop;
    database_->database().PostTaskWithThisObject(
        FROM_HERE,
        base::BindLambdaForTesting([&](const storage::DomStorageDatabase& db) {
          EXPECT_TRUE(db.Get(database_version_key_, &version_value).ok());
          EXPECT_TRUE(db.Get(next_map_id_key_, &next_map_id_value).ok());
          EXPECT_TRUE(
              db.GetPrefixed(namespaces_prefix_key_, &namespace_entries).ok());
          loop.Quit();
        }));
    loop.Run();

    std::vector<leveldb::mojom::BatchedOperationPtr> migration_operations;
    EXPECT_TRUE(
        metadata->ParseDatabaseVersion(version_value, &migration_operations));
    EXPECT_TRUE(migration_operations.empty());

    metadata->ParseNextMapId(next_map_id_value);

    EXPECT_TRUE(metadata->ParseNamespaces(std::move(namespace_entries),
                                          &migration_operations));
    EXPECT_TRUE(migration_operations.empty());
  }

  void SetupTestData() {
    // | key                                    | value              |
    // |----------------------------------------|--------------------|
    // | map-1-key1                             | data1              |
    // | map-3-key1                             | data3              |
    // | map-4-key1                             | data4              |
    // | namespace-<guid 1>-http://host1:1/     | 1                  |
    // | namespace-<guid 1>-http://host2:2/     | 3                  |
    // | namespace-<guid 2>-http://host1:1/     | 1                  |
    // | namespace-<guid 2>-http://host2:2/     | 4                  |
    // | next-map-id                            | 5                  |
    // | version                                | 1                  |
    base::RunLoop loop;
    database_->database().PostTaskWithThisObject(
        FROM_HERE,
        base::BindLambdaForTesting([&](const storage::DomStorageDatabase& db) {
          db.Put(StdStringToUint8Vector(std::string("namespace-") +
                                        test_namespace1_id_ + "-" +
                                        test_origin1_.GetURL().spec()),
                 StdStringToUint8Vector("1"));
          db.Put(StdStringToUint8Vector(std::string("namespace-") +
                                        test_namespace1_id_ + "-" +
                                        test_origin2_.GetURL().spec()),
                 StdStringToUint8Vector("3"));
          db.Put(StdStringToUint8Vector(std::string("namespace-") +
                                        test_namespace2_id_ + "-" +
                                        test_origin1_.GetURL().spec()),
                 StdStringToUint8Vector("1"));
          db.Put(StdStringToUint8Vector(std::string("namespace-") +
                                        test_namespace2_id_ + "-" +
                                        test_origin2_.GetURL().spec()),
                 StdStringToUint8Vector("4"));

          db.Put(next_map_id_key_, StdStringToUint8Vector("5"));

          db.Put(StdStringToUint8Vector("map-1-key1"),
                 StdStringToUint8Vector("data1"));
          db.Put(StdStringToUint8Vector("map-3-key1"),
                 StdStringToUint8Vector("data3"));
          db.Put(StdStringToUint8Vector("map-4-key1"),
                 StdStringToUint8Vector("data4"));

          db.Put(database_version_key_, StdStringToUint8Vector("1"));
          loop.Quit();
        }));
    loop.Run();
  }

  std::map<std::vector<uint8_t>, std::vector<uint8_t>> GetDatabaseContents() {
    std::vector<storage::DomStorageDatabase::KeyValuePair> entries;
    base::RunLoop loop;
    database_->database().PostTaskWithThisObject(
        FROM_HERE,
        base::BindLambdaForTesting([&](const storage::DomStorageDatabase& db) {
          leveldb::Status status = db.GetPrefixed({}, &entries);
          ASSERT_TRUE(status.ok());
          loop.Quit();
        }));
    loop.Run();

    std::map<std::vector<uint8_t>, std::vector<uint8_t>> contents;
    for (auto& entry : entries)
      contents.emplace(entry.key, entry.value);
    return contents;
  }

  void WriteBatch(std::vector<leveldb::mojom::BatchedOperationPtr> operations,
                  base::OnceCallback<void(leveldb::Status)> callback) {
    base::RunLoop loop;
    database_->Write(std::move(operations),
                     base::BindLambdaForTesting([&](leveldb::Status status) {
                       std::move(callback).Run(status);
                       loop.Quit();
                     }));
    loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::string test_namespace1_id_;
  std::string test_namespace2_id_;
  std::string test_namespace3_id_;
  url::Origin test_origin1_;
  url::Origin test_origin2_;
  std::unique_ptr<leveldb::LevelDBDatabaseImpl> database_;

  std::vector<uint8_t> database_version_key_;
  std::vector<uint8_t> next_map_id_key_;
  std::vector<uint8_t> namespaces_prefix_key_;
};

TEST_F(SessionStorageMetadataTest, SaveNewMetadata) {
  SessionStorageMetadata metadata;
  std::vector<leveldb::mojom::BatchedOperationPtr> operations =
      metadata.SetupNewDatabase();

  leveldb::Status status;
  WriteBatch(std::move(operations), base::BindOnce(&ErrorCallback, &status));
  EXPECT_TRUE(status.ok());

  auto contents = GetDatabaseContents();
  EXPECT_EQ(StdStringToUint8Vector("1"), contents[database_version_key_]);
  EXPECT_EQ(StdStringToUint8Vector("0"), contents[next_map_id_key_]);
}

TEST_F(SessionStorageMetadataTest, LoadingData) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  EXPECT_EQ(5, metadata.NextMapId());
  EXPECT_EQ(2ul, metadata.namespace_origin_map().size());

  // Namespace 1 should have 2 origins, referencing map 1 and 3. Map 1 is shared
  // between namespace 1 and namespace 2.
  auto entry = metadata.GetOrCreateNamespaceEntry(test_namespace1_id_);
  EXPECT_EQ(test_namespace1_id_, entry->first);
  EXPECT_EQ(2ul, entry->second.size());
  EXPECT_EQ(StdStringToUint8Vector("map-1-"),
            entry->second[test_origin1_]->KeyPrefix());
  EXPECT_EQ(2, entry->second[test_origin1_]->ReferenceCount());
  EXPECT_EQ(StdStringToUint8Vector("map-3-"),
            entry->second[test_origin2_]->KeyPrefix());
  EXPECT_EQ(1, entry->second[test_origin2_]->ReferenceCount());

  // Namespace 2 is the same, except the second origin references map 4.
  entry = metadata.GetOrCreateNamespaceEntry(test_namespace2_id_);
  EXPECT_EQ(test_namespace2_id_, entry->first);
  EXPECT_EQ(2ul, entry->second.size());
  EXPECT_EQ(StdStringToUint8Vector("map-1-"),
            entry->second[test_origin1_]->KeyPrefix());
  EXPECT_EQ(2, entry->second[test_origin1_]->ReferenceCount());
  EXPECT_EQ(StdStringToUint8Vector("map-4-"),
            entry->second[test_origin2_]->KeyPrefix());
  EXPECT_EQ(1, entry->second[test_origin2_]->ReferenceCount());
}

TEST_F(SessionStorageMetadataTest, SaveNewMap) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  std::vector<leveldb::mojom::BatchedOperationPtr> operations;
  auto ns1_entry = metadata.GetOrCreateNamespaceEntry(test_namespace1_id_);
  auto map_data =
      metadata.RegisterNewMap(ns1_entry, test_origin1_, &operations);
  ASSERT_TRUE(map_data);

  // Verify in-memory metadata is correct.
  EXPECT_EQ(StdStringToUint8Vector("map-5-"),
            ns1_entry->second[test_origin1_]->KeyPrefix());
  EXPECT_EQ(1, ns1_entry->second[test_origin1_]->ReferenceCount());
  EXPECT_EQ(1, metadata.GetOrCreateNamespaceEntry(test_namespace2_id_)
                   ->second[test_origin1_]
                   ->ReferenceCount());

  leveldb::Status status;
  WriteBatch(std::move(operations), base::BindOnce(&ErrorCallback, &status));
  EXPECT_TRUE(status.ok());

  // Verify metadata was written to disk.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(StdStringToUint8Vector("6"), contents[next_map_id_key_]);
  EXPECT_EQ(StdStringToUint8Vector("5"),
            contents[StdStringToUint8Vector(std::string("namespace-") +
                                            test_namespace1_id_ + "-" +
                                            test_origin1_.GetURL().spec())]);
}

TEST_F(SessionStorageMetadataTest, ShallowCopies) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  auto ns1_entry = metadata.GetOrCreateNamespaceEntry(test_namespace1_id_);
  auto ns3_entry = metadata.GetOrCreateNamespaceEntry(test_namespace3_id_);

  std::vector<leveldb::mojom::BatchedOperationPtr> operations;
  metadata.RegisterShallowClonedNamespace(ns1_entry, ns3_entry, &operations);

  leveldb::Status status;
  ;
  WriteBatch(std::move(operations), base::BindOnce(&ErrorCallback, &status));
  EXPECT_TRUE(status.ok());

  // Verify in-memory metadata is correct.
  EXPECT_EQ(StdStringToUint8Vector("map-1-"),
            ns3_entry->second[test_origin1_]->KeyPrefix());
  EXPECT_EQ(StdStringToUint8Vector("map-3-"),
            ns3_entry->second[test_origin2_]->KeyPrefix());
  EXPECT_EQ(ns1_entry->second[test_origin1_].get(),
            ns3_entry->second[test_origin1_].get());
  EXPECT_EQ(ns1_entry->second[test_origin2_].get(),
            ns3_entry->second[test_origin2_].get());
  EXPECT_EQ(3, ns3_entry->second[test_origin1_]->ReferenceCount());
  EXPECT_EQ(2, ns3_entry->second[test_origin2_]->ReferenceCount());

  // Verify metadata was written to disk.
  auto contents = GetDatabaseContents();
  EXPECT_EQ(StdStringToUint8Vector("1"),
            contents[StdStringToUint8Vector(std::string("namespace-") +
                                            test_namespace3_id_ + "-" +
                                            test_origin1_.GetURL().spec())]);
  EXPECT_EQ(StdStringToUint8Vector("3"),
            contents[StdStringToUint8Vector(std::string("namespace-") +
                                            test_namespace3_id_ + "-" +
                                            test_origin2_.GetURL().spec())]);
}

TEST_F(SessionStorageMetadataTest, DeleteNamespace) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  std::vector<leveldb::mojom::BatchedOperationPtr> operations;
  metadata.DeleteNamespace(test_namespace1_id_, &operations);
  leveldb::Status status;
  WriteBatch(std::move(operations), base::BindOnce(&ErrorCallback, &status));
  EXPECT_TRUE(status.ok());

  EXPECT_FALSE(
      base::Contains(metadata.namespace_origin_map(), test_namespace1_id_));

  // Verify in-memory metadata is correct.
  auto ns2_entry = metadata.GetOrCreateNamespaceEntry(test_namespace2_id_);
  EXPECT_EQ(1, ns2_entry->second[test_origin1_]->ReferenceCount());
  EXPECT_EQ(1, ns2_entry->second[test_origin2_]->ReferenceCount());

  // Verify metadata and data was deleted from disk.
  auto contents = GetDatabaseContents();
  EXPECT_FALSE(base::Contains(
      contents,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace1_id_ +
                             "-" + test_origin1_.GetURL().spec())));
  EXPECT_FALSE(base::Contains(
      contents,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace1_id_ +
                             "-" + test_origin2_.GetURL().spec())));
  EXPECT_FALSE(base::Contains(contents, StdStringToUint8Vector("map-3-key1")));
  EXPECT_TRUE(base::Contains(contents, StdStringToUint8Vector("map-1-key1")));
}

TEST_F(SessionStorageMetadataTest, DeleteArea) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  // First delete an area with a shared map.
  std::vector<leveldb::mojom::BatchedOperationPtr> operations;
  metadata.DeleteArea(test_namespace1_id_, test_origin1_, &operations);
  leveldb::Status status;
  WriteBatch(std::move(operations), base::BindOnce(&ErrorCallback, &status));
  EXPECT_TRUE(status.ok());

  // Verify in-memory metadata is correct.
  auto ns1_entry = metadata.GetOrCreateNamespaceEntry(test_namespace1_id_);
  auto ns2_entry = metadata.GetOrCreateNamespaceEntry(test_namespace2_id_);
  EXPECT_FALSE(base::Contains(ns1_entry->second, test_origin1_));
  EXPECT_EQ(1, ns1_entry->second[test_origin2_]->ReferenceCount());
  EXPECT_EQ(1, ns2_entry->second[test_origin1_]->ReferenceCount());
  EXPECT_EQ(1, ns2_entry->second[test_origin2_]->ReferenceCount());

  // Verify only the applicable data was deleted.
  auto contents = GetDatabaseContents();
  EXPECT_FALSE(base::Contains(
      contents,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace1_id_ +
                             "-" + test_origin1_.GetURL().spec())));
  EXPECT_TRUE(base::Contains(
      contents,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace1_id_ +
                             "-" + test_origin2_.GetURL().spec())));
  EXPECT_TRUE(base::Contains(contents, StdStringToUint8Vector("map-1-key1")));
  EXPECT_TRUE(base::Contains(contents, StdStringToUint8Vector("map-4-key1")));

  // Now delete an area with a unique map.
  operations.clear();
  metadata.DeleteArea(test_namespace2_id_, test_origin2_, &operations);
  WriteBatch(std::move(operations), base::BindOnce(&ErrorCallback, &status));
  EXPECT_TRUE(status.ok());

  // Verify in-memory metadata is correct.
  EXPECT_FALSE(base::Contains(ns1_entry->second, test_origin1_));
  EXPECT_EQ(1, ns1_entry->second[test_origin2_]->ReferenceCount());
  EXPECT_EQ(1, ns2_entry->second[test_origin1_]->ReferenceCount());
  EXPECT_FALSE(base::Contains(ns2_entry->second, test_origin2_));

  // Verify only the applicable data was deleted.
  contents = GetDatabaseContents();
  EXPECT_TRUE(base::Contains(
      contents,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace2_id_ +
                             "-" + test_origin1_.GetURL().spec())));
  EXPECT_FALSE(base::Contains(
      contents,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace2_id_ +
                             "-" + test_origin2_.GetURL().spec())));
  EXPECT_TRUE(base::Contains(contents, StdStringToUint8Vector("map-1-key1")));
  EXPECT_TRUE(base::Contains(contents, StdStringToUint8Vector("map-3-key1")));
  EXPECT_FALSE(base::Contains(contents, StdStringToUint8Vector("map-4-key1")));
}

class SessionStorageMetadataMigrationTest : public testing::Test {
 public:
  SessionStorageMetadataMigrationTest()
      : test_namespace1_id_(base::GenerateGUID()),
        test_namespace2_id_(base::GenerateGUID()),
        test_origin1_(url::Origin::Create(GURL("http://host1:1/"))) {
    next_map_id_key_ = std::vector<uint8_t>(
        std::begin(SessionStorageMetadata::kNextMapIdKeyBytes),
        std::end(SessionStorageMetadata::kNextMapIdKeyBytes));
    database_version_key_ = std::vector<uint8_t>(
        std::begin(SessionStorageMetadata::kDatabaseVersionBytes),
        std::end(SessionStorageMetadata::kDatabaseVersionBytes));
    namespaces_prefix_key_ = std::vector<uint8_t>(
        std::begin(SessionStorageMetadata::kNamespacePrefixBytes),
        std::end(SessionStorageMetadata::kNamespacePrefixBytes));
  }
  ~SessionStorageMetadataMigrationTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_path_.CreateUniqueTempDir());
    in_memory_env_ =
        leveldb_chrome::NewMemEnv("SessionStorage", LevelDBEnv::Get());
    leveldb_env::Options options;
    options.create_if_missing = true;
    options.env = in_memory_env_.get();
    std::unique_ptr<leveldb::DB> db;
    leveldb::Status s =
        leveldb_env::OpenDB(options, temp_path_.GetPath().AsUTF8Unsafe(), &db);
    ASSERT_TRUE(s.ok()) << s.ToString();
    old_ss_database_ = base::MakeRefCounted<SessionStorageDatabase>(
        temp_path_.GetPath(), base::ThreadTaskRunnerHandle::Get().get());
    old_ss_database_->SetDatabaseForTesting(std::move(db));
  }

  leveldb::DB* db() { return old_ss_database_->db(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_path_;
  std::string test_namespace1_id_;
  std::string test_namespace2_id_;
  url::Origin test_origin1_;
  std::unique_ptr<leveldb::Env> in_memory_env_;
  scoped_refptr<SessionStorageDatabase> old_ss_database_;

  std::vector<uint8_t> database_version_key_;
  std::vector<uint8_t> next_map_id_key_;
  std::vector<uint8_t> namespaces_prefix_key_;
};

TEST_F(SessionStorageMetadataMigrationTest, MigrateV0ToV1) {
  base::string16 key = base::ASCIIToUTF16("key");
  base::string16 value = base::ASCIIToUTF16("value");
  base::string16 key2 = base::ASCIIToUTF16("key2");
  key2.push_back(0xd83d);
  key2.push_back(0xde00);
  DOMStorageValuesMap data;
  data[key] = base::NullableString16(value, false);
  data[key2] = base::NullableString16(value, false);
  EXPECT_TRUE(old_ss_database_->CommitAreaChanges(test_namespace1_id_,
                                                  test_origin1_, false, data));
  EXPECT_TRUE(old_ss_database_->CloneNamespace(test_namespace1_id_,
                                               test_namespace2_id_));

  SessionStorageMetadata metadata;
  // Read non-existant version, give new version to save.
  leveldb::ReadOptions options;
  std::string db_value;
  leveldb::Status s = db()->Get(options, leveldb::Slice("version"), &db_value);
  EXPECT_TRUE(s.IsNotFound());
  std::vector<leveldb::mojom::BatchedOperationPtr> migration_operations;
  EXPECT_TRUE(
      metadata.ParseDatabaseVersion(base::nullopt, &migration_operations));
  EXPECT_FALSE(migration_operations.empty());
  EXPECT_EQ(1ul, migration_operations.size());

  // Grab the next map id, verify it doesn't crash.
  s = db()->Get(options, leveldb::Slice("next-map-id"), &db_value);
  EXPECT_TRUE(s.ok());
  metadata.ParseNextMapId(leveldb::StdStringToUint8Vector(db_value));

  // Get all keys-value pairs with the given key prefix
  std::vector<storage::DomStorageDatabase::KeyValuePair> values;
  {
    std::unique_ptr<leveldb::Iterator> it(db()->NewIterator(options));
    it->Seek(leveldb::Slice("namespace-"));
    for (; it->Valid(); it->Next()) {
      if (!it->key().starts_with(leveldb::Slice("namespace-")))
        break;
      leveldb::mojom::KeyValuePtr kv = leveldb::mojom::KeyValue::New();
      values.emplace_back(leveldb::GetVectorFor(it->key()),
                          leveldb::GetVectorFor(it->value()));
    }
    EXPECT_TRUE(it->status().ok());
  }

  EXPECT_TRUE(
      metadata.ParseNamespaces(std::move(values), &migration_operations));
  EXPECT_FALSE(migration_operations.empty());
  EXPECT_EQ(3ul, migration_operations.size());

  EXPECT_TRUE(base::Contains(
      migration_operations,
      leveldb::mojom::BatchedOperation::New(
          leveldb::mojom::BatchOperationType::PUT_KEY,
          StdStringToUint8Vector("version"), StdStringToUint8Vector("1"))));
  EXPECT_TRUE(
      base::Contains(migration_operations,
                     leveldb::mojom::BatchedOperation::New(
                         leveldb::mojom::BatchOperationType::DELETE_KEY,
                         StdStringToUint8Vector("map-0-"), base::nullopt)));
  EXPECT_TRUE(
      base::Contains(migration_operations,
                     leveldb::mojom::BatchedOperation::New(
                         leveldb::mojom::BatchOperationType::DELETE_KEY,
                         StdStringToUint8Vector("namespace-"), base::nullopt)));
}

}  // namespace

}  // namespace content
