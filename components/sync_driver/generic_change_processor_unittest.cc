// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/generic_change_processor.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/sync_driver/data_type_error_handler_mock.h"
#include "components/sync_driver/fake_sync_client.h"
#include "components/sync_driver/local_device_info_provider.h"
#include "components/sync_driver/sync_api_component_factory.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/attachments/attachment_store.h"
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
      std::unique_ptr<syncer::AttachmentStoreForSync> attachment_store);
  ~MockAttachmentService() override;
  void UploadAttachments(
      const syncer::AttachmentIdList& attachment_ids) override;
  std::vector<syncer::AttachmentIdList>* attachment_id_lists();

 private:
  std::vector<syncer::AttachmentIdList> attachment_id_lists_;
};

MockAttachmentService::MockAttachmentService(
    std::unique_ptr<syncer::AttachmentStoreForSync> attachment_store)
    : AttachmentServiceImpl(std::move(attachment_store),
                            std::unique_ptr<syncer::AttachmentUploader>(
                                new syncer::FakeAttachmentUploader),
                            std::unique_ptr<syncer::AttachmentDownloader>(
                                new syncer::FakeAttachmentDownloader),
                            NULL,
                            base::TimeDelta(),
                            base::TimeDelta()) {}

MockAttachmentService::~MockAttachmentService() {
}

void MockAttachmentService::UploadAttachments(
    const syncer::AttachmentIdList& attachment_ids) {
  attachment_id_lists_.push_back(attachment_ids);
  AttachmentServiceImpl::UploadAttachments(attachment_ids);
}

std::vector<syncer::AttachmentIdList>*
MockAttachmentService::attachment_id_lists() {
  return &attachment_id_lists_;
}

// MockSyncApiComponentFactory needed to initialize GenericChangeProcessor and
// pass MockAttachmentService to it.
class MockSyncApiComponentFactory : public SyncApiComponentFactory {
 public:
  MockSyncApiComponentFactory() {}

  // SyncApiComponentFactory implementation.
  void RegisterDataTypes(
      sync_driver::SyncService* sync_service,
      const RegisterDataTypesMethod& register_platform_types_method) override {}
  sync_driver::DataTypeManager* CreateDataTypeManager(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const sync_driver::DataTypeController::TypeMap* controllers,
      const sync_driver::DataTypeEncryptionHandler* encryption_handler,
      browser_sync::SyncBackendHost* backend,
      sync_driver::DataTypeManagerObserver* observer) override{
    return nullptr;
  };
  browser_sync::SyncBackendHost* CreateSyncBackendHost(
      const std::string& name,
      invalidation::InvalidationService* invalidator,
      const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
      const base::FilePath& sync_folder) override {
    return nullptr;
  }
  std::unique_ptr<sync_driver::LocalDeviceInfoProvider>
  CreateLocalDeviceInfoProvider() override {
    return nullptr;
  }
  SyncComponents CreateBookmarkSyncComponents(
      sync_driver::SyncService* sync_service,
      sync_driver::DataTypeErrorHandler* error_handler) override {
    return SyncComponents(nullptr, nullptr);
  }

  std::unique_ptr<syncer::AttachmentService> CreateAttachmentService(
      std::unique_ptr<syncer::AttachmentStoreForSync> attachment_store,
      const syncer::UserShare& user_share,
      const std::string& store_birthday,
      syncer::ModelType model_type,
      syncer::AttachmentService::Delegate* delegate) override {
    std::unique_ptr<MockAttachmentService> attachment_service(
        new MockAttachmentService(std::move(attachment_store)));
    // GenericChangeProcessor takes ownership of the AttachmentService, but we
    // need to have a pointer to it so we can see that it was used properly.
    // Take a pointer and trust that GenericChangeProcessor does not prematurely
    // destroy it.
    mock_attachment_service_ = attachment_service.get();
    return std::move(attachment_service);
  }

  MockAttachmentService* GetMockAttachmentService() {
    return mock_attachment_service_;
  }

 private:
  MockAttachmentService* mock_attachment_service_;
};

class SyncGenericChangeProcessorTest : public testing::Test {
 public:
  // Most test cases will use this type.  For those that need a
  // GenericChangeProcessor for a different type, use |InitializeForType|.
  static const syncer::ModelType kType = syncer::PREFERENCES;

  SyncGenericChangeProcessorTest()
      : syncable_service_ptr_factory_(&fake_syncable_service_),
        mock_attachment_service_(NULL),
        sync_client_(&sync_factory_) {}

  void SetUp() override {
    // Use kType by default, but allow test cases to re-initialize with whatever
    // type they choose.  Therefore, it's important that all type dependent
    // initialization occurs in InitializeForType.
    InitializeForType(kType);
  }

