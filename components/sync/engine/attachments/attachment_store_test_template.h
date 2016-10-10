// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_ATTACHMENTS_ATTACHMENT_STORE_TEST_TEMPLATE_H_
#define COMPONENTS_SYNC_ENGINE_ATTACHMENTS_ATTACHMENT_STORE_TEST_TEMPLATE_H_

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/engine/attachments/attachment_util.h"
#include "components/sync/model/attachments/attachment.h"
#include "components/sync/model/attachments/attachment_store.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

// AttachmentStoreTest defines tests for AttachmentStore. To instantiate these
// tests for a particular implementation you need to:
//  - Include this file in test.
//  - Create factory class for attachment store that implements factory method.
//  - add INSTANTIATE_TYPED_TEST_CASE_P statement.
// Here is how to do it for MyAttachmentStore:
//
// class MyAttachmentStoreFactory {
//  public:
//   scoped_refptr<AttachmentStore> CreateAttachmentStore() {
//     return new MyAttachmentStore();
//   }
// };
//
// INSTANTIATE_TYPED_TEST_CASE_P(My,
//                               AttachmentStoreTest,
//                               MyAttachmentStoreFactory);

namespace syncer {

const char kTestData1[] = "test data 1";
const char kTestData2[] = "test data 2";

template <typename AttachmentStoreFactory>
class AttachmentStoreTest : public testing::Test {
 protected:
  AttachmentStoreFactory attachment_store_factory;
  base::MessageLoop message_loop;
  std::unique_ptr<AttachmentStore> store;
  std::unique_ptr<AttachmentStoreForSync> store_for_sync;
  AttachmentStore::Result result;
  std::unique_ptr<AttachmentMap> attachments;
  std::unique_ptr<AttachmentIdList> failed_attachment_ids;
  std::unique_ptr<AttachmentMetadataList> attachment_metadata;

  AttachmentStore::ReadCallback read_callback;
  AttachmentStore::WriteCallback write_callback;
  AttachmentStore::DropCallback drop_callback;
  AttachmentStore::ReadMetadataCallback read_metadata_callback;

  scoped_refptr<base::RefCountedString> some_data1;
  scoped_refptr<base::RefCountedString> some_data2;

  AttachmentStoreTest() {}

  void SetUp() override {
    store = attachment_store_factory.CreateAttachmentStore();
    store_for_sync = store->CreateAttachmentStoreForSync();

    Clear();
    read_callback = base::Bind(&AttachmentStoreTest::CopyResultAttachments,
                               base::Unretained(this), &result, &attachments,
                               &failed_attachment_ids);
    write_callback = base::Bind(&AttachmentStoreTest::CopyResult,
                                base::Unretained(this), &result);
    drop_callback = write_callback;
    read_metadata_callback =
        base::Bind(&AttachmentStoreTest::CopyResultMetadata,
                   base::Unretained(this), &result, &attachment_metadata);

    some_data1 = new base::RefCountedString;
    some_data1->data() = kTestData1;

    some_data2 = new base::RefCountedString;
    some_data2->data() = kTestData2;
  }

  void ClearAndPumpLoop() {
    Clear();
    base::RunLoop().RunUntilIdle();
  }

 private:
  void Clear() {
    result = AttachmentStore::UNSPECIFIED_ERROR;
    attachments.reset();
    failed_attachment_ids.reset();
    attachment_metadata.reset();
  }

  void CopyResult(AttachmentStore::Result* destination_result,
                  const AttachmentStore::Result& source_result) {
    *destination_result = source_result;
  }

  void CopyResultAttachments(
      AttachmentStore::Result* destination_result,
      std::unique_ptr<AttachmentMap>* destination_attachments,
      std::unique_ptr<AttachmentIdList>* destination_failed_attachment_ids,
      const AttachmentStore::Result& source_result,
      std::unique_ptr<AttachmentMap> source_attachments,
      std::unique_ptr<AttachmentIdList> source_failed_attachment_ids) {
    CopyResult(destination_result, source_result);
    *destination_attachments = std::move(source_attachments);
    *destination_failed_attachment_ids =
        std::move(source_failed_attachment_ids);
  }

