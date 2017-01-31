// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/model_type_store_impl.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

sync_pb::ModelTypeState CreateModelTypeState(const std::string& value) {
  sync_pb::ModelTypeState state;
  state.set_encryption_key_name(value);
  return state;
}

sync_pb::EntityMetadata CreateEntityMetadata(const std::string& value) {
  sync_pb::EntityMetadata metadata;
  metadata.set_client_tag_hash(value);
  return metadata;
}

}  // namespace

class ModelTypeStoreImplTest : public testing::Test {
 public:
  void TearDown() override {
    if (store_) {
      store_.reset();
      PumpLoop();
    }
  }

  ModelTypeStore* store() { return store_.get(); }

  static void OnStoreCreated(std::unique_ptr<ModelTypeStore>* store_ref,
                             ModelTypeStore::Result result,
                             std::unique_ptr<ModelTypeStore> store) {
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
    *store_ref = std::move(store);
  }

  static void PumpLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  static void CreateStoreCaptureReference(
      const ModelType type,
      std::unique_ptr<ModelTypeStore>* store_ref) {
    ModelTypeStore::CreateInMemoryStoreForTest(
        type, base::Bind(&ModelTypeStoreImplTest::OnStoreCreated, store_ref));
    PumpLoop();
  }

  void CreateStore() { CreateStoreCaptureReference(UNSPECIFIED, &store_); }

  static void WriteData(ModelTypeStore* store,
                        const std::string& key,
                        const std::string& data) {
    auto write_batch = store->CreateWriteBatch();
    write_batch->WriteData(key, data);
    ModelTypeStore::Result result;
    store->CommitWriteBatch(std::move(write_batch),
                            base::Bind(&CaptureResult, &result));
    PumpLoop();
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  }

  static void WriteMetadata(ModelTypeStore* store,
                            const std::string& key,
                            const sync_pb::EntityMetadata& metadata) {
    auto write_batch = store->CreateWriteBatch();
    write_batch->GetMetadataChangeList()->UpdateMetadata(key, metadata);

    ModelTypeStore::Result result;
    store->CommitWriteBatch(std::move(write_batch),
                            base::Bind(&CaptureResult, &result));
    PumpLoop();
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  }

  static void WriteModelTypeState(ModelTypeStore* store,
                                  const sync_pb::ModelTypeState& state) {
    auto write_batch = store->CreateWriteBatch();
    write_batch->GetMetadataChangeList()->UpdateModelTypeState(state);

    ModelTypeStore::Result result;
    store->CommitWriteBatch(std::move(write_batch),
                            base::Bind(&CaptureResult, &result));
    PumpLoop();
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  }

  static void WriteRawMetadata(ModelTypeStore* store,
                               const std::string& key,
                               const std::string& raw_metadata) {
    auto write_batch = store->CreateWriteBatch();
    store->WriteMetadata(write_batch.get(), key, raw_metadata);

    ModelTypeStore::Result result;
    store->CommitWriteBatch(std::move(write_batch),
                            base::Bind(&CaptureResult, &result));
    PumpLoop();
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  }

  static void WriteRawModelTypeState(ModelTypeStore* store,
                                     const std::string& raw_model_type_state) {
    auto write_batch = store->CreateWriteBatch();
    store->WriteGlobalMetadata(write_batch.get(), raw_model_type_state);

    ModelTypeStore::Result result;
    store->CommitWriteBatch(std::move(write_batch),
                            base::Bind(&CaptureResult, &result));
    PumpLoop();
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  }

  void WriteTestData() {
    WriteData(store(), "id1", "data1");
    WriteMetadata(store(), "id1", CreateEntityMetadata("metadata1"));
    WriteData(store(), "id2", "data2");
    WriteModelTypeState(store(), CreateModelTypeState("type_state"));
  }

  static void ReadStoreContents(
      ModelTypeStore* store,
      std::unique_ptr<ModelTypeStore::RecordList>* data_records,
      std::unique_ptr<MetadataBatch>* metadata_batch) {
    ModelTypeStore::Result result;
    store->ReadAllData(
        base::Bind(&CaptureResultAndRecords, &result, data_records));
    PumpLoop();
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
    base::Optional<ModelError> error;
    store->ReadAllMetadata(
        base::Bind(&CaptureErrorAndMetadataBatch, &error, metadata_batch));
    PumpLoop();
    ASSERT_FALSE(error);
  }

  // Following functions capture parameters passed to callbacks into variables
  // provided by test. They can be passed as callbacks to ModelTypeStore
  // functions.
  static void CaptureResult(ModelTypeStore::Result* dst,
                            ModelTypeStore::Result result) {
    *dst = result;
  }

