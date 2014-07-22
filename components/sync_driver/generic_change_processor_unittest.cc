// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/generic_change_processor.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "components/sync_driver/data_type_error_handler_mock.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "sync/api/attachments/attachment_service_impl.h"
#include "sync/api/fake_syncable_service.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_merge_result.h"
#include "sync/internal_api/public/attachments/fake_attachment_downloader.h"
#include "sync/internal_api/public/attachments/fake_attachment_store.h"
#include "sync/internal_api/public/attachments/fake_attachment_uploader.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/sync_encryption_handler.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_driver {

namespace {

const char kTestData[] = "some data";

// A mock that keeps track of attachments passed to StoreAttachments.
class MockAttachmentService : public syncer::AttachmentServiceImpl {
 public:
  MockAttachmentService();
  virtual ~MockAttachmentService();
  virtual void StoreAttachments(const syncer::AttachmentList& attachments,
                                const StoreCallback& callback) OVERRIDE;
  std::vector<syncer::AttachmentList>* attachment_lists();

 private:
  std::vector<syncer::AttachmentList> attachment_lists_;
};

MockAttachmentService::MockAttachmentService()
    : AttachmentServiceImpl(
          scoped_ptr<syncer::AttachmentStore>(new syncer::FakeAttachmentStore(
              base::MessageLoopProxy::current())),
          scoped_ptr<syncer::AttachmentUploader>(
              new syncer::FakeAttachmentUploader),
          scoped_ptr<syncer::AttachmentDownloader>(
              new syncer::FakeAttachmentDownloader),
          NULL) {
}

MockAttachmentService::~MockAttachmentService() {
}

void MockAttachmentService::StoreAttachments(
    const syncer::AttachmentList& attachments,
    const StoreCallback& callback) {
  attachment_lists_.push_back(attachments);
  AttachmentServiceImpl::StoreAttachments(attachments, callback);
}

std::vector<syncer::AttachmentList>* MockAttachmentService::attachment_lists() {
  return &attachment_lists_;
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
  // It doesn't matter which type we use.  Just pick one.
  static const syncer::ModelType kType = syncer::PREFERENCES;

  SyncGenericChangeProcessorTest()
      : sync_merge_result_(kType),
        merge_result_ptr_factory_(&sync_merge_result_),
        syncable_service_ptr_factory_(&fake_syncable_service_),
        mock_attachment_service_(NULL) {}

  virtual void SetUp() OVERRIDE {
    test_user_share_.SetUp();
    syncer::ModelTypeSet types = syncer::ProtocolTypes();
    for (syncer::ModelTypeSet::Iterator iter = types.First(); iter.Good();
         iter.Inc()) {
      syncer::TestUserShare::CreateRoot(iter.Get(),
                                        test_user_share_.user_share());
    }
    test_user_share_.encryption_handler()->Init();
    scoped_ptr<MockAttachmentService> mock_attachment_service(
        new MockAttachmentService);
    // GenericChangeProcessor takes ownership of the AttachmentService, but we
    // need to have a pointer to it so we can see that it was used properly.
    // Take a pointer and trust that GenericChangeProcessor does not prematurely
    // destroy it.
    mock_attachment_service_ = mock_attachment_service.get();
    sync_factory_.reset(new MockSyncApiComponentFactory(
        mock_attachment_service.PassAs<syncer::AttachmentService>()));
    change_processor_.reset(
        new GenericChangeProcessor(&data_type_error_handler_,
                                   syncable_service_ptr_factory_.GetWeakPtr(),
                                   merge_result_ptr_factory_.GetWeakPtr(),
                                   test_user_share_.user_share(),
                                   sync_factory_.get()));
  }

  virtual void TearDown() OVERRIDE {
    mock_attachment_service_ = NULL;
    test_user_share_.TearDown();
  }

  void BuildChildNodes(int n) {
    syncer::WriteTransaction trans(FROM_HERE, user_share());
    syncer::ReadNode root(&trans);
    ASSERT_EQ(syncer::BaseNode::INIT_OK, root.InitTypeRoot(kType));
    for (int i = 0; i < n; ++i) {
      syncer::WriteNode node(&trans);
      node.InitUniqueByCreation(kType, root, base::StringPrintf("node%05d", i));
    }
  }

  GenericChangeProcessor* change_processor() {
    return change_processor_.get();
  }

  syncer::UserShare* user_share() {
    return test_user_share_.user_share();
  }

  MockAttachmentService* mock_attachment_service() {
    return mock_attachment_service_;
  }

 private:
  base::MessageLoopForUI loop_;

  syncer::SyncMergeResult sync_merge_result_;
  base::WeakPtrFactory<syncer::SyncMergeResult> merge_result_ptr_factory_;

  syncer::FakeSyncableService fake_syncable_service_;
  base::WeakPtrFactory<syncer::FakeSyncableService>
      syncable_service_ptr_factory_;

  DataTypeErrorHandlerMock data_type_error_handler_;
  syncer::TestUserShare test_user_share_;
  MockAttachmentService* mock_attachment_service_;
  scoped_ptr<SyncApiComponentFactory> sync_factory_;

  scoped_ptr<GenericChangeProcessor> change_processor_;
};

// Similar to above, but focused on the method that implements sync/api
// interfaces and is hence exposed to datatypes directly.
TEST_F(SyncGenericChangeProcessorTest, StressGetAllSyncData) {
  const int kNumChildNodes = 1000;
  const int kRepeatCount = 1;

  ASSERT_NO_FATAL_FAILURE(BuildChildNodes(kNumChildNodes));

  for (int i = 0; i < kRepeatCount; ++i) {
    syncer::SyncDataList sync_data =
        change_processor()->GetAllSyncData(kType);

    // Start with a simple test.  We can add more in-depth testing later.
    EXPECT_EQ(static_cast<size_t>(kNumChildNodes), sync_data.size());
  }
}

TEST_F(SyncGenericChangeProcessorTest, SetGetPasswords) {
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
  syncer::AttachmentList attachments;
  scoped_refptr<base::RefCountedString> attachment_data =
      new base::RefCountedString;
  attachment_data->data() = kTestData;
  attachments.push_back(syncer::Attachment::Create(attachment_data));
  attachments.push_back(syncer::Attachment::Create(attachment_data));

  // Add a SyncData with two attachments.
  syncer::SyncChangeList change_list;
  change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         syncer::SyncData::CreateLocalDataWithAttachments(
                             tag, title, specifics, attachments)));
  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, change_list).IsSet());

  // Check that the AttachmentService received the new attachments.
  ASSERT_EQ(mock_attachment_service()->attachment_lists()->size(), 1U);
  const syncer::AttachmentList& attachments_added =
      mock_attachment_service()->attachment_lists()->front();
  ASSERT_EQ(attachments_added.size(), 2U);
  ASSERT_EQ(attachments_added[0].GetId(), attachments[0].GetId());
  ASSERT_EQ(attachments_added[1].GetId(), attachments[1].GetId());

  // Update the SyncData, replacing its two attachments with one new attachment.
  syncer::AttachmentList new_attachments;
  new_attachments.push_back(syncer::Attachment::Create(attachment_data));
  mock_attachment_service()->attachment_lists()->clear();
  change_list.clear();
  change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_UPDATE,
                         syncer::SyncData::CreateLocalDataWithAttachments(
                             tag, title, specifics, new_attachments)));
  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, change_list).IsSet());

  // Check that the AttachmentService received it.
  ASSERT_EQ(mock_attachment_service()->attachment_lists()->size(), 1U);
  const syncer::AttachmentList& new_attachments_added =
      mock_attachment_service()->attachment_lists()->front();
  ASSERT_EQ(new_attachments_added.size(), 1U);
  ASSERT_EQ(new_attachments_added[0].GetId(), new_attachments[0].GetId());
}

