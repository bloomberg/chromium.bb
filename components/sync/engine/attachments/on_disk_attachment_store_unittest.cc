// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/attachments/on_disk_attachment_store.h"

#include <stdint.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/sync/engine/attachments/attachment_store_test_template.h"
#include "components/sync/engine_impl/attachments/proto/attachment_store.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace syncer {

namespace {

void AttachmentStoreCreated(AttachmentStore::Result* result_dest,
                            const AttachmentStore::Result& result) {
  *result_dest = result;
}

}  // namespace

// Instantiation of common attachment store tests.
class OnDiskAttachmentStoreFactory {
 public:
  OnDiskAttachmentStoreFactory() {}
  ~OnDiskAttachmentStoreFactory() {}

  std::unique_ptr<AttachmentStore> CreateAttachmentStore() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    std::unique_ptr<AttachmentStore> store;
    AttachmentStore::Result result = AttachmentStore::UNSPECIFIED_ERROR;
    store = AttachmentStore::CreateOnDiskStore(
        temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
        base::Bind(&AttachmentStoreCreated, &result));
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
    EXPECT_EQ(AttachmentStore::SUCCESS, result);
    return store;
  }

 private:
  base::ScopedTempDir temp_dir_;
};

INSTANTIATE_TYPED_TEST_CASE_P(OnDisk,
                              AttachmentStoreTest,
                              OnDiskAttachmentStoreFactory);

// Tests specific to OnDiskAttachmentStore.
class OnDiskAttachmentStoreSpecificTest : public testing::Test {
 public:
  base::ScopedTempDir temp_dir_;
  base::FilePath db_path_;
  base::MessageLoop message_loop_;
  std::unique_ptr<AttachmentStore> store_;

  OnDiskAttachmentStoreSpecificTest() {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("leveldb"));
    base::CreateDirectory(db_path_);
  }

  void CopyResult(AttachmentStore::Result* destination_result,
                  const AttachmentStore::Result& source_result) {
    *destination_result = source_result;
  }

  void CopyResultAttachments(
      AttachmentStore::Result* destination_result,
      AttachmentIdList* destination_failed_attachment_ids,
      const AttachmentStore::Result& source_result,
      std::unique_ptr<AttachmentMap> source_attachments,
      std::unique_ptr<AttachmentIdList> source_failed_attachment_ids) {
    CopyResult(destination_result, source_result);
    *destination_failed_attachment_ids = *source_failed_attachment_ids;
  }

  void CopyResultMetadata(
      AttachmentStore::Result* destination_result,
      std::unique_ptr<AttachmentMetadataList>* destination_metadata,
      const AttachmentStore::Result& source_result,
      std::unique_ptr<AttachmentMetadataList> source_metadata) {
    CopyResult(destination_result, source_result);
    *destination_metadata = std::move(source_metadata);
  }

  std::unique_ptr<leveldb::DB> OpenLevelDB() {
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status s =
        leveldb::DB::Open(options, db_path_.AsUTF8Unsafe(), &db);
    EXPECT_TRUE(s.ok());
    return base::WrapUnique(db);
  }

  void UpdateRecord(const std::string& key, const std::string& content) {
    std::unique_ptr<leveldb::DB> db = OpenLevelDB();
    leveldb::Status s = db->Put(leveldb::WriteOptions(), key, content);
    EXPECT_TRUE(s.ok());
  }

  void UpdateStoreMetadataRecord(const std::string& content) {
    UpdateRecord("database-metadata", content);
  }

  void UpdateAttachmentMetadataRecord(const AttachmentId& attachment_id,
                                      const std::string& content) {
    std::string metadata_key =
        OnDiskAttachmentStore::MakeMetadataKeyFromAttachmentId(attachment_id);
    UpdateRecord(metadata_key, content);
  }

  std::string ReadStoreMetadataRecord() {
    std::unique_ptr<leveldb::DB> db = OpenLevelDB();
    std::string content;
    leveldb::Status s =
        db->Get(leveldb::ReadOptions(), "database-metadata", &content);
    EXPECT_TRUE(s.ok());
    return content;
  }

  void VerifyAttachmentRecordsPresent(const AttachmentId& attachment_id,
                                      bool expect_records_present) {
    std::unique_ptr<leveldb::DB> db = OpenLevelDB();

    std::string metadata_key =
        OnDiskAttachmentStore::MakeMetadataKeyFromAttachmentId(attachment_id);
    std::string data_key =
        OnDiskAttachmentStore::MakeDataKeyFromAttachmentId(attachment_id);
    std::string data;
    leveldb::Status s = db->Get(leveldb::ReadOptions(), data_key, &data);
    if (expect_records_present)
      EXPECT_TRUE(s.ok());
    else
      EXPECT_TRUE(s.IsNotFound());
    s = db->Get(leveldb::ReadOptions(), metadata_key, &data);
    if (expect_records_present)
      EXPECT_TRUE(s.ok());
    else
      EXPECT_TRUE(s.IsNotFound());
  }

  void RunLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
};