  static void CaptureResultAndRecords(
      ModelTypeStore::Result* dst_result,
      std::unique_ptr<ModelTypeStore::RecordList>* dst_records,
      ModelTypeStore::Result result,
      std::unique_ptr<ModelTypeStore::RecordList> records) {
    *dst_result = result;
    *dst_records = std::move(records);
  }

  static void CaptureErrorAndMetadataBatch(
      base::Optional<ModelError>* dst_error,
      std::unique_ptr<MetadataBatch>* dst_batch,
      base::Optional<ModelError> error,
      std::unique_ptr<MetadataBatch> batch) {
    *dst_error = error;
    *dst_batch = std::move(batch);
  }

  static void CaptureResultRecordsAndIdList(
      ModelTypeStore::Result* dst_result,
      std::unique_ptr<ModelTypeStore::RecordList>* dst_records,
      std::unique_ptr<ModelTypeStore::IdList>* dst_id_list,
      ModelTypeStore::Result result,
      std::unique_ptr<ModelTypeStore::RecordList> records,
      std::unique_ptr<ModelTypeStore::IdList> missing_id_list) {
    *dst_result = result;
    *dst_records = std::move(records);
    *dst_id_list = std::move(missing_id_list);
  }

  void VerifyMetadata(
      std::unique_ptr<MetadataBatch> batch,
      const sync_pb::ModelTypeState& state,
      std::map<std::string, sync_pb::EntityMetadata> expected_metadata) {
    EXPECT_EQ(state.SerializeAsString(),
              batch->GetModelTypeState().SerializeAsString());
    EntityMetadataMap actual_metadata = batch->TakeAllMetadata();
    for (const auto& kv : expected_metadata) {
      auto it = actual_metadata.find(kv.first);
      ASSERT_TRUE(it != actual_metadata.end());
      EXPECT_EQ(kv.second.SerializeAsString(), it->second.SerializeAsString());
      actual_metadata.erase(it);
    }
    EXPECT_EQ(0U, actual_metadata.size());
  }

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<ModelTypeStore> store_;
};

// Matcher to verify contents of returned RecordList .
MATCHER_P2(RecordMatches, id, value, "") {
  return arg.id == id && arg.value == value;
}

// Test that CreateInMemoryStoreForTest triggers store initialization that
// results with callback being called with valid store pointer.
TEST_F(ModelTypeStoreImplTest, CreateInMemoryStore) {
  std::unique_ptr<ModelTypeStore> store;
  ModelTypeStore::CreateInMemoryStoreForTest(
      UNSPECIFIED, base::Bind(&ModelTypeStoreImplTest::OnStoreCreated, &store));
  ASSERT_EQ(nullptr, store);
  PumpLoop();
  ASSERT_NE(nullptr, store);
}

// Test read functions on empty store.
TEST_F(ModelTypeStoreImplTest, ReadEmptyStore) {
  CreateStore();

  std::unique_ptr<ModelTypeStore::RecordList> data_records;
  std::unique_ptr<MetadataBatch> metadata_batch;
  ReadStoreContents(store(), &data_records, &metadata_batch);
  ASSERT_TRUE(data_records->empty());
  VerifyMetadata(std::move(metadata_batch), sync_pb::ModelTypeState(),
                 std::map<std::string, sync_pb::EntityMetadata>());
}

// Test that records that are written to store later can be read from it.
TEST_F(ModelTypeStoreImplTest, WriteThenRead) {
  CreateStore();
  WriteTestData();

  std::unique_ptr<ModelTypeStore::RecordList> data_records;
  std::unique_ptr<MetadataBatch> metadata_batch;
  ReadStoreContents(store(), &data_records, &metadata_batch);
  ASSERT_THAT(*data_records,
              testing::UnorderedElementsAre(RecordMatches("id1", "data1"),
                                            RecordMatches("id2", "data2")));
  VerifyMetadata(std::move(metadata_batch), CreateModelTypeState("type_state"),
                 {{"id1", CreateEntityMetadata("metadata1")}});
}

// Test that if ModelTypeState is not set then ReadAllMetadata still succeeds
// and returns entry metadata records.
TEST_F(ModelTypeStoreImplTest, MissingModelTypeState) {
  CreateStore();
  WriteTestData();

  ModelTypeStore::Result result;

  auto write_batch = store()->CreateWriteBatch();
  write_batch->GetMetadataChangeList()->ClearModelTypeState();
  store()->CommitWriteBatch(std::move(write_batch),
                            base::Bind(&CaptureResult, &result));
  PumpLoop();
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);

  base::Optional<ModelError> error;
  std::unique_ptr<MetadataBatch> metadata_batch;
  store()->ReadAllMetadata(
      base::Bind(&CaptureErrorAndMetadataBatch, &error, &metadata_batch));
  PumpLoop();
  ASSERT_FALSE(error);
  VerifyMetadata(std::move(metadata_batch), sync_pb::ModelTypeState(),
                 {{"id1", CreateEntityMetadata("metadata1")}});
}

