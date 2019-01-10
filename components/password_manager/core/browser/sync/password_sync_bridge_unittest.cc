// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_sync_bridge.h"

#include "components/sync/model/fake_model_type_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;

namespace password_manager {

namespace {

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

}  // namespace

class PasswordSyncBridgeTest : public testing::Test {
 public:
  PasswordSyncBridgeTest()
      : bridge_(std::make_unique<syncer::FakeModelTypeChangeProcessor>()) {}
  ~PasswordSyncBridgeTest() override {}

  PasswordSyncBridge* bridge() { return &bridge_; }

 private:
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

}  // namespace password_manager
