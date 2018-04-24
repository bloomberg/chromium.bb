// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_metadata.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "components/services/leveldb/public/cpp/util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/fake_leveldb_database.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {
using leveldb::StdStringToUint8Vector;
using leveldb::Uint8VectorToStdString;
using leveldb::mojom::DatabaseError;

void GetCallback(std::vector<uint8_t>* value_out,
                 DatabaseError error,
                 const std::vector<uint8_t>& value) {
  *value_out = value;
}

void ErrorCallback(DatabaseError* error_out, DatabaseError error) {
  *error_out = error;
}

void GetAllCallback(std::vector<leveldb::mojom::KeyValuePtr>* values_out,
                    DatabaseError error,
                    std::vector<leveldb::mojom::KeyValuePtr> values) {
  *values_out = std::move(values);
}

class SessionStorageMetadataTest : public testing::Test {
 public:
  SessionStorageMetadataTest()
      : test_namespace1_id_(base::GenerateGUID()),
        test_namespace2_id_(base::GenerateGUID()),
        test_namespace3_id_(base::GenerateGUID()),
        test_origin1_(url::Origin::Create(GURL("http://host1:1/"))),
        test_origin2_(url::Origin::Create(GURL("http://host2:2/"))),
        database_(&mock_data_) {
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
    std::vector<uint8_t> value;
    database_.Get(database_version_key_, base::BindOnce(&GetCallback, &value));
    EXPECT_TRUE(metadata->ParseDatabaseVersion(value));
    database_.Get(next_map_id_key_, base::BindOnce(&GetCallback, &value));
    metadata->ParseNextMapId(value);
    std::vector<leveldb::mojom::KeyValuePtr> values;
    database_.GetPrefixed(namespaces_prefix_key_,
                          base::BindOnce(&GetAllCallback, &values));
    EXPECT_TRUE(metadata->ParseNamespaces(std::move(values)));
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
    mock_data_[StdStringToUint8Vector(
        std::string("namespace-") + test_namespace1_id_ + "-" +
        test_origin1_.GetURL().spec())] = StdStringToUint8Vector("1");
    mock_data_[StdStringToUint8Vector(
        std::string("namespace-") + test_namespace1_id_ + "-" +
        test_origin2_.GetURL().spec())] = StdStringToUint8Vector("3");
    mock_data_[StdStringToUint8Vector(
        std::string("namespace-") + test_namespace2_id_ + "-" +
        test_origin1_.GetURL().spec())] = StdStringToUint8Vector("1");
    mock_data_[StdStringToUint8Vector(
        std::string("namespace-") + test_namespace2_id_ + "-" +
        test_origin2_.GetURL().spec())] = StdStringToUint8Vector("4");

    mock_data_[next_map_id_key_] = StdStringToUint8Vector("5");

    mock_data_[StdStringToUint8Vector("map-1-key1")] =
        StdStringToUint8Vector("data1");
    mock_data_[StdStringToUint8Vector("map-3-key1")] =
        StdStringToUint8Vector("data3");
    mock_data_[StdStringToUint8Vector("map-4-key1")] =
        StdStringToUint8Vector("data4");

    mock_data_[database_version_key_] = StdStringToUint8Vector("1");
  }

 protected:
  std::string test_namespace1_id_;
  std::string test_namespace2_id_;
  std::string test_namespace3_id_;
  url::Origin test_origin1_;
  url::Origin test_origin2_;
  TestBrowserThreadBundle thread_bundle_;
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> mock_data_;
  FakeLevelDBDatabase database_;

  std::vector<uint8_t> database_version_key_;
  std::vector<uint8_t> next_map_id_key_;
  std::vector<uint8_t> namespaces_prefix_key_;
};

TEST_F(SessionStorageMetadataTest, SaveNewMetadata) {
  SessionStorageMetadata metadata;
  std::vector<leveldb::mojom::BatchedOperationPtr> operations =
      metadata.SetupNewDatabase(1);

  DatabaseError error;
  database_.Write(std::move(operations),
                  base::BindOnce(&ErrorCallback, &error));
  EXPECT_EQ(DatabaseError::OK, error);

  EXPECT_EQ(StdStringToUint8Vector("1"), mock_data_[database_version_key_]);
  EXPECT_EQ(StdStringToUint8Vector("0"), mock_data_[next_map_id_key_]);
}