// Ensure that store can be closed and reopen while retaining stored
// attachments.
TEST_F(OnDiskAttachmentStoreSpecificTest, CloseAndReopen) {
  AttachmentStore::Result result;

  result = AttachmentStore::UNSPECIFIED_ERROR;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &result));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, result);

  result = AttachmentStore::UNSPECIFIED_ERROR;
  std::string some_data = "data";
  Attachment attachment =
      Attachment::Create(base::RefCountedString::TakeString(&some_data));
  AttachmentList attachments;
  attachments.push_back(attachment);
  store_->Write(attachments,
                base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResult,
                           base::Unretained(this), &result));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, result);

  // Close and reopen attachment store.
  store_ = nullptr;
  result = AttachmentStore::UNSPECIFIED_ERROR;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &result));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, result);

  result = AttachmentStore::UNSPECIFIED_ERROR;
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(attachment.GetId());
  AttachmentIdList failed_attachment_ids;
  store_->Read(
      attachment_ids,
      base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResultAttachments,
                 base::Unretained(this), &result, &failed_attachment_ids));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, result);
  EXPECT_TRUE(failed_attachment_ids.empty());
}

// Ensure loading corrupt attachment store fails.
TEST_F(OnDiskAttachmentStoreSpecificTest, FailToOpen) {
  // To simulate corrupt database write empty CURRENT file.
  std::string current_file_content = "";
  base::WriteFile(db_path_.Append(FILE_PATH_LITERAL("CURRENT")),
                  current_file_content.c_str(), current_file_content.size());

  AttachmentStore::Result result = AttachmentStore::SUCCESS;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &result));
  RunLoop();
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, result);
}

// Ensure that attachment store works correctly when store metadata is missing,
// corrupt or has unknown schema version.
TEST_F(OnDiskAttachmentStoreSpecificTest, StoreMetadata) {
  // Create and close empty database.
  OpenLevelDB();
  // Open database with AttachmentStore.
  AttachmentStore::Result result = AttachmentStore::UNSPECIFIED_ERROR;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &result));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, result);
  // Close AttachmentStore so that test can check content.
  store_ = nullptr;
  RunLoop();

  // AttachmentStore should create metadata record.
  std::string data = ReadStoreMetadataRecord();
  attachment_store_pb::StoreMetadata metadata;
  EXPECT_TRUE(metadata.ParseFromString(data));
  EXPECT_EQ(1, metadata.schema_version());

  // Set unknown future schema version.
  metadata.set_schema_version(2);
  data = metadata.SerializeAsString();
  UpdateStoreMetadataRecord(data);

  // AttachmentStore should fail to load.
  result = AttachmentStore::SUCCESS;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &result));
  RunLoop();
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, result);

  // Write garbage into metadata record.
  UpdateStoreMetadataRecord("abra.cadabra");

  // AttachmentStore should fail to load.
  result = AttachmentStore::SUCCESS;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &result));
  RunLoop();
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, result);
}

