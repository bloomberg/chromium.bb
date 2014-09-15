// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/homedir_methods.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::WithArg;

namespace cryptohome {

namespace {

MATCHER_P(EqualsProto, expected_proto, "") {
  std::string expected_value;
  expected_proto.SerializeToString(&expected_value);
  std::string actual_value;
  arg.SerializeToString(&actual_value);
  return actual_value == expected_value;
}

}  // namespace

const char kUserID[] = "user@example.com";
const char kKeyLabel[] = "key_label";

const int64 kKeyRevision = 123;
const char kProviderData1Name[] = "data_1";
const int64 kProviderData1Number = 12345;
const char kProviderData2Name[] = "data_2";
const char kProviderData2Bytes[] = "data_2 bytes";

class HomedirMethodsTest : public testing::Test {
 public:
  HomedirMethodsTest();
  virtual ~HomedirMethodsTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void RunProtobufMethodCallback(
      const chromeos::CryptohomeClient::ProtobufMethodCallback& callback);

  void StoreGetKeyDataExResult(
      bool success,
      MountError return_code,
      const std::vector<KeyDefinition>& key_definitions);

 protected:
  chromeos::MockCryptohomeClient* cryptohome_client_;

  // The reply that |cryptohome_client_| will make.
  cryptohome::BaseReply cryptohome_reply_;

  // The results of the most recent |HomedirMethods| method call.
  bool success_;
  MountError return_code_;
  std::vector<KeyDefinition> key_definitions_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HomedirMethodsTest);
};

HomedirMethodsTest::HomedirMethodsTest() : cryptohome_client_(NULL),
                                           success_(false),
                                           return_code_(MOUNT_ERROR_FATAL) {
}

HomedirMethodsTest::~HomedirMethodsTest() {
}

void HomedirMethodsTest::SetUp() {
  scoped_ptr<chromeos::MockCryptohomeClient> cryptohome_client(
      new chromeos::MockCryptohomeClient);
  cryptohome_client_ = cryptohome_client.get();
  chromeos::DBusThreadManager::GetSetterForTesting()->SetCryptohomeClient(
      cryptohome_client.PassAs<chromeos::CryptohomeClient>());
  HomedirMethods::Initialize();
}

void HomedirMethodsTest::TearDown() {
  HomedirMethods::Shutdown();
  chromeos::DBusThreadManager::Shutdown();
}

void HomedirMethodsTest::RunProtobufMethodCallback(
    const chromeos::CryptohomeClient::ProtobufMethodCallback& callback) {
  callback.Run(chromeos::DBUS_METHOD_CALL_SUCCESS,
               true,
               cryptohome_reply_);
}

void HomedirMethodsTest::StoreGetKeyDataExResult(
    bool success,
    MountError return_code,
    const std::vector<KeyDefinition>& key_definitions) {
  success_ = success;
  return_code_ = return_code;
  key_definitions_ = key_definitions;
}

// Verifies that the result of a GetKeyDataEx() call is correctly parsed.
TEST_F(HomedirMethodsTest, GetKeyDataEx) {
  AccountIdentifier expected_id;
  expected_id.set_email(kUserID);
  const cryptohome::AuthorizationRequest expected_auth;
  cryptohome::GetKeyDataRequest expected_request;
      expected_request.mutable_key()->mutable_data()->set_label(kKeyLabel);

  EXPECT_CALL(*cryptohome_client_,
              GetKeyDataEx(EqualsProto(expected_id),
                           EqualsProto(expected_auth),
                           EqualsProto(expected_request),
                           _))
      .Times(1)
      .WillOnce(WithArg<3>(Invoke(
          this,
          &HomedirMethodsTest::RunProtobufMethodCallback)));

  // Set up the reply that |cryptohome_client_| will make.
  cryptohome::GetKeyDataReply* reply =
      cryptohome_reply_.MutableExtension(cryptohome::GetKeyDataReply::reply);
  KeyData* key_data = reply->add_key_data();
  key_data->set_type(KeyData::KEY_TYPE_PASSWORD);
  key_data->set_label(kKeyLabel);
  key_data->mutable_privileges()->set_update(false);
  key_data->set_revision(kKeyRevision);
  key_data->add_authorization_data()->set_type(
      KeyAuthorizationData::KEY_AUTHORIZATION_TYPE_HMACSHA256);
  KeyProviderData* data = key_data->mutable_provider_data();
  KeyProviderData::Entry* entry = data->add_entry();
  entry->set_name(kProviderData1Name);
  entry->set_number(kProviderData1Number);
  entry = data->add_entry();
  entry->set_name(kProviderData2Name);
  entry->set_bytes(kProviderData2Bytes);

  // Call GetKeyDataEx().
  HomedirMethods::GetInstance()->GetKeyDataEx(
    Identification(kUserID),
    kKeyLabel,
    base::Bind(&HomedirMethodsTest::StoreGetKeyDataExResult,
               base::Unretained(this)));

  // Verify that the call was successful and the result was correctly parsed.
  EXPECT_TRUE(success_);
  EXPECT_EQ(MOUNT_ERROR_NONE, return_code_);
  ASSERT_EQ(1u, key_definitions_.size());
  const KeyDefinition& key_definition = key_definitions_.front();
  EXPECT_EQ(KeyDefinition::TYPE_PASSWORD, key_definition.type);
  EXPECT_EQ(PRIV_MOUNT | PRIV_ADD | PRIV_REMOVE,
            key_definition.privileges);
  EXPECT_EQ(kKeyRevision, key_definition.revision);
  ASSERT_EQ(1u, key_definition.authorization_data.size());
  EXPECT_EQ(KeyDefinition::AuthorizationData::TYPE_HMACSHA256,
            key_definition.authorization_data.front().type);
  ASSERT_EQ(2u, key_definition.provider_data.size());
  const KeyDefinition::ProviderData* provider_data =
      &key_definition.provider_data[0];
  EXPECT_EQ(kProviderData1Name, provider_data->name);
  ASSERT_TRUE(provider_data->number);
  EXPECT_EQ(kProviderData1Number, *provider_data->number.get());
  EXPECT_FALSE(provider_data->bytes);
  provider_data = &key_definition.provider_data[1];
  EXPECT_EQ(kProviderData2Name, provider_data->name);
  EXPECT_FALSE(provider_data->number);
  ASSERT_TRUE(provider_data->bytes);
  EXPECT_EQ(kProviderData2Bytes, *provider_data->bytes.get());
}

}  // namespace cryptohome
