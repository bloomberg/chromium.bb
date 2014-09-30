// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/generic_change_processor.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/sync_driver/data_type_error_handler_mock.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/attachments/fake_attachment_store.h"
#include "sync/api/fake_syncable_service.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_merge_result.h"
#include "sync/internal_api/public/attachments/attachment_service_impl.h"
#include "sync/internal_api/public/attachments/fake_attachment_downloader.h"
#include "sync/internal_api/public/attachments/fake_attachment_uploader.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/sync_encryption_handler.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_driver {

namespace {

// A mock that keeps track of attachments passed to UploadAttachments.
class MockAttachmentService : public syncer::AttachmentServiceImpl {
 public:
  MockAttachmentService(
      const scoped_refptr<syncer::AttachmentStore>& attachment_store);
  virtual ~MockAttachmentService();
  virtual void UploadAttachments(
      const syncer::AttachmentIdSet& attachment_ids) OVERRIDE;
  std::vector<syncer::AttachmentIdSet>* attachment_id_sets();

 private:
  std::vector<syncer::AttachmentIdSet> attachment_id_sets_;
};

MockAttachmentService::MockAttachmentService(
    const scoped_refptr<syncer::AttachmentStore>& attachment_store)
    : AttachmentServiceImpl(attachment_store,
                            scoped_ptr<syncer::AttachmentUploader>(
                                new syncer::FakeAttachmentUploader),
                            scoped_ptr<syncer::AttachmentDownloader>(
                                new syncer::FakeAttachmentDownloader),
                            NULL,
                            base::TimeDelta(),
                            base::TimeDelta()) {
}

MockAttachmentService::~MockAttachmentService() {
}

void MockAttachmentService::UploadAttachments(
    const syncer::AttachmentIdSet& attachment_ids) {
  attachment_id_sets_.push_back(attachment_ids);
  AttachmentServiceImpl::UploadAttachments(attachment_ids);
}

std::vector<syncer::AttachmentIdSet>*
MockAttachmentService::attachment_id_sets() {
  return &attachment_id_sets_;
}

// MockSyncApiComponentFactory needed to initialize GenericChangeProcessor and
// pass MockAttachmentService to it.
class MockSyncApiComponentFactory : public SyncApiComponentFactory {
 public:
  MockSyncApiComponentFactory(
      scoped_ptr<syncer::AttachmentService> attachment_service)
      : attachment_service_(attachment_service.Pass()) {}

  virtual base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) OVERRIDE {
    // Shouldn't be called for this test.
    NOTREACHED();
    return base::WeakPtr<syncer::SyncableService>();
  }

  virtual scoped_ptr<syncer::AttachmentService> CreateAttachmentService(
      const scoped_refptr<syncer::AttachmentStore>& attachment_store,
      const syncer::UserShare& user_share,
      syncer::AttachmentService::Delegate* delegate) OVERRIDE {
    EXPECT_TRUE(attachment_service_ != NULL);
    return attachment_service_.Pass();
  }

 private:
  scoped_ptr<syncer::AttachmentService> attachment_service_;
};

class SyncGenericChangeProcessorTest : public testing::Test {
 public:
  // Most test cases will use this type.  For those that need a
  // GenericChangeProcessor for a different type, use |InitializeForType|.
  static const syncer::ModelType kType = syncer::PREFERENCES;

  SyncGenericChangeProcessorTest()
      : syncable_service_ptr_factory_(&fake_syncable_service_),
        mock_attachment_service_(NULL) {}

  virtual void SetUp() OVERRIDE {
    // Use kType by default, but allow test cases to re-initialize with whatever
    // type they choose.  Therefore, it's important that all type dependent
    // initialization occurs in InitializeForType.
    InitializeForType(kType);
  }

  virtual void TearDown() OVERRIDE {
    mock_attachment_service_ = NULL;
    if (test_user_share_) {
      test_user_share_->TearDown();
    }
  }

  // Initialize GenericChangeProcessor and related classes for testing with
  // model type |type|.
  void InitializeForType(syncer::ModelType type) {
    TearDown();
    test_user_share_.reset(new syncer::TestUserShare);
    test_user_share_->SetUp();
    sync_merge_result_.reset(new syncer::SyncMergeResult(type));
    merge_result_ptr_factory_.reset(
        new base::WeakPtrFactory<syncer::SyncMergeResult>(
            sync_merge_result_.get()));

    syncer::ModelTypeSet types = syncer::ProtocolTypes();
    for (syncer::ModelTypeSet::Iterator iter = types.First(); iter.Good();
         iter.Inc()) {
      syncer::TestUserShare::CreateRoot(iter.Get(),
                                        test_user_share_->user_share());
    }
    test_user_share_->encryption_handler()->Init();
    ConstructGenericChangeProcessor(type);
  }

