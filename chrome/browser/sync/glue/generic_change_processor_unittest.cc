// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/generic_change_processor.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/sync/glue/data_type_error_handler_mock.h"
#include "sync/api/fake_syncable_service.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_merge_result.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/internal_api/public/user_share.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

class SyncGenericChangeProcessorTest : public testing::Test {
 public:
  // It doesn't matter which type we use.  Just pick one.
  static const syncer::ModelType kType = syncer::PREFERENCES;

  SyncGenericChangeProcessorTest() :
      loop_(base::MessageLoop::TYPE_UI),
      sync_merge_result_(kType),
      merge_result_ptr_factory_(&sync_merge_result_),
      syncable_service_ptr_factory_(&fake_syncable_service_) {
  }

  virtual void SetUp() OVERRIDE {
    test_user_share_.SetUp();
    syncer::ModelTypeSet types = syncer::ProtocolTypes();
    for (syncer::ModelTypeSet::Iterator iter = types.First(); iter.Good();
         iter.Inc()) {
      syncer::TestUserShare::CreateRoot(iter.Get(),
                                        test_user_share_.user_share());
    }
    test_user_share_.encryption_handler()->Init();
    change_processor_.reset(
        new GenericChangeProcessor(
            &data_type_error_handler_,
            syncable_service_ptr_factory_.GetWeakPtr(),
            merge_result_ptr_factory_.GetWeakPtr(),
            test_user_share_.user_share()));
  }

  virtual void TearDown() OVERRIDE {
    test_user_share_.TearDown();
  }

  void BuildChildNodes(int n) {
    syncer::WriteTransaction trans(FROM_HERE, user_share());
    syncer::ReadNode root(&trans);
    ASSERT_EQ(syncer::BaseNode::INIT_OK,
              root.InitByTagLookup(syncer::ModelTypeToRootTag(kType)));
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

 private:
  base::MessageLoop loop_;

  syncer::SyncMergeResult sync_merge_result_;
  base::WeakPtrFactory<syncer::SyncMergeResult> merge_result_ptr_factory_;

  syncer::FakeSyncableService fake_syncable_service_;
  base::WeakPtrFactory<syncer::FakeSyncableService>
      syncable_service_ptr_factory_;

  DataTypeErrorHandlerMock data_type_error_handler_;
  syncer::TestUserShare test_user_share_;

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

}  // namespace

}  // namespace browser_sync