  void CopyResultMetadata(
      AttachmentStore::Result* destination_result,
      std::unique_ptr<AttachmentMetadataList>* destination_metadata,
      const AttachmentStore::Result& source_result,
      std::unique_ptr<AttachmentMetadataList> source_metadata) {
    CopyResult(destination_result, source_result);
    *destination_metadata = std::move(source_metadata);
  }
};

TYPED_TEST_CASE_P(AttachmentStoreTest);

// Verify that CreateAttachmentStoreForSync() creates valid object.
TYPED_TEST_P(AttachmentStoreTest, CreateAttachmentStoreForSync) {
  std::unique_ptr<AttachmentStoreForSync> attachment_store_for_sync =
      this->store->CreateAttachmentStoreForSync();
  EXPECT_NE(nullptr, attachment_store_for_sync);
}

// Verify that we do not overwrite existing attachments and that we do not treat
// it as an error.
TYPED_TEST_P(AttachmentStoreTest, Write_NoOverwriteNoError) {
  // Create two attachments with the same id but different data.
  Attachment attachment1 = Attachment::Create(this->some_data1);
  Attachment attachment2 =
      Attachment::CreateFromParts(attachment1.GetId(), this->some_data2);

  // Write the first one.
  AttachmentList some_attachments;
  some_attachments.push_back(attachment1);
  this->store->Write(some_attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);

  // Write the second one.
  some_attachments.clear();
  some_attachments.push_back(attachment2);
  this->store->Write(some_attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);

  // Read it back and see that it was not overwritten.
  AttachmentIdList some_attachment_ids;
  some_attachment_ids.push_back(attachment1.GetId());
  this->store->Read(some_attachment_ids, this->read_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);
  EXPECT_EQ(1U, this->attachments->size());
  EXPECT_EQ(0U, this->failed_attachment_ids->size());
  AttachmentMap::const_iterator a1 =
      this->attachments->find(attachment1.GetId());
  EXPECT_TRUE(a1 != this->attachments->end());
  EXPECT_TRUE(attachment1.GetData()->Equals(a1->second.GetData()));
}

// Verify that we can write some attachments and read them back.
TYPED_TEST_P(AttachmentStoreTest, Write_RoundTrip) {
  Attachment attachment1 = Attachment::Create(this->some_data1);
  Attachment attachment2 = Attachment::Create(this->some_data2);
  AttachmentList some_attachments;
  some_attachments.push_back(attachment1);
  some_attachments.push_back(attachment2);

  this->store->Write(some_attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);

  AttachmentIdList some_attachment_ids;
  some_attachment_ids.push_back(attachment1.GetId());
  some_attachment_ids.push_back(attachment2.GetId());
  this->store->Read(some_attachment_ids, this->read_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);
  EXPECT_EQ(2U, this->attachments->size());
  EXPECT_EQ(0U, this->failed_attachment_ids->size());

  AttachmentMap::const_iterator a1 =
      this->attachments->find(attachment1.GetId());
  EXPECT_TRUE(a1 != this->attachments->end());
  EXPECT_TRUE(attachment1.GetData()->Equals(a1->second.GetData()));

  AttachmentMap::const_iterator a2 =
      this->attachments->find(attachment2.GetId());
  EXPECT_TRUE(a2 != this->attachments->end());
  EXPECT_TRUE(attachment2.GetData()->Equals(a2->second.GetData()));
}

// Try to read two attachments when only one exists.
TYPED_TEST_P(AttachmentStoreTest, Read_OneNotFound) {
  Attachment attachment1 = Attachment::Create(this->some_data1);
  Attachment attachment2 = Attachment::Create(this->some_data2);

  AttachmentList some_attachments;
  // Write attachment1 only.
  some_attachments.push_back(attachment1);
  this->store->Write(some_attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);

  // Try to read both attachment1 and attachment2.
  AttachmentIdList ids;
  ids.push_back(attachment1.GetId());
  ids.push_back(attachment2.GetId());
  this->store->Read(ids, this->read_callback);
  this->ClearAndPumpLoop();

  // See that only attachment1 was read.
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, this->result);
  EXPECT_EQ(1U, this->attachments->size());
  EXPECT_EQ(1U, this->failed_attachment_ids->size());
}

// Try to drop two attachments when only one exists. Verify that no error occurs
// and that the existing attachment was dropped.
TYPED_TEST_P(AttachmentStoreTest, Drop_DropTwoButOnlyOneExists) {
  // First, create two attachments.
  Attachment attachment1 = Attachment::Create(this->some_data1);
  Attachment attachment2 = Attachment::Create(this->some_data2);
  AttachmentList some_attachments;
  some_attachments.push_back(attachment1);
  some_attachments.push_back(attachment2);
  this->store->Write(some_attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);

  // Drop attachment1 only.
  AttachmentIdList ids;
  ids.push_back(attachment1.GetId());
  this->store->Drop(ids, this->drop_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);

  // See that attachment1 is gone.
  this->store->Read(ids, this->read_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, this->result);
  EXPECT_EQ(0U, this->attachments->size());
  EXPECT_EQ(1U, this->failed_attachment_ids->size());
  EXPECT_EQ(attachment1.GetId(), (*this->failed_attachment_ids)[0]);

  // Drop both attachment1 and attachment2.
  ids.clear();
  ids.push_back(attachment1.GetId());
  ids.push_back(attachment2.GetId());
  this->store->Drop(ids, this->drop_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);

  // See that attachment2 is now gone.
  ids.clear();
  ids.push_back(attachment2.GetId());
  this->store->Read(ids, this->read_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, this->result);
  EXPECT_EQ(0U, this->attachments->size());
  EXPECT_EQ(1U, this->failed_attachment_ids->size());
  EXPECT_EQ(attachment2.GetId(), (*this->failed_attachment_ids)[0]);
}

