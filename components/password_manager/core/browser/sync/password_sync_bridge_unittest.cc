// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_sync_bridge.h"

#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mock_model_type_change_processor.h"
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

}  // namespace

class PasswordSyncBridgeTest : public testing::Test {
 public:
  PasswordSyncBridgeTest()
      : bridge_(mock_processor_.CreateForwardingProcessor()) {}
  ~PasswordSyncBridgeTest() override {}

  PasswordSyncBridge* bridge() { return &bridge_; }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

 private:
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;
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

  EXPECT_CALL(mock_processor(), Put("1", HasSignonRealm(kSignonRealm1), _));
  EXPECT_CALL(mock_processor(), Put("2", HasSignonRealm(kSignonRealm2), _));
  EXPECT_CALL(mock_processor(), Delete("3", _));

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