TEST_F(SessionStorageMetadataTest, LoadingData) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  EXPECT_EQ(1, metadata.DatabaseVersion());
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

  DatabaseError error;
  database_.Write(std::move(operations),
                  base::BindOnce(&ErrorCallback, &error));
  EXPECT_EQ(DatabaseError::OK, error);

  // Verify metadata was written to disk.
  EXPECT_EQ(StdStringToUint8Vector("6"), mock_data_[next_map_id_key_]);
  EXPECT_EQ(StdStringToUint8Vector("5"),
            mock_data_[StdStringToUint8Vector(std::string("namespace-") +
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

  DatabaseError error;
  database_.Write(std::move(operations),
                  base::BindOnce(&ErrorCallback, &error));
  EXPECT_EQ(DatabaseError::OK, error);

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
  EXPECT_EQ(StdStringToUint8Vector("1"),
            mock_data_[StdStringToUint8Vector(std::string("namespace-") +
                                              test_namespace3_id_ + "-" +
                                              test_origin1_.GetURL().spec())]);
  EXPECT_EQ(StdStringToUint8Vector("3"),
            mock_data_[StdStringToUint8Vector(std::string("namespace-") +
                                              test_namespace3_id_ + "-" +
                                              test_origin2_.GetURL().spec())]);
}

TEST_F(SessionStorageMetadataTest, DeleteNamespace) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  std::vector<leveldb::mojom::BatchedOperationPtr> operations;
  metadata.DeleteNamespace(test_namespace1_id_, &operations);
  DatabaseError error;
  database_.Write(std::move(operations),
                  base::BindOnce(&ErrorCallback, &error));
  EXPECT_EQ(DatabaseError::OK, error);

  EXPECT_FALSE(
      base::ContainsKey(metadata.namespace_origin_map(), test_namespace1_id_));

  // Verify in-memory metadata is correct.
  auto ns2_entry = metadata.GetOrCreateNamespaceEntry(test_namespace2_id_);
  EXPECT_EQ(1, ns2_entry->second[test_origin1_]->ReferenceCount());
  EXPECT_EQ(1, ns2_entry->second[test_origin2_]->ReferenceCount());

  // Verify metadata and data was deleted from disk.
  EXPECT_FALSE(base::ContainsKey(
      mock_data_,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace1_id_ +
                             "-" + test_origin1_.GetURL().spec())));
  EXPECT_FALSE(base::ContainsKey(
      mock_data_,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace1_id_ +
                             "-" + test_origin2_.GetURL().spec())));
  EXPECT_FALSE(
      base::ContainsKey(mock_data_, StdStringToUint8Vector("map-3-key1")));
  EXPECT_TRUE(
      base::ContainsKey(mock_data_, StdStringToUint8Vector("map-1-key1")));
}

TEST_F(SessionStorageMetadataTest, DeleteArea) {
  SetupTestData();
  SessionStorageMetadata metadata;
  ReadMetadataFromDatabase(&metadata);

  // First delete an area with a shared map.
  std::vector<leveldb::mojom::BatchedOperationPtr> operations;
  metadata.DeleteArea(test_namespace1_id_, test_origin1_, &operations);
  DatabaseError error;
  database_.Write(std::move(operations),
                  base::BindOnce(&ErrorCallback, &error));
  EXPECT_EQ(DatabaseError::OK, error);

  // Verify in-memory metadata is correct.
  auto ns1_entry = metadata.GetOrCreateNamespaceEntry(test_namespace1_id_);
  auto ns2_entry = metadata.GetOrCreateNamespaceEntry(test_namespace2_id_);
  EXPECT_FALSE(base::ContainsKey(ns1_entry->second, test_origin1_));
  EXPECT_EQ(1, ns1_entry->second[test_origin2_]->ReferenceCount());
  EXPECT_EQ(1, ns2_entry->second[test_origin1_]->ReferenceCount());
  EXPECT_EQ(1, ns2_entry->second[test_origin2_]->ReferenceCount());

  // Verify only the applicable data was deleted.
  EXPECT_FALSE(base::ContainsKey(
      mock_data_,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace1_id_ +
                             "-" + test_origin1_.GetURL().spec())));
  EXPECT_TRUE(base::ContainsKey(
      mock_data_,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace1_id_ +
                             "-" + test_origin2_.GetURL().spec())));
  EXPECT_TRUE(
      base::ContainsKey(mock_data_, StdStringToUint8Vector("map-1-key1")));
  EXPECT_TRUE(
      base::ContainsKey(mock_data_, StdStringToUint8Vector("map-4-key1")));

  // Now delete an area with a unique map.
  operations.clear();
  metadata.DeleteArea(test_namespace2_id_, test_origin2_, &operations);
  database_.Write(std::move(operations),
                  base::BindOnce(&ErrorCallback, &error));
  EXPECT_EQ(DatabaseError::OK, error);

  // Verify in-memory metadata is correct.
  EXPECT_FALSE(base::ContainsKey(ns1_entry->second, test_origin1_));
  EXPECT_EQ(1, ns1_entry->second[test_origin2_]->ReferenceCount());
  EXPECT_EQ(1, ns2_entry->second[test_origin1_]->ReferenceCount());
  EXPECT_FALSE(base::ContainsKey(ns2_entry->second, test_origin2_));

  // Verify only the applicable data was deleted.
  EXPECT_TRUE(base::ContainsKey(
      mock_data_,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace2_id_ +
                             "-" + test_origin1_.GetURL().spec())));
  EXPECT_FALSE(base::ContainsKey(
      mock_data_,
      StdStringToUint8Vector(std::string("namespace-") + test_namespace2_id_ +
                             "-" + test_origin2_.GetURL().spec())));
  EXPECT_TRUE(
      base::ContainsKey(mock_data_, StdStringToUint8Vector("map-1-key1")));
  EXPECT_TRUE(
      base::ContainsKey(mock_data_, StdStringToUint8Vector("map-3-key1")));
  EXPECT_FALSE(
      base::ContainsKey(mock_data_, StdStringToUint8Vector("map-4-key1")));
}

}  // namespace

}  // namespace content
