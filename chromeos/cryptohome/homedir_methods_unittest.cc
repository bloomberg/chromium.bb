// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/homedir_methods.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cryptohome {

const char kUserID[] = "user@example.com";
const char kKeyLabel[] = "key_label";

const int64_t kKeyRevision = 123;
const char kProviderData1Name[] = "data_1";
const int64_t kProviderData1Number = 12345;
const char kProviderData2Name[] = "data_2";
const char kProviderData2Bytes[] = "data_2 bytes";

class HomedirMethodsTest : public testing::Test {
 public:
  HomedirMethodsTest() = default;
  ~HomedirMethodsTest() override = default;

  // testing::Test:
  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    HomedirMethods::Initialize();
  }

  void TearDown() override {
    HomedirMethods::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HomedirMethodsTest);
};

// Verifies that the result of a GetKeyDataEx() call is correctly parsed.
TEST_F(HomedirMethodsTest, GetKeyDataEx) {
  // Set up the pseudo KeyData.
  AddKeyRequest request;
  KeyData* key_data = request.mutable_key()->mutable_data();
  key_data->set_type(KeyData::KEY_TYPE_PASSWORD);
  key_data->set_label(kKeyLabel);
  key_data->mutable_privileges()->set_update(false);
  key_data->set_revision(kKeyRevision);
  key_data->add_authorization_data()->set_type(
      KeyAuthorizationData::KEY_AUTHORIZATION_TYPE_HMACSHA256);
  KeyProviderData* data = key_data->mutable_provider_data();
  KeyProviderData::Entry* entry1 = data->add_entry();
  entry1->set_name(kProviderData1Name);
  entry1->set_number(kProviderData1Number);
  KeyProviderData::Entry* entry2 = data->add_entry();
  entry2->set_name(kProviderData2Name);
  entry2->set_bytes(kProviderData2Bytes);
  chromeos::DBusThreadManager::Get()->GetCryptohomeClient()->AddKeyEx(
      cryptohome::Identification(AccountId::FromUserEmail(kUserID)),
      AuthorizationRequest(), request,
      base::Bind([](base::Optional<BaseReply> result) {
        ASSERT_TRUE(result.has_value());
      }));
  ASSERT_NO_FATAL_FAILURE(base::RunLoop().RunUntilIdle());

  // Call GetKeyDataEx().
  std::vector<KeyDefinition> key_definitions;
  cryptohome::GetKeyDataRequest get_key_data_request;
  request.mutable_key()->mutable_data()->set_label(kKeyLabel);
  HomedirMethods::GetInstance()->GetKeyDataEx(
      Identification(AccountId::FromUserEmail(kUserID)), AuthorizationRequest(),
      get_key_data_request,
      base::Bind(
          [](std::vector<KeyDefinition>* out_key_definitions, bool success,
             MountError return_code,
             const std::vector<KeyDefinition>& key_definitions) {
            EXPECT_TRUE(success);
            EXPECT_EQ(MOUNT_ERROR_NONE, return_code);
            *out_key_definitions = key_definitions;
          },
          &key_definitions));
  base::RunLoop().RunUntilIdle();

  // Verify that the call was successful and the result was correctly parsed.
  ASSERT_EQ(1u, key_definitions.size());
  const KeyDefinition& key_definition = key_definitions.front();
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