// Verify that after attachment is uploaded GenericChangeProcessor updates
// corresponding entries
TEST_F(SyncGenericChangeProcessorTest, AttachmentUploaded) {
  std::string tag = "client_tag";
  std::string title = "client_title";
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref_specifics = specifics.mutable_preference();
  pref_specifics->set_name("test");
  syncer::AttachmentList attachments;
  scoped_refptr<base::RefCountedString> attachment_data =
      new base::RefCountedString;
  attachment_data->data() = kTestData;
  attachments.push_back(syncer::Attachment::Create(attachment_data));

  // Add a SyncData with two attachments.
  syncer::SyncChangeList change_list;
  change_list.push_back(
      syncer::SyncChange(FROM_HERE,
                         syncer::SyncChange::ACTION_ADD,
                         syncer::SyncData::CreateLocalDataWithAttachments(
                             tag, title, specifics, attachments)));
  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, change_list).IsSet());

  sync_pb::AttachmentIdProto attachment_id_proto =
      attachments[0].GetId().GetProto();
  syncer::AttachmentId attachment_id =
      syncer::AttachmentId::CreateFromProto(attachment_id_proto);

  change_processor()->OnAttachmentUploaded(attachment_id);
  syncer::ReadTransaction read_transaction(FROM_HERE, user_share());
  syncer::ReadNode node(&read_transaction);
  ASSERT_EQ(node.InitByClientTagLookup(syncer::PREFERENCES, tag),
            syncer::BaseNode::INIT_OK);
  syncer::AttachmentIdList attachment_ids = node.GetAttachmentIds();
  EXPECT_EQ(1U, attachment_ids.size());
}

}  // namespace

}  // namespace sync_driver
