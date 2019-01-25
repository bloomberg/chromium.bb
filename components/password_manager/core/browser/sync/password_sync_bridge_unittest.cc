// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_sync_bridge.h"

#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using testing::_;
using testing::Eq;
using testing::Return;

constexpr char kSignonRealm1[] = "abc";
constexpr char kSignonRealm2[] = "def";
constexpr char kSignonRealm3[] = "xyz";

// |*args| must be of type EntityData.
MATCHER_P(HasSignonRealm, expected_signon_realm, "") {
  return arg->specifics.password()
             .client_only_encrypted_data()
             .signon_realm() == expected_signon_realm;
}

// |*args| must be of type SyncMetadataStoreChangeList.
MATCHER_P(IsSyncMetadataStoreChangeListWithStore, expected_metadata_store, "") {
  return static_cast<const syncer::SyncMetadataStoreChangeList*>(arg)
             ->GetMetadataStoreForTesting() == expected_metadata_store;
}

sync_pb::PasswordSpecifics CreateSpecifics(const std::string& origin,
                                           const std::string& username_element,
                                           const std::string& username_value,
                                           const std::string& password_element,
                                           const std::string& signon_realm) {
  sync_pb::EntitySpecifics password_specifics;
  sync_pb::PasswordSpecificsData* password_data =
      password_specifics.mutable_password()
          ->mutable_client_only_encrypted_data();
  password_data->set_origin(origin);
  password_data->set_username_element(username_element);
  password_data->set_username_value(username_value);
  password_data->set_password_element(password_element);
  password_data->set_signon_realm(signon_realm);
  return password_specifics.password();
}

autofill::PasswordForm MakePasswordForm(const std::string& signon_realm) {
  autofill::PasswordForm form;
  form.signon_realm = signon_realm;
  return form;
}

class MockSyncMetadataStore : public syncer::SyncMetadataStore {
 public:
  MockSyncMetadataStore() = default;
  ~MockSyncMetadataStore() = default;

  MOCK_METHOD3(UpdateSyncMetadata,
               bool(syncer::ModelType,
                    const std::string&,
                    const sync_pb::EntityMetadata&));
  MOCK_METHOD2(ClearSyncMetadata, bool(syncer::ModelType, const std::string&));
  MOCK_METHOD2(UpdateModelTypeState,
               bool(syncer::ModelType, const sync_pb::ModelTypeState&));
  MOCK_METHOD1(ClearModelTypeState, bool(syncer::ModelType));
};

class MockPasswordStoreSync : public PasswordStoreSync {
 public:
  MockPasswordStoreSync() = default;
  ~MockPasswordStoreSync() = default;

  MOCK_METHOD1(FillAutofillableLogins,
               bool(std::vector<std::unique_ptr<autofill::PasswordForm>>*));
  MOCK_METHOD1(FillBlacklistLogins,
               bool(std::vector<std::unique_ptr<autofill::PasswordForm>>*));
  MOCK_METHOD1(ReadAllLogins, bool(PrimaryKeyToFormMap*));
  MOCK_METHOD1(RemoveLoginByPrimaryKeySync, PasswordStoreChangeList(int));
  MOCK_METHOD0(DeleteUndecryptableLogins, DatabaseCleanupResult());
  MOCK_METHOD1(AddLoginSync,
               PasswordStoreChangeList(const autofill::PasswordForm&));
  MOCK_METHOD1(UpdateLoginSync,
               PasswordStoreChangeList(const autofill::PasswordForm&));
  MOCK_METHOD1(RemoveLoginSync,
               PasswordStoreChangeList(const autofill::PasswordForm&));
  MOCK_METHOD1(NotifyLoginsChanged, void(const PasswordStoreChangeList&));
  MOCK_METHOD0(BeginTransaction, bool());
  MOCK_METHOD0(CommitTransaction, bool());
  MOCK_METHOD0(GetMetadataStore, syncer::SyncMetadataStore*());
};

}  // namespace