// Ensure that attachment store correctly maintains metadata records for
// attachments.
TEST_F(OnDiskAttachmentStoreSpecificTest, RecordMetadata) {
  // Create attachment store.
  AttachmentStore::Result create_result = AttachmentStore::UNSPECIFIED_ERROR;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &create_result));

  // Write two attachments.
  AttachmentStore::Result write_result = AttachmentStore::UNSPECIFIED_ERROR;
  std::string some_data;
  AttachmentList attachments;
  some_data = "data1";
  attachments.push_back(
      Attachment::Create(base::RefCountedString::TakeString(&some_data)));
  some_data = "data2";
  attachments.push_back(
      Attachment::Create(base::RefCountedString::TakeString(&some_data)));
  store_->Write(attachments,
                base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResult,
                           base::Unretained(this), &write_result));

  // Delete one of written attachments.
  AttachmentStore::Result drop_result = AttachmentStore::UNSPECIFIED_ERROR;
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(attachments[0].GetId());
  store_->Drop(attachment_ids,
               base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResult,
                          base::Unretained(this), &drop_result));
  store_ = nullptr;
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, create_result);
  EXPECT_EQ(AttachmentStore::SUCCESS, write_result);
  EXPECT_EQ(AttachmentStore::SUCCESS, drop_result);

  // Verify that attachment store contains only records for second attachment.
  VerifyAttachmentRecordsPresent(attachments[0].GetId(), false);
  VerifyAttachmentRecordsPresent(attachments[1].GetId(), true);
}

// Ensure that attachment store fails to load attachment if the crc in the store
// does not match the data.
TEST_F(OnDiskAttachmentStoreSpecificTest, MismatchedCrcInStore) {
  // Create attachment store.
  AttachmentStore::Result create_result = AttachmentStore::UNSPECIFIED_ERROR;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &create_result));

  // Write attachment with incorrect crc32c.
  AttachmentStore::Result write_result = AttachmentStore::UNSPECIFIED_ERROR;
  const uint32_t intentionally_wrong_crc32c = 0;

  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString());
  some_data->data() = "data1";
  Attachment attachment = Attachment::CreateFromParts(
      AttachmentId::Create(some_data->size(), intentionally_wrong_crc32c),
      some_data);
  AttachmentList attachments;
  attachments.push_back(attachment);
  store_->Write(attachments,
                base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResult,
                           base::Unretained(this), &write_result));

  // Read attachment.
  AttachmentStore::Result read_result = AttachmentStore::UNSPECIFIED_ERROR;
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(attachment.GetId());
  AttachmentIdList failed_attachment_ids;
  store_->Read(
      attachment_ids,
      base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResultAttachments,
                 base::Unretained(this), &read_result, &failed_attachment_ids));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, create_result);
  EXPECT_EQ(AttachmentStore::SUCCESS, write_result);
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, read_result);
  EXPECT_THAT(failed_attachment_ids, testing::ElementsAre(attachment.GetId()));
}

// Ensure that attachment store fails to load attachment if the crc in the id
// does not match the data.
TEST_F(OnDiskAttachmentStoreSpecificTest, MismatchedCrcInId) {
  // Create attachment store.
  AttachmentStore::Result create_result = AttachmentStore::UNSPECIFIED_ERROR;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &create_result));

  AttachmentStore::Result write_result = AttachmentStore::UNSPECIFIED_ERROR;
  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString());
  some_data->data() = "data1";
  Attachment attachment = Attachment::Create(some_data);
  AttachmentList attachments;
  attachments.push_back(attachment);
  store_->Write(attachments,
                base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResult,
                           base::Unretained(this), &write_result));

  // Read, but with the wrong crc32c in the id.
  AttachmentStore::Result read_result = AttachmentStore::SUCCESS;

  AttachmentId id_with_bad_crc32c =
      AttachmentId::Create(attachment.GetId().GetSize(), 12345);
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(id_with_bad_crc32c);
  AttachmentIdList failed_attachment_ids;
  store_->Read(
      attachment_ids,
      base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResultAttachments,
                 base::Unretained(this), &read_result, &failed_attachment_ids));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, create_result);
  EXPECT_EQ(AttachmentStore::SUCCESS, write_result);
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, read_result);
  EXPECT_THAT(failed_attachment_ids, testing::ElementsAre(id_with_bad_crc32c));
}

