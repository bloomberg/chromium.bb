// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/model_type_store_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

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
    std::unique_ptr<ModelTypeStore::WriteBatch> write_batch =
        store->CreateWriteBatch();
    store->WriteData(write_batch.get(), key, data);
    ModelTypeStore::Result result;
    store->CommitWriteBatch(std::move(write_batch),
                            base::Bind(&CaptureResult, &result));
    PumpLoop();
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  }

  static void WriteMetadata(ModelTypeStore* store,
                            const std::string& key,
                            const std::string& metadata) {
    std::unique_ptr<ModelTypeStore::WriteBatch> write_batch =
        store->CreateWriteBatch();
    store->WriteMetadata(write_batch.get(), key, metadata);
    ModelTypeStore::Result result;
    store->CommitWriteBatch(std::move(write_batch),
                            base::Bind(&CaptureResult, &result));
    PumpLoop();
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  }

  static void WriteGlobalMetadata(ModelTypeStore* store,
                                  const std::string& metadata) {
    std::unique_ptr<ModelTypeStore::WriteBatch> write_batch =
        store->CreateWriteBatch();
    store->WriteGlobalMetadata(write_batch.get(), metadata);
    ModelTypeStore::Result result;
    store->CommitWriteBatch(std::move(write_batch),
                            base::Bind(&CaptureResult, &result));
    PumpLoop();
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  }

  void WriteTestData() {
    WriteData(store(), "id1", "data1");
    WriteMetadata(store(), "id1", "metadata1");
    WriteData(store(), "id2", "data2");
    WriteGlobalMetadata(store(), "global_metadata");
  }

  static void ReadStoreContents(
      ModelTypeStore* store,
      std::unique_ptr<ModelTypeStore::RecordList>* data_records,
      std::unique_ptr<ModelTypeStore::RecordList>* metadata_records,
      std::string* global_metadata) {
    ModelTypeStore::Result result;
    store->ReadAllData(
        base::Bind(&CaptureResultWithRecords, &result, data_records));
    PumpLoop();
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
    store->ReadAllMetadata(base::Bind(&CaptureResutRecordsAndString, &result,
                                      metadata_records, global_metadata));
    PumpLoop();
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  }

  // Following functions capture parameters passed to callbacks into variables
  // provided by test. They can be passed as callbacks to ModelTypeStore
  // functions.
  static void CaptureResult(ModelTypeStore::Result* dst,
                            ModelTypeStore::Result result) {
    *dst = result;
  }

  static void CaptureResultWithRecords(
      ModelTypeStore::Result* dst_result,
      std::unique_ptr<ModelTypeStore::RecordList>* dst_records,
      ModelTypeStore::Result result,
      std::unique_ptr<ModelTypeStore::RecordList> records) {
    *dst_result = result;
    *dst_records = std::move(records);
  }

  static void CaptureResutRecordsAndString(
      ModelTypeStore::Result* dst_result,
      std::unique_ptr<ModelTypeStore::RecordList>* dst_records,
      std::string* dst_value,
      ModelTypeStore::Result result,
      std::unique_ptr<ModelTypeStore::RecordList> records,
      const std::string& value) {
    *dst_result = result;
    *dst_records = std::move(records);
    *dst_value = value;
  }

  static void CaptureResutRecordsAndIdList(
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
  std::unique_ptr<ModelTypeStore::RecordList> metadata_records;
  std::string global_metadata;
  ReadStoreContents(store(), &data_records, &metadata_records,
                    &global_metadata);
  ASSERT_TRUE(data_records->empty());
  ASSERT_TRUE(metadata_records->empty());
  ASSERT_TRUE(global_metadata.empty());
}

// Test that records that are written to store later can be read from it.
TEST_F(ModelTypeStoreImplTest, WriteThenRead) {
  CreateStore();
  WriteTestData();

  std::unique_ptr<ModelTypeStore::RecordList> data_records;
  std::unique_ptr<ModelTypeStore::RecordList> metadata_records;
  std::string global_metadata;
  ReadStoreContents(store(), &data_records, &metadata_records,
                    &global_metadata);
  ASSERT_THAT(*data_records,
              testing::UnorderedElementsAre(RecordMatches("id1", "data1"),
                                            RecordMatches("id2", "data2")));
  ASSERT_THAT(*metadata_records,
              testing::ElementsAre(RecordMatches("id1", "metadata1")));
  ASSERT_EQ("global_metadata", global_metadata);
}

// Test that if global metadata is not set then ReadAllMetadata still succeeds
// and returns entry metadata records.
TEST_F(ModelTypeStoreImplTest, MissingGlobalMetadata) {
  CreateStore();
  WriteTestData();

  ModelTypeStore::Result result;

  std::unique_ptr<ModelTypeStore::WriteBatch> write_batch =
      store()->CreateWriteBatch();
  store()->DeleteGlobalMetadata(write_batch.get());
  store()->CommitWriteBatch(std::move(write_batch),
                            base::Bind(&CaptureResult, &result));
  PumpLoop();
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);

  std::unique_ptr<ModelTypeStore::RecordList> records;
  std::string global_metadata;
  store()->ReadAllMetadata(base::Bind(&CaptureResutRecordsAndString, &result,
                                      &records, &global_metadata));
  PumpLoop();
  ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
  ASSERT_THAT(*records,
              testing::UnorderedElementsAre(RecordMatches("id1", "metadata1")));
  ASSERT_EQ(std::string(), global_metadata);
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

  store()->ReadData(id_list, base::Bind(&CaptureResutRecordsAndIdList, &result,
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

  WriteData(store_1.get(), "key", "data1");
  WriteMetadata(store_1.get(), "key", "metadata1");
  WriteGlobalMetadata(store_1.get(), "global1");

  WriteData(store_2.get(), "key", "data2");
  WriteMetadata(store_2.get(), "key", "metadata2");
  WriteGlobalMetadata(store_2.get(), "global2");

  std::unique_ptr<ModelTypeStore::RecordList> data_records;
  std::unique_ptr<ModelTypeStore::RecordList> metadata_records;
  std::string global_metadata;

  ReadStoreContents(store_1.get(), &data_records, &metadata_records,
                    &global_metadata);

  EXPECT_THAT(*data_records,
              testing::ElementsAre(RecordMatches("key", "data1")));
  EXPECT_THAT(*metadata_records,
              testing::ElementsAre(RecordMatches("key", "metadata1")));
  EXPECT_EQ("global1", global_metadata);

  ReadStoreContents(store_2.get(), &data_records, &metadata_records,
                    &global_metadata);

  EXPECT_THAT(*data_records,
              testing::ElementsAre(RecordMatches("key", "data2")));
  EXPECT_THAT(*metadata_records,
              testing::ElementsAre(RecordMatches("key", "metadata2")));
  EXPECT_EQ("global2", global_metadata);
}

}  // namespace syncer