class PasswordSyncBridgeTest : public testing::Test {
 public:
  PasswordSyncBridgeTest()
      : bridge_(mock_processor_.CreateForwardingProcessor(),
                &mock_password_store_sync_) {
    ON_CALL(mock_password_store_sync_, GetMetadataStore())
        .WillByDefault(testing::Return(&mock_sync_metadata_store_sync_));

    ON_CALL(mock_sync_metadata_store_sync_, UpdateSyncMetadata(_, _, _))
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_sync_metadata_store_sync_, ClearSyncMetadata(_, _))
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_sync_metadata_store_sync_, UpdateModelTypeState(_, _))
        .WillByDefault(testing::Return(true));
    ON_CALL(mock_sync_metadata_store_sync_, ClearModelTypeState(_))
        .WillByDefault(testing::Return(true));
  }

  ~PasswordSyncBridgeTest() override {}

  PasswordSyncBridge* bridge() { return &bridge_; }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  MockPasswordStoreSync* mock_password_store_sync() {
    return &mock_password_store_sync_;
  }

 private:
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
  testing::NiceMock<MockSyncMetadataStore> mock_sync_metadata_store_sync_;
  testing::NiceMock<MockPasswordStoreSync> mock_password_store_sync_;
  PasswordSyncBridge bridge_;
};

TEST_F(PasswordSyncBridgeTest, ShouldComputeClientTagHash) {
  syncer::EntityData data;
  *data.specifics.mutable_password() =
      CreateSpecifics("http://www.origin.com", "username_element",
                      "username_value", "password_element", "signon_realm");

  EXPECT_THAT(
      bridge()->GetClientTag(data),
      Eq("http%3A//www.origin.com/"
         "|username_element|username_value|password_element|signon_realm"));
}

TEST_F(PasswordSyncBridgeTest, ShouldForwardLocalChangesToTheProcessor) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(true));

  PasswordStoreChangeList changes;
  changes.push_back(PasswordStoreChange(
      PasswordStoreChange::ADD, MakePasswordForm(kSignonRealm1), /*id=*/1));
  changes.push_back(PasswordStoreChange(
      PasswordStoreChange::UPDATE, MakePasswordForm(kSignonRealm2), /*id=*/2));
  changes.push_back(PasswordStoreChange(
      PasswordStoreChange::REMOVE, MakePasswordForm(kSignonRealm3), /*id=*/3));
  syncer::SyncMetadataStore* store =
      mock_password_store_sync()->GetMetadataStore();
  EXPECT_CALL(mock_processor(),
              Put("1", HasSignonRealm(kSignonRealm1),
                  IsSyncMetadataStoreChangeListWithStore(store)));
  EXPECT_CALL(mock_processor(),
              Put("2", HasSignonRealm(kSignonRealm2),
                  IsSyncMetadataStoreChangeListWithStore(store)));
  EXPECT_CALL(mock_processor(),
              Delete("3", IsSyncMetadataStoreChangeListWithStore(store)));

  bridge()->ActOnPasswordStoreChanges(changes);
}

TEST_F(PasswordSyncBridgeTest,
       ShouldNotForwardLocalChangesToTheProcessorIfSyncDisabled) {
  ON_CALL(mock_processor(), IsTrackingMetadata()).WillByDefault(Return(false));

  PasswordStoreChangeList changes;
  changes.push_back(PasswordStoreChange(
      PasswordStoreChange::ADD, MakePasswordForm(kSignonRealm1), /*id=*/1));
  changes.push_back(PasswordStoreChange(
      PasswordStoreChange::UPDATE, MakePasswordForm(kSignonRealm2), /*id=*/2));
  changes.push_back(PasswordStoreChange(
      PasswordStoreChange::REMOVE, MakePasswordForm(kSignonRealm3), /*id=*/3));

  EXPECT_CALL(mock_processor(), Put(_, _, _)).Times(0);
  EXPECT_CALL(mock_processor(), Delete(_, _)).Times(0);

  bridge()->ActOnPasswordStoreChanges(changes);
}

}  // namespace password_manager