  void ConstructGenericChangeProcessor(syncer::ModelType type) {
    scoped_refptr<syncer::AttachmentStore> attachment_store(
        new syncer::FakeAttachmentStore(base::MessageLoopProxy::current()));
    scoped_ptr<MockAttachmentService> mock_attachment_service(
        new MockAttachmentService(attachment_store));
    // GenericChangeProcessor takes ownership of the AttachmentService, but we
    // need to have a pointer to it so we can see that it was used properly.
    // Take a pointer and trust that GenericChangeProcessor does not prematurely
    // destroy it.
    mock_attachment_service_ = mock_attachment_service.get();
    sync_factory_.reset(new MockSyncApiComponentFactory(
        mock_attachment_service.PassAs<syncer::AttachmentService>()));
    change_processor_.reset(
        new GenericChangeProcessor(type,
                                   &data_type_error_handler_,
                                   syncable_service_ptr_factory_.GetWeakPtr(),
                                   merge_result_ptr_factory_->GetWeakPtr(),
                                   test_user_share_->user_share(),
                                   sync_factory_.get(),
                                   attachment_store));
  }

  void BuildChildNodes(syncer::ModelType type, int n) {
    syncer::WriteTransaction trans(FROM_HERE, user_share());
    syncer::ReadNode root(&trans);
    ASSERT_EQ(syncer::BaseNode::INIT_OK, root.InitTypeRoot(type));
    for (int i = 0; i < n; ++i) {
      syncer::WriteNode node(&trans);
      node.InitUniqueByCreation(type, root, base::StringPrintf("node%05d", i));
    }
  }

  GenericChangeProcessor* change_processor() {
    return change_processor_.get();
  }

  syncer::UserShare* user_share() {
    return test_user_share_->user_share();
  }

  MockAttachmentService* mock_attachment_service() {
    return mock_attachment_service_;
  }

  void RunLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 private:
  base::MessageLoopForUI loop_;

  scoped_ptr<syncer::SyncMergeResult> sync_merge_result_;
  scoped_ptr<base::WeakPtrFactory<syncer::SyncMergeResult> >
      merge_result_ptr_factory_;

  syncer::FakeSyncableService fake_syncable_service_;
  base::WeakPtrFactory<syncer::FakeSyncableService>
      syncable_service_ptr_factory_;

  DataTypeErrorHandlerMock data_type_error_handler_;
  scoped_ptr<syncer::TestUserShare> test_user_share_;
  MockAttachmentService* mock_attachment_service_;
  scoped_ptr<SyncApiComponentFactory> sync_factory_;

  scoped_ptr<GenericChangeProcessor> change_processor_;
};

// Similar to above, but focused on the method that implements sync/api
// interfaces and is hence exposed to datatypes directly.
TEST_F(SyncGenericChangeProcessorTest, StressGetAllSyncData) {
  const int kNumChildNodes = 1000;
  const int kRepeatCount = 1;

  ASSERT_NO_FATAL_FAILURE(BuildChildNodes(kType, kNumChildNodes));

  for (int i = 0; i < kRepeatCount; ++i) {
    syncer::SyncDataList sync_data =
        change_processor()->GetAllSyncData(kType);

    // Start with a simple test.  We can add more in-depth testing later.
    EXPECT_EQ(static_cast<size_t>(kNumChildNodes), sync_data.size());
  }
}

TEST_F(SyncGenericChangeProcessorTest, SetGetPasswords) {
  InitializeForType(syncer::PASSWORDS);
  const int kNumPasswords = 10;
  sync_pb::PasswordSpecificsData password_data;
  password_data.set_username_value("user");

  sync_pb::EntitySpecifics password_holder;

  syncer::SyncChangeList change_list;
  for (int i = 0; i < kNumPasswords; ++i) {
    password_data.set_password_value(
        base::StringPrintf("password%i", i));
    password_holder.mutable_password()->mutable_client_only_encrypted_data()->
        CopyFrom(password_data);
    change_list.push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_ADD,
                           syncer::SyncData::CreateLocalData(
                               base::StringPrintf("tag%i", i),
                               base::StringPrintf("title%i", i),
                               password_holder)));
  }

  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, change_list).IsSet());

  syncer::SyncDataList password_list(
      change_processor()->GetAllSyncData(syncer::PASSWORDS));

  ASSERT_EQ(password_list.size(), change_list.size());
  for (int i = 0; i < kNumPasswords; ++i) {
    // Verify the password is returned properly.
    ASSERT_TRUE(password_list[i].GetSpecifics().has_password());
    ASSERT_TRUE(password_list[i].GetSpecifics().password().
                    has_client_only_encrypted_data());
    ASSERT_FALSE(password_list[i].GetSpecifics().password().has_encrypted());
    const sync_pb::PasswordSpecificsData& sync_password =
        password_list[i].GetSpecifics().password().client_only_encrypted_data();
    const sync_pb::PasswordSpecificsData& change_password =
        change_list[i].sync_data().GetSpecifics().password().
            client_only_encrypted_data();
    ASSERT_EQ(sync_password.password_value(), change_password.password_value());
    ASSERT_EQ(sync_password.username_value(), change_password.username_value());

    // Verify the raw sync data was stored securely.
    syncer::ReadTransaction read_transaction(FROM_HERE, user_share());
    syncer::ReadNode node(&read_transaction);
    ASSERT_EQ(node.InitByClientTagLookup(syncer::PASSWORDS,
                                         base::StringPrintf("tag%i", i)),
              syncer::BaseNode::INIT_OK);
    ASSERT_EQ(node.GetTitle(), "encrypted");
    const sync_pb::EntitySpecifics& raw_specifics = node.GetEntitySpecifics();
    ASSERT_TRUE(raw_specifics.has_password());
    ASSERT_TRUE(raw_specifics.password().has_encrypted());
    ASSERT_FALSE(raw_specifics.password().has_client_only_encrypted_data());
  }
}