// Tests that unparseable metadata results in an error being set.
TEST_F(ModelTypeStoreImplTest, CorruptModelTypeState) {
  CreateStore();
  WriteTestData();

  // Write a ModelTypeState that can't be parsed.
  WriteRawModelTypeState(store(), "unparseable");

  base::Optional<ModelError> error;
  std::unique_ptr<MetadataBatch> metadata_batch;
  store()->ReadAllMetadata(
      base::Bind(&CaptureErrorAndMetadataBatch, &error, &metadata_batch));
  PumpLoop();
  ASSERT_TRUE(error);
  VerifyMetadata(std::move(metadata_batch), sync_pb::ModelTypeState(),
                 std::map<std::string, sync_pb::EntityMetadata>());
}

// Tests that unparseable metadata results in an error being set.
TEST_F(ModelTypeStoreImplTest, CorruptEntityMetadata) {
  CreateStore();
  WriteTestData();

  // Write an EntityMetadata that can't be parsed.
  WriteRawMetadata(store(), "id", "unparseable");

  base::Optional<ModelError> error;
  std::unique_ptr<MetadataBatch> metadata_batch;
  store()->ReadAllMetadata(
      base::Bind(&CaptureErrorAndMetadataBatch, &error, &metadata_batch));
  PumpLoop();
  ASSERT_TRUE(error);
  VerifyMetadata(std::move(metadata_batch), sync_pb::ModelTypeState(),
                 std::map<std::string, sync_pb::EntityMetadata>());
}

// Test that when reading data records by id, if one of the ids is missing
// operation still succeeds and missing id is returned in missing_id_list.
TEST_F(ModelTypeStoreImplTest, ReadMissingDataRecords) {
  CreateStore();
  WriteTestData();

  ModelTypeStore::Result result;

  ModelTypeStore::IdList id_list;
  id_list.push_back("id1");
  id_list.push_back("id3");

  std::unique_ptr<ModelTypeStore::RecordList> records;
  std::unique_ptr<ModelTypeStore::IdList> missing_id_list;

  store()->ReadData(id_list, base::Bind(&CaptureResultRecordsAndIdList, &result,
                                        &records, &missing_id_list));
  PumpLoop();
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  ASSERT_THAT(*records,
              testing::UnorderedElementsAre(RecordMatches("id1", "data1")));
  ASSERT_THAT(*missing_id_list, testing::UnorderedElementsAre("id3"));
}

// Test that stores for different types that share the same backend don't
// interfere with each other's records.
TEST_F(ModelTypeStoreImplTest, TwoStoresWithSharedBackend) {
  std::unique_ptr<ModelTypeStore> store_1;
  std::unique_ptr<ModelTypeStore> store_2;
  CreateStoreCaptureReference(AUTOFILL, &store_1);
  CreateStoreCaptureReference(BOOKMARKS, &store_2);

  const sync_pb::EntityMetadata metadata1 = CreateEntityMetadata("metadata1");
  const sync_pb::EntityMetadata metadata2 = CreateEntityMetadata("metadata2");
  const sync_pb::ModelTypeState state1 = CreateModelTypeState("state1");
  const sync_pb::ModelTypeState state2 = CreateModelTypeState("state2");

  WriteData(store_1.get(), "key", "data1");
  WriteMetadata(store_1.get(), "key", metadata1);
  WriteModelTypeState(store_1.get(), state1);

  WriteData(store_2.get(), "key", "data2");
  WriteMetadata(store_2.get(), "key", metadata2);
  WriteModelTypeState(store_2.get(), state2);

  std::unique_ptr<ModelTypeStore::RecordList> data_records;
  std::unique_ptr<MetadataBatch> metadata_batch;

  ReadStoreContents(store_1.get(), &data_records, &metadata_batch);

  EXPECT_THAT(*data_records,
              testing::ElementsAre(RecordMatches("key", "data1")));
  VerifyMetadata(std::move(metadata_batch), state1, {{"key", metadata1}});

  ReadStoreContents(store_2.get(), &data_records, &metadata_batch);

  EXPECT_THAT(*data_records,
              testing::ElementsAre(RecordMatches("key", "data2")));
  VerifyMetadata(std::move(metadata_batch), state2, {{"key", metadata2}});
}

}  // namespace syncer