// Verify that attempting to drop an attachment that does not exist is not an
// error.
TYPED_TEST_P(AttachmentStoreTest, Drop_DoesNotExist) {
  Attachment attachment1 = Attachment::Create(this->some_data1);
  AttachmentList some_attachments;
  some_attachments.push_back(attachment1);
  this->store->Write(some_attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);

  // Drop the attachment.
  AttachmentIdList ids;
  ids.push_back(attachment1.GetId());
  this->store->Drop(ids, this->drop_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);

  // See that it's gone.
  this->store->Read(ids, this->read_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, this->result);
  EXPECT_EQ(0U, this->attachments->size());
  EXPECT_EQ(1U, this->failed_attachment_ids->size());
  EXPECT_EQ(attachment1.GetId(), (*this->failed_attachment_ids)[0]);

  // Drop again, see that no error occurs.
  ids.clear();
  ids.push_back(attachment1.GetId());
  this->store->Drop(ids, this->drop_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);
}

// Verify getting metadata for specific attachments.
TYPED_TEST_P(AttachmentStoreTest, ReadMetadataById) {
  Attachment attachment1 = Attachment::Create(this->some_data1);
  Attachment attachment2 = Attachment::Create(this->some_data2);

  AttachmentList some_attachments;
  // Write attachment1 only.
  some_attachments.push_back(attachment1);
  this->store->Write(some_attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);

  // Try to read metadata for both attachment1 and attachment2.
  AttachmentIdList ids;
  ids.push_back(attachment1.GetId());
  ids.push_back(attachment2.GetId());
  this->store->ReadMetadataById(ids, this->read_metadata_callback);
  this->ClearAndPumpLoop();

  // See that only one entry was read.
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, this->result);
  EXPECT_EQ(1U, this->attachment_metadata->size());

  // Now write attachment2.
  some_attachments[0] = attachment2;
  this->store->Write(some_attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);

  // Try to read metadata for both attachment1 and attachment2 again.
  this->store->ReadMetadataById(ids, this->read_metadata_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);
  EXPECT_EQ(2U, this->attachment_metadata->size());

  // Verify that we've got both entries back in the right order.
  AttachmentMetadataList::const_iterator iter =
      this->attachment_metadata->begin();
  EXPECT_EQ(attachment1.GetId(), iter->GetId());
  ++iter;
  EXPECT_EQ(attachment2.GetId(), iter->GetId());
}

// Verify that ReadMetadata/ReadMetadataForSync returns metadata for correct
// set of attachments.
TYPED_TEST_P(AttachmentStoreTest, ReadMetadata) {
  // Try to read all metadata from an empty store.
  this->store->ReadMetadata(this->read_metadata_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);
  EXPECT_EQ(0U, this->attachment_metadata->size());

  // Create and write attachments with different set of references.
  Attachment attachment_mt = Attachment::Create(this->some_data1);
  Attachment attachment_sync = Attachment::Create(this->some_data1);
  Attachment attachment_both = Attachment::Create(this->some_data1);

  AttachmentList attachments;
  attachments.push_back(attachment_mt);
  attachments.push_back(attachment_sync);
  attachments.push_back(attachment_both);
  this->store->Write(attachments, this->write_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);

  AttachmentIdList ids;
  ids.push_back(attachment_sync.GetId());
  ids.push_back(attachment_both.GetId());
  this->store_for_sync->SetSyncReference(ids);

  ids.clear();
  ids.push_back(attachment_sync.GetId());
  this->store->Drop(ids, this->drop_callback);
  this->ClearAndPumpLoop();

  // Calling ReadMetadataById for above three attachments should only return
  // attachments with model type reference.
  ids.clear();
  ids.push_back(attachment_mt.GetId());
  ids.push_back(attachment_sync.GetId());
  ids.push_back(attachment_both.GetId());
  this->store->ReadMetadataById(ids, this->read_metadata_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, this->result);
  EXPECT_EQ(2U, this->attachment_metadata->size());
  AttachmentIdSet model_type_id_set;
  model_type_id_set.insert(attachment_mt.GetId());
  model_type_id_set.insert(attachment_both.GetId());
  EXPECT_THAT(model_type_id_set,
              testing::Contains((*this->attachment_metadata)[0].GetId()));
  EXPECT_THAT(model_type_id_set,
              testing::Contains((*this->attachment_metadata)[1].GetId()));

  // Call to ReadMetadata() should only return attachments with model type
  // reference.
  this->store->ReadMetadata(this->read_metadata_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);
  EXPECT_EQ(2U, this->attachment_metadata->size());

  // Verify that we get all attachments back (the order is undefined).
  EXPECT_THAT(model_type_id_set,
              testing::Contains((*this->attachment_metadata)[0].GetId()));
  EXPECT_THAT(model_type_id_set,
              testing::Contains((*this->attachment_metadata)[1].GetId()));

  // Call to ReadMetadataForSync() should only return attachments with sync
  // reference.
  this->store_for_sync->ReadMetadataForSync(this->read_metadata_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);
  EXPECT_EQ(2U, this->attachment_metadata->size());

  AttachmentIdSet sync_id_set;
  sync_id_set.insert(attachment_sync.GetId());
  sync_id_set.insert(attachment_both.GetId());
  EXPECT_THAT(sync_id_set,
              testing::Contains((*this->attachment_metadata)[0].GetId()));
  EXPECT_THAT(sync_id_set,
              testing::Contains((*this->attachment_metadata)[1].GetId()));
}