  void TearDown() override {
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
    std::unique_ptr<syncer::AttachmentStore> attachment_store =
        syncer::AttachmentStore::CreateInMemoryStore();
    change_processor_.reset(new GenericChangeProcessor(
        type, &data_type_error_handler_,
        syncable_service_ptr_factory_.GetWeakPtr(),
        merge_result_ptr_factory_->GetWeakPtr(), test_user_share_->user_share(),
        &sync_client_, attachment_store->CreateAttachmentStoreForSync()));
    mock_attachment_service_ = sync_factory_.GetMockAttachmentService();
  }

  void BuildChildNodes(syncer::ModelType type, int n) {
    syncer::WriteTransaction trans(FROM_HERE, user_share());
    for (int i = 0; i < n; ++i) {
      syncer::WriteNode node(&trans);
      node.InitUniqueByCreation(type, base::StringPrintf("node%05d", i));
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

  std::unique_ptr<syncer::SyncMergeResult> sync_merge_result_;
  std::unique_ptr<base::WeakPtrFactory<syncer::SyncMergeResult>>
      merge_result_ptr_factory_;

  syncer::FakeSyncableService fake_syncable_service_;
  base::WeakPtrFactory<syncer::FakeSyncableService>
      syncable_service_ptr_factory_;

  DataTypeErrorHandlerMock data_type_error_handler_;
  std::unique_ptr<syncer::TestUserShare> test_user_share_;
  MockAttachmentService* mock_attachment_service_;
  FakeSyncClient sync_client_;
  MockSyncApiComponentFactory sync_factory_;

  std::unique_ptr<GenericChangeProcessor> change_processor_;
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
  attachment_ids.push_back(syncer::AttachmentId::Create(0, 0));
  attachment_ids.push_back(syncer::AttachmentId::Create(0, 0));

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
  ASSERT_EQ(mock_attachment_service()->attachment_id_lists()->size(), 1U);
  const syncer::AttachmentIdList& attachments_added =
      mock_attachment_service()->attachment_id_lists()->front();
  ASSERT_THAT(
      attachments_added,
      testing::UnorderedElementsAre(attachment_ids[0], attachment_ids[1]));

  // Update the SyncData, replacing its two attachments with one new attachment.
  syncer::AttachmentIdList new_attachment_ids;
  new_attachment_ids.push_back(syncer::AttachmentId::Create(0, 0));
  mock_attachment_service()->attachment_id_lists()->clear();
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
  ASSERT_EQ(mock_attachment_service()->attachment_id_lists()->size(), 1U);
  const syncer::AttachmentIdList& new_attachments_added =
      mock_attachment_service()->attachment_id_lists()->front();
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
  attachment_ids.push_back(syncer::AttachmentId::Create(0, 0));

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
  syncer::AttachmentId id1 = syncer::AttachmentId::Create(0, 0);
  syncer::AttachmentId id2 = syncer::AttachmentId::Create(0, 0);
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
  ASSERT_EQ(1U, mock_attachment_service()->attachment_id_lists()->size());
  ASSERT_THAT(mock_attachment_service()->attachment_id_lists()->front(),
              testing::UnorderedElementsAre(id1));
}

// Test that attempting to add an entry that already exists still works.
TEST_F(SyncGenericChangeProcessorTest, AddExistingEntry) {
  InitializeForType(syncer::SESSIONS);
  sync_pb::EntitySpecifics sessions_specifics;
  sessions_specifics.mutable_session()->set_session_tag("session tag");
  syncer::SyncChangeList changes;

  // First add it normally.
  changes.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_ADD,
      syncer::SyncData::CreateLocalData(base::StringPrintf("tag"),
                                        base::StringPrintf("title"),
                                        sessions_specifics)));
  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, changes).IsSet());

  // Now attempt to add it again, but with different specifics. Should not
  // result in an error and should still update the specifics.
  sessions_specifics.mutable_session()->set_session_tag("session tag 2");
  changes[0] =
      syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_ADD,
                         syncer::SyncData::CreateLocalData(
                             base::StringPrintf("tag"),
                             base::StringPrintf("title"), sessions_specifics));
  ASSERT_FALSE(
      change_processor()->ProcessSyncChanges(FROM_HERE, changes).IsSet());

  // Verify the data was updated properly.
  syncer::SyncDataList sync_data =
      change_processor()->GetAllSyncData(syncer::SESSIONS);
  ASSERT_EQ(sync_data.size(), 1U);
  ASSERT_EQ("session tag 2",
            sync_data[0].GetSpecifics().session().session_tag());
}

}  // namespace

}  // namespace sync_driver