// Ensure that after store initialization failure ReadWrite/Drop operations fail
// with correct error.
TEST_F(OnDiskAttachmentStoreSpecificTest, OpsAfterInitializationFailed) {
  // To simulate corrupt database write empty CURRENT file.
  std::string current_file_content = "";
  base::WriteFile(db_path_.Append(FILE_PATH_LITERAL("CURRENT")),
                  current_file_content.c_str(), current_file_content.size());

  AttachmentStore::Result create_result = AttachmentStore::SUCCESS;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &create_result));

  // Reading from uninitialized store should result in
  // STORE_INITIALIZATION_FAILED.
  AttachmentStore::Result read_result = AttachmentStore::SUCCESS;
  AttachmentIdList attachment_ids;
  std::string some_data("data1");
  Attachment attachment =
      Attachment::Create(base::RefCountedString::TakeString(&some_data));
  attachment_ids.push_back(attachment.GetId());
  AttachmentIdList failed_attachment_ids;
  store_->Read(
      attachment_ids,
      base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResultAttachments,
                 base::Unretained(this), &read_result, &failed_attachment_ids));

  // Dropping from uninitialized store should result in
  // STORE_INITIALIZATION_FAILED.
  AttachmentStore::Result drop_result = AttachmentStore::SUCCESS;
  store_->Drop(attachment_ids,
               base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResult,
                          base::Unretained(this), &drop_result));

  // Writing to uninitialized store should result in
  // STORE_INITIALIZATION_FAILED.
  AttachmentStore::Result write_result = AttachmentStore::SUCCESS;
  AttachmentList attachments;
  attachments.push_back(attachment);
  store_->Write(attachments,
                base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResult,
                           base::Unretained(this), &write_result));

  RunLoop();
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, create_result);
  EXPECT_EQ(AttachmentStore::STORE_INITIALIZATION_FAILED, read_result);
  EXPECT_THAT(failed_attachment_ids, testing::ElementsAre(attachment_ids[0]));
  EXPECT_EQ(AttachmentStore::STORE_INITIALIZATION_FAILED, drop_result);
  EXPECT_EQ(AttachmentStore::STORE_INITIALIZATION_FAILED, write_result);
}

// Ensure that attachment store handles the case of having an unexpected
// record at the end without crashing.
TEST_F(OnDiskAttachmentStoreSpecificTest, ReadMetadataWithUnexpectedRecord) {
  // Write a bogus entry at the end of the database.
  UpdateRecord("zzz", "foobar");

  // Create attachment store.
  AttachmentStore::Result create_result = AttachmentStore::UNSPECIFIED_ERROR;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &create_result));

  // Read all metadata. Should be getting no error and zero entries.
  AttachmentStore::Result metadata_result = AttachmentStore::UNSPECIFIED_ERROR;
  std::unique_ptr<AttachmentMetadataList> metadata_list;
  store_->ReadMetadata(
      base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResultMetadata,
                 base::Unretained(this), &metadata_result, &metadata_list));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, create_result);
  EXPECT_EQ(AttachmentStore::SUCCESS, metadata_result);
  EXPECT_EQ(0U, metadata_list->size());
  metadata_list.reset();

  // Write 3 attachments to the store
  AttachmentList attachments;

  for (int i = 0; i < 3; i++) {
    std::string some_data = "data";
    Attachment attachment =
        Attachment::Create(base::RefCountedString::TakeString(&some_data));
    attachments.push_back(attachment);
  }
  AttachmentStore::Result write_result = AttachmentStore::UNSPECIFIED_ERROR;
  store_->Write(attachments,
                base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResult,
                           base::Unretained(this), &write_result));

  // Read all metadata back. We should be getting 3 entries.
  store_->ReadMetadata(
      base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResultMetadata,
                 base::Unretained(this), &metadata_result, &metadata_list));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, write_result);
  EXPECT_EQ(AttachmentStore::SUCCESS, metadata_result);
  EXPECT_EQ(3U, metadata_list->size());
  // Get Id of the attachment in the middle.
  AttachmentId id = (*metadata_list.get())[1].GetId();
  metadata_list.reset();

  // Close the store.
  store_ = nullptr;
  RunLoop();

  // Modify the middle attachment metadata entry so that it isn't valid anymore.
  UpdateAttachmentMetadataRecord(id, "foobar");

  // Reopen the store.
  create_result = AttachmentStore::UNSPECIFIED_ERROR;
  metadata_result = AttachmentStore::UNSPECIFIED_ERROR;
  store_ = AttachmentStore::CreateOnDiskStore(
      temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AttachmentStoreCreated, &create_result));

  // Read all metadata back. We should be getting a failure and
  // only 2 entries now.
  store_->ReadMetadata(
      base::Bind(&OnDiskAttachmentStoreSpecificTest::CopyResultMetadata,
                 base::Unretained(this), &metadata_result, &metadata_list));
  RunLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, create_result);
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, metadata_result);
  EXPECT_EQ(2U, metadata_list->size());
}

}  // namespace syncer