// Verify that setting/droping references gets reflected in ReadMetadata and
// that attachment is only deleted after last reference is droped.
TYPED_TEST_P(AttachmentStoreTest, SetSyncReference_DropSyncReference) {
  Attachment attachment = Attachment::Create(this->some_data1);
  AttachmentList attachments;
  attachments.push_back(attachment);
  AttachmentIdList ids;
  ids.push_back(attachment.GetId());

  // When writing attachment to store only model type reference should be set.
  this->store->Write(attachments, this->write_callback);

  this->store->ReadMetadata(this->read_metadata_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);
  EXPECT_EQ(1U, this->attachment_metadata->size());
  EXPECT_EQ(attachment.GetId(), this->attachment_metadata->begin()->GetId());

  this->store_for_sync->ReadMetadataForSync(this->read_metadata_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);
  EXPECT_EQ(0U, this->attachment_metadata->size());

  // After call to SetSyncReference() ReadMetadataForSync should start returning
  // attachment.
  this->store_for_sync->SetSyncReference(ids);

  this->store_for_sync->ReadMetadataForSync(this->read_metadata_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);
  EXPECT_EQ(1U, this->attachment_metadata->size());

  // Call SetSyncReference() to verify it is idempotent.
  this->store_for_sync->SetSyncReference(ids);
  this->ClearAndPumpLoop();

  // Droping attachment should remove model type reference, but there is still
  // sync reference.
  this->store->Drop(ids, this->drop_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);

  this->store->ReadMetadata(this->read_metadata_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);
  EXPECT_EQ(0U, this->attachment_metadata->size());

  this->store_for_sync->ReadMetadataForSync(this->read_metadata_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);
  EXPECT_EQ(1U, this->attachment_metadata->size());

  // ReadMetadataById should return UNSPECIFIED_ERROR, attachment doesn't have
  // model type reference.
  this->store->ReadMetadataById(ids, this->read_metadata_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::UNSPECIFIED_ERROR, this->result);
  EXPECT_EQ(0U, this->attachment_metadata->size());

  // Call Drop() again to ensure it doesn't fail.
  this->store->Drop(ids, this->drop_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);

  // After droping sync reference attachment should be deleted from store.
  // ReadMetadataForSync should return empty result.
  this->store_for_sync->DropSyncReference(ids);

  this->store_for_sync->ReadMetadataForSync(this->read_metadata_callback);
  this->ClearAndPumpLoop();
  EXPECT_EQ(AttachmentStore::SUCCESS, this->result);
  EXPECT_EQ(0U, this->attachment_metadata->size());
}

REGISTER_TYPED_TEST_CASE_P(AttachmentStoreTest,
                           CreateAttachmentStoreForSync,
                           Write_NoOverwriteNoError,
                           Write_RoundTrip,
                           Read_OneNotFound,
                           Drop_DropTwoButOnlyOneExists,
                           Drop_DoesNotExist,
                           ReadMetadataById,
                           ReadMetadata,
                           SetSyncReference_DropSyncReference);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_ATTACHMENTS_ATTACHMENT_STORE_TEST_TEMPLATE_H_