TEST_F(SyncGenericChangeProcessorTest, UpdatePasswords) {
  InitializeForType(syncer::PASSWORDS);
  const int kNumPasswords = 10;
  sync_pb::PasswordSpecificsData password_data;
  password_data.set_username_value("user");

  sync_pb::EntitySpecifics password_holder;

  syncer::SyncChangeList change_list;
  syncer::SyncChangeList change_list2;
  for (int i = 0; i < kNumPasswords; ++i) {
    password_data.set_password_value(
        base::StringPrintf("password%i", i));
    password_holder.mutable_password()->mutable_client_only_encrypted_data()->
        CopyFrom(password_data);
    change_list.push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_ADD,
                           syncer::SyncData::CreateLocalData(
                               base::StringPrintf("tag%i", i),
                               base::StringPrintf("title%i", i),
                               password_holder)));
    password_data.set_password_value(
        base::StringPrintf("password_m%i", i));
    password_holder.mutable_password()->mutable_client_only_encrypted_data()->
        CopyFrom(password_data);
    change_list2.push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_UPDATE,
                           syncer::SyncData::CreateLocalData(
                               base::StringPrintf("tag%i", i),
                               base::StringPrintf("title_m%i", i),
                               password_holder)));
  }

  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, change_list).IsSet());
  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, change_list2).IsSet());

  syncer::SyncDataList password_list(
      change_processor()->GetAllSyncData(syncer::PASSWORDS));

  ASSERT_EQ(password_list.size(), change_list2.size());
  for (int i = 0; i < kNumPasswords; ++i) {
    // Verify the password is returned properly.
    ASSERT_TRUE(password_list[i].GetSpecifics().has_password());
    ASSERT_TRUE(password_list[i].GetSpecifics().password().
                    has_client_only_encrypted_data());
    ASSERT_FALSE(password_list[i].GetSpecifics().password().has_encrypted());
    const sync_pb::PasswordSpecificsData& sync_password =
        password_list[i].GetSpecifics().password().client_only_encrypted_data();
    const sync_pb::PasswordSpecificsData& change_password =
        change_list2[i].sync_data().GetSpecifics().password().
            client_only_encrypted_data();
    ASSERT_EQ(sync_password.password_value(), change_password.password_value());
    ASSERT_EQ(sync_password.username_value(), change_password.username_value());

    // Verify the raw sync data was stored securely.
    syncer::ReadTransaction read_transaction(FROM_HERE, user_share());
    syncer::ReadNode node(&read_transaction);
    ASSERT_EQ(node.InitByClientTagLookup(syncer::PASSWORDS,
                                         base::StringPrintf("tag%i", i)),
              syncer::BaseNode::INIT_OK);
    ASSERT_EQ(node.GetTitle(), "encrypted");
    const sync_pb::EntitySpecifics& raw_specifics = node.GetEntitySpecifics();
    ASSERT_TRUE(raw_specifics.has_password());
    ASSERT_TRUE(raw_specifics.password().has_encrypted());
    ASSERT_FALSE(raw_specifics.password().has_client_only_encrypted_data());
  }
}

// Verify that attachments on newly added or updated SyncData are passed to the
// AttachmentService.
TEST_F(SyncGenericChangeProcessorTest,
       ProcessSyncChanges_AddUpdateWithAttachment) {
  std::string tag = "client_tag";
  std::string title = "client_title";
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref_specifics = specifics.mutable_preference();
  pref_specifics->set_name("test");

  syncer::AttachmentIdList attachment_ids;
  attachment_ids.push_back(syncer::AttachmentId::Create());
  attachment_ids.push_back(syncer::AttachmentId::Create());

  // Add a SyncData with two attachments.
  syncer::SyncChangeList change_list;
  change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         syncer::SyncData::CreateLocalDataWithAttachments(
                             tag, title, specifics, attachment_ids)));
  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, change_list).IsSet());
  RunLoop();

  // Check that the AttachmentService received the new attachments.
  ASSERT_EQ(mock_attachment_service()->attachment_id_sets()->size(), 1U);
  const syncer::AttachmentIdSet& attachments_added =
      mock_attachment_service()->attachment_id_sets()->front();
  ASSERT_THAT(
      attachments_added,
      testing::UnorderedElementsAre(attachment_ids[0], attachment_ids[1]));

  // Update the SyncData, replacing its two attachments with one new attachment.
  syncer::AttachmentIdList new_attachment_ids;
  new_attachment_ids.push_back(syncer::AttachmentId::Create());
  mock_attachment_service()->attachment_id_sets()->clear();
  change_list.clear();
  change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_UPDATE,
                         syncer::SyncData::CreateLocalDataWithAttachments(
                             tag, title, specifics, new_attachment_ids)));
  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, change_list).IsSet());
  RunLoop();

  // Check that the AttachmentService received it.
  ASSERT_EQ(mock_attachment_service()->attachment_id_sets()->size(), 1U);
  const syncer::AttachmentIdSet& new_attachments_added =
      mock_attachment_service()->attachment_id_sets()->front();
  ASSERT_THAT(new_attachments_added,
              testing::UnorderedElementsAre(new_attachment_ids[0]));
}

// Verify that after attachment is uploaded GenericChangeProcessor updates
// corresponding entries
TEST_F(SyncGenericChangeProcessorTest, AttachmentUploaded) {
  std::string tag = "client_tag";
  std::string title = "client_title";
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref_specifics = specifics.mutable_preference();
  pref_specifics->set_name("test");

  syncer::AttachmentIdList attachment_ids;
  attachment_ids.push_back(syncer::AttachmentId::Create());

  // Add a SyncData with two attachments.
  syncer::SyncChangeList change_list;
  change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         syncer::SyncData::CreateLocalDataWithAttachments(
                             tag, title, specifics, attachment_ids)));
  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, change_list).IsSet());

  sync_pb::AttachmentIdProto attachment_id_proto = attachment_ids[0].GetProto();
  syncer::AttachmentId attachment_id =
      syncer::AttachmentId::CreateFromProto(attachment_id_proto);

  change_processor()->OnAttachmentUploaded(attachment_id);
  syncer::ReadTransaction read_transaction(FROM_HERE, user_share());
  syncer::ReadNode node(&read_transaction);
  ASSERT_EQ(node.InitByClientTagLookup(kType, tag), syncer::BaseNode::INIT_OK);
  attachment_ids = node.GetAttachmentIds();
  EXPECT_EQ(1U, attachment_ids.size());
}

// Verify that upon construction, all attachments not yet on the server are
// scheduled for upload.
TEST_F(SyncGenericChangeProcessorTest, UploadAllAttachmentsNotOnServer) {
  // Create two attachment ids.  id2 will be marked as "on server".
  syncer::AttachmentId id1 = syncer::AttachmentId::Create();
  syncer::AttachmentId id2 = syncer::AttachmentId::Create();
  {
    // Write an entry containing these two attachment ids.
    syncer::WriteTransaction trans(FROM_HERE, user_share());
    syncer::ReadNode root(&trans);
    ASSERT_EQ(syncer::BaseNode::INIT_OK, root.InitTypeRoot(kType));
    syncer::WriteNode node(&trans);
    node.InitUniqueByCreation(kType, root, "some node");
    sync_pb::AttachmentMetadata metadata;
    sync_pb::AttachmentMetadataRecord* record1 = metadata.add_record();
    *record1->mutable_id() = id1.GetProto();
    sync_pb::AttachmentMetadataRecord* record2 = metadata.add_record();
    *record2->mutable_id() = id2.GetProto();
    record2->set_is_on_server(true);
    node.SetAttachmentMetadata(metadata);
  }

  // Construct the GenericChangeProcessor and see that it asks the
  // AttachmentService to upload id1 only.
  ConstructGenericChangeProcessor(kType);
  ASSERT_EQ(1U, mock_attachment_service()->attachment_id_sets()->size());
  ASSERT_THAT(mock_attachment_service()->attachment_id_sets()->front(),
              testing::UnorderedElementsAre(id1));
}

}  // namespace

}  // namespace sync_driver
