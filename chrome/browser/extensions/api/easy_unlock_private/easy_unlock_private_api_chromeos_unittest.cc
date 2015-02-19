// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_api.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/signin/easy_unlock_app_manager.h"
#include "chrome/browser/signin/easy_unlock_service_factory.h"
#include "chrome/browser/signin/easy_unlock_service_regular.h"
#include "chrome/common/extensions/api/easy_unlock_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_easy_unlock_client.h"
#include "extensions/browser/event_router.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

namespace api = extensions::api::easy_unlock_private;

using extensions::api::EasyUnlockPrivateGenerateEcP256KeyPairFunction;
using extensions::api::EasyUnlockPrivatePerformECDHKeyAgreementFunction;
using extensions::api::EasyUnlockPrivateCreateSecureMessageFunction;
using extensions::api::EasyUnlockPrivateUnwrapSecureMessageFunction;
using extensions::api::EasyUnlockPrivateSetAutoPairingResultFunction;

// Converts a string to a base::BinaryValue value whose buffer contains the
// string data without the trailing '\0'.
base::BinaryValue* StringToBinaryValue(const std::string value) {
  return base::BinaryValue::CreateWithCopiedBuffer(value.data(),
                                                   value.length());
}

// Copies |private_key_source| and |public_key_source| to |private_key_target|
// and |public_key_target|. It is used as a callback for
// |EasyUnlockClient::GenerateEcP256KeyPair| to save the values returned by the
// method.
void CopyKeyPair(std::string* private_key_target,
                 std::string* public_key_target,
                 const std::string& private_key_source,
                 const std::string& public_key_source) {
  *private_key_target = private_key_source;
  *public_key_target = public_key_source;
}

// Copies |data_source| to |data_target|. Used as a callback to EasyUnlockClient
// methods to save the data returned by the method.
void CopyData(std::string* data_target, const std::string& data_source) {
  *data_target = data_source;
}

class EasyUnlockPrivateApiTest : public extensions::ExtensionApiUnittest {
 public:
  EasyUnlockPrivateApiTest() {}
  ~EasyUnlockPrivateApiTest() override {}

 protected:
  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    client_ = chromeos::DBusThreadManager::Get()->GetEasyUnlockClient();

    extensions::ExtensionApiUnittest::SetUp();
  }

  void TearDown() override {
    extensions::ExtensionApiUnittest::TearDown();

    chromeos::DBusThreadManager::Shutdown();
  }

  // Extracts a single binary value result from a run extension function
  // |function| and converts it to string.
  // It will fail if the extension doesn't have exactly one binary value result.
  // On failure, an empty string is returned.
  std::string GetSingleBinaryResultAsString(
        UIThreadExtensionFunction* function) {
    const base::ListValue* result_list = function->GetResultList();
    if (!result_list) {
      LOG(ERROR) << "Function has no result list.";
      return "";
    }

    if (result_list->GetSize() != 1u) {
      LOG(ERROR) << "Invalid number of results.";
      return "";
    }

    const base::BinaryValue* result_binary_value;
    if (!result_list->GetBinary(0, &result_binary_value) ||
        !result_binary_value) {
      LOG(ERROR) << "Result not a binary value.";
      return "";
    }

    return std::string(result_binary_value->GetBuffer(),
                       result_binary_value->GetSize());
  }

  chromeos::EasyUnlockClient* client_;
};

TEST_F(EasyUnlockPrivateApiTest, GenerateEcP256KeyPair) {
  scoped_refptr<EasyUnlockPrivateGenerateEcP256KeyPairFunction> function(
      new EasyUnlockPrivateGenerateEcP256KeyPairFunction());
  function->set_has_callback(true);

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(),
      "[]",
      browser(),
      extension_function_test_utils::NONE));

  const base::ListValue* result_list = function->GetResultList();
  ASSERT_TRUE(result_list);
  ASSERT_EQ(2u, result_list->GetSize());

  const base::BinaryValue* public_key;
  ASSERT_TRUE(result_list->GetBinary(0, &public_key));
  ASSERT_TRUE(public_key);

  const base::BinaryValue* private_key;
  ASSERT_TRUE(result_list->GetBinary(1, &private_key));
  ASSERT_TRUE(private_key);

  EXPECT_TRUE(chromeos::FakeEasyUnlockClient::IsEcP256KeyPair(
      std::string(private_key->GetBuffer(), private_key->GetSize()),
      std::string(public_key->GetBuffer(), public_key->GetSize())));
}

TEST_F(EasyUnlockPrivateApiTest, PerformECDHKeyAgreement) {
  scoped_refptr<EasyUnlockPrivatePerformECDHKeyAgreementFunction> function(
      new EasyUnlockPrivatePerformECDHKeyAgreementFunction());
  function->set_has_callback(true);

  std::string private_key_1;
  std::string public_key_1_unused;
  client_->GenerateEcP256KeyPair(
      base::Bind(&CopyKeyPair, &private_key_1, &public_key_1_unused));

  std::string private_key_2_unused;
  std::string public_key_2;
  client_->GenerateEcP256KeyPair(
      base::Bind(&CopyKeyPair, &private_key_2_unused, &public_key_2));

  std::string expected_result;
  client_->PerformECDHKeyAgreement(
      private_key_1,
      public_key_2,
      base::Bind(&CopyData, &expected_result));
  ASSERT_GT(expected_result.length(), 0u);

  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(StringToBinaryValue(private_key_1));
  args->Append(StringToBinaryValue(public_key_2));

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(),
      args.Pass(),
      browser(),
      extension_function_test_utils::NONE));

  EXPECT_EQ(expected_result, GetSingleBinaryResultAsString(function.get()));
}

TEST_F(EasyUnlockPrivateApiTest, CreateSecureMessage) {
  scoped_refptr<EasyUnlockPrivateCreateSecureMessageFunction> function(
      new EasyUnlockPrivateCreateSecureMessageFunction());
  function->set_has_callback(true);

  chromeos::EasyUnlockClient::CreateSecureMessageOptions create_options;
  create_options.key = "KEY";
  create_options.associated_data = "ASSOCIATED_DATA";
  create_options.public_metadata = "PUBLIC_METADATA";
  create_options.verification_key_id = "VERIFICATION_KEY_ID";
  create_options.decryption_key_id = "DECRYPTION_KEY_ID";
  create_options.encryption_type = easy_unlock::kEncryptionTypeAES256CBC;
  create_options.signature_type = easy_unlock::kSignatureTypeHMACSHA256;

  std::string expected_result;
  client_->CreateSecureMessage(
      "PAYLOAD",
      create_options,
      base::Bind(&CopyData, &expected_result));
  ASSERT_GT(expected_result.length(), 0u);

  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(StringToBinaryValue("PAYLOAD"));
  args->Append(StringToBinaryValue("KEY"));
  base::DictionaryValue* options = new base::DictionaryValue();
  args->Append(options);
  options->Set("associatedData", StringToBinaryValue("ASSOCIATED_DATA"));
  options->Set("publicMetadata", StringToBinaryValue("PUBLIC_METADATA"));
  options->Set("verificationKeyId",
               StringToBinaryValue("VERIFICATION_KEY_ID"));
  options->Set("decryptionKeyId",
               StringToBinaryValue("DECRYPTION_KEY_ID"));
  options->SetString(
      "encryptType",
      api::ToString(api::ENCRYPTION_TYPE_AES_256_CBC));
  options->SetString(
      "signType",
      api::ToString(api::SIGNATURE_TYPE_HMAC_SHA256));

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(),
      args.Pass(),
      browser(),
      extension_function_test_utils::NONE));

  EXPECT_EQ(expected_result, GetSingleBinaryResultAsString(function.get()));
}

TEST_F(EasyUnlockPrivateApiTest, CreateSecureMessage_EmptyOptions) {
  scoped_refptr<EasyUnlockPrivateCreateSecureMessageFunction> function(
      new EasyUnlockPrivateCreateSecureMessageFunction());
  function->set_has_callback(true);

  chromeos::EasyUnlockClient::CreateSecureMessageOptions create_options;
  create_options.key = "KEY";
  create_options.encryption_type = easy_unlock::kEncryptionTypeNone;
  create_options.signature_type = easy_unlock::kSignatureTypeHMACSHA256;

  std::string expected_result;
  client_->CreateSecureMessage(
      "PAYLOAD",
      create_options,
      base::Bind(&CopyData, &expected_result));
  ASSERT_GT(expected_result.length(), 0u);

  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(StringToBinaryValue("PAYLOAD"));
  args->Append(StringToBinaryValue("KEY"));
  base::DictionaryValue* options = new base::DictionaryValue();
  args->Append(options);

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(),
      args.Pass(),
      browser(),
      extension_function_test_utils::NONE));

  EXPECT_EQ(expected_result, GetSingleBinaryResultAsString(function.get()));
}

TEST_F(EasyUnlockPrivateApiTest, CreateSecureMessage_AsymmetricSign) {
  scoped_refptr<EasyUnlockPrivateCreateSecureMessageFunction> function(
      new EasyUnlockPrivateCreateSecureMessageFunction());
  function->set_has_callback(true);

  chromeos::EasyUnlockClient::CreateSecureMessageOptions create_options;
  create_options.key = "KEY";
  create_options.associated_data = "ASSOCIATED_DATA";
  create_options.verification_key_id = "VERIFICATION_KEY_ID";
  create_options.encryption_type = easy_unlock::kEncryptionTypeNone;
  create_options.signature_type = easy_unlock::kSignatureTypeECDSAP256SHA256;

  std::string expected_result;
  client_->CreateSecureMessage(
      "PAYLOAD",
      create_options,
      base::Bind(&CopyData, &expected_result));
  ASSERT_GT(expected_result.length(), 0u);

  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(StringToBinaryValue("PAYLOAD"));
  args->Append(StringToBinaryValue("KEY"));
  base::DictionaryValue* options = new base::DictionaryValue();
  args->Append(options);
  options->Set("associatedData",
               StringToBinaryValue("ASSOCIATED_DATA"));
  options->Set("verificationKeyId",
               StringToBinaryValue("VERIFICATION_KEY_ID"));
  options->SetString(
      "signType",
      api::ToString(api::SIGNATURE_TYPE_ECDSA_P256_SHA256));

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(),
      args.Pass(),
      browser(),
      extension_function_test_utils::NONE));

  EXPECT_EQ(expected_result, GetSingleBinaryResultAsString(function.get()));
}

TEST_F(EasyUnlockPrivateApiTest, UnwrapSecureMessage) {
  scoped_refptr<EasyUnlockPrivateUnwrapSecureMessageFunction> function(
      new EasyUnlockPrivateUnwrapSecureMessageFunction());
  function->set_has_callback(true);

  chromeos::EasyUnlockClient::UnwrapSecureMessageOptions unwrap_options;
  unwrap_options.key = "KEY";
  unwrap_options.associated_data = "ASSOCIATED_DATA";
  unwrap_options.encryption_type = easy_unlock::kEncryptionTypeAES256CBC;
  unwrap_options.signature_type = easy_unlock::kSignatureTypeHMACSHA256;

  std::string expected_result;
  client_->UnwrapSecureMessage(
      "MESSAGE",
      unwrap_options,
      base::Bind(&CopyData, &expected_result));
  ASSERT_GT(expected_result.length(), 0u);

  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(StringToBinaryValue("MESSAGE"));
  args->Append(StringToBinaryValue("KEY"));
  base::DictionaryValue* options = new base::DictionaryValue();
  args->Append(options);
  options->Set("associatedData", StringToBinaryValue("ASSOCIATED_DATA"));
  options->SetString(
      "encryptType",
      api::ToString(api::ENCRYPTION_TYPE_AES_256_CBC));
  options->SetString(
      "signType",
      api::ToString(api::SIGNATURE_TYPE_HMAC_SHA256));

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(),
      args.Pass(),
      browser(),
      extension_function_test_utils::NONE));

  EXPECT_EQ(expected_result, GetSingleBinaryResultAsString(function.get()));
}

TEST_F(EasyUnlockPrivateApiTest, UnwrapSecureMessage_EmptyOptions) {
  scoped_refptr<EasyUnlockPrivateUnwrapSecureMessageFunction> function(
      new EasyUnlockPrivateUnwrapSecureMessageFunction());
  function->set_has_callback(true);

  chromeos::EasyUnlockClient::UnwrapSecureMessageOptions unwrap_options;
  unwrap_options.key = "KEY";
  unwrap_options.encryption_type = easy_unlock::kEncryptionTypeNone;
  unwrap_options.signature_type = easy_unlock::kSignatureTypeHMACSHA256;

  std::string expected_result;
  client_->UnwrapSecureMessage(
      "MESSAGE",
      unwrap_options,
      base::Bind(&CopyData, &expected_result));
  ASSERT_GT(expected_result.length(), 0u);

  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(StringToBinaryValue("MESSAGE"));
  args->Append(StringToBinaryValue("KEY"));
  base::DictionaryValue* options = new base::DictionaryValue();
  args->Append(options);

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(),
      args.Pass(),
      browser(),
      extension_function_test_utils::NONE));

  EXPECT_EQ(expected_result, GetSingleBinaryResultAsString(function.get()));
}

TEST_F(EasyUnlockPrivateApiTest, UnwrapSecureMessage_AsymmetricSign) {
  scoped_refptr<EasyUnlockPrivateUnwrapSecureMessageFunction> function(
      new EasyUnlockPrivateUnwrapSecureMessageFunction());
  function->set_has_callback(true);

  chromeos::EasyUnlockClient::UnwrapSecureMessageOptions unwrap_options;
  unwrap_options.key = "KEY";
  unwrap_options.associated_data = "ASSOCIATED_DATA";
  unwrap_options.encryption_type = easy_unlock::kEncryptionTypeNone;
  unwrap_options.signature_type = easy_unlock::kSignatureTypeECDSAP256SHA256;

  std::string expected_result;
  client_->UnwrapSecureMessage(
      "MESSAGE",
      unwrap_options,
      base::Bind(&CopyData, &expected_result));
  ASSERT_GT(expected_result.length(), 0u);

  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(StringToBinaryValue("MESSAGE"));
  args->Append(StringToBinaryValue("KEY"));
  base::DictionaryValue* options = new base::DictionaryValue();
  args->Append(options);
  options->Set("associatedData",
               StringToBinaryValue("ASSOCIATED_DATA"));
  options->SetString(
      "signType",
      api::ToString(api::SIGNATURE_TYPE_ECDSA_P256_SHA256));

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(),
      args.Pass(),
      browser(),
      extension_function_test_utils::NONE));

  EXPECT_EQ(expected_result, GetSingleBinaryResultAsString(function.get()));
}

struct AutoPairingResult {
  AutoPairingResult() : success(false) {}

  void SetResult(bool success, const std::string& error) {
    this->success = success;
    this->error = error;
  }

  bool success;
  std::string error;
};

// Test factory to register EasyUnlockService.
KeyedService* BuildTestEasyUnlockService(content::BrowserContext* context) {
  EasyUnlockService* service =
      new EasyUnlockServiceRegular(static_cast<Profile*>(context));
  service->Initialize(
      EasyUnlockAppManager::Create(extensions::ExtensionSystem::Get(context),
                                   -1 /* manifest id*/, base::FilePath()));
  return service;
}

// A fake EventRouter that logs event it dispatches for testing.
class FakeEventRouter : public extensions::EventRouter {
 public:
  FakeEventRouter(Profile* profile, extensions::ExtensionPrefs* extension_prefs)
      : EventRouter(profile, extension_prefs),
        event_count_(0) {}

  void DispatchEventToExtension(const std::string& extension_id,
                                scoped_ptr<extensions::Event> event) override {
    ++event_count_;
    last_extension_id_ = extension_id;
    last_event_name_ = event ? event->event_name : std::string();
  }

  int event_count() const { return event_count_; }
  const std::string& last_extension_id() const { return last_extension_id_; }
  const std::string& last_event_name() const { return last_event_name_; }

 private:
  int event_count_;
  std::string last_extension_id_;
  std::string last_event_name_;
};

// A fake ExtensionSystem that returns a FakeEventRouter for event_router().
class FakeExtensionSystem : public extensions::TestExtensionSystem {
 public:
  explicit FakeExtensionSystem(Profile* profile)
      : TestExtensionSystem(profile),
        prefs_(new extensions::TestExtensionPrefs(
            base::MessageLoopProxy::current())) {
    fake_event_router_.reset(new FakeEventRouter(profile, prefs_->prefs()));
  }

  extensions::EventRouter* event_router() override {
    return fake_event_router_.get();
  }

 private:
  scoped_ptr<extensions::TestExtensionPrefs> prefs_;
  scoped_ptr<FakeEventRouter> fake_event_router_;
};

// Factory function to register for the ExtensionSystem.
KeyedService* BuildFakeExtensionSystem(content::BrowserContext* profile) {
  return new FakeExtensionSystem(static_cast<Profile*>(profile));
}

TEST_F(EasyUnlockPrivateApiTest, AutoPairing) {
  FakeExtensionSystem* fake_extension_system =
      static_cast<FakeExtensionSystem*>(
          extensions::ExtensionSystemFactory::GetInstance()
              ->SetTestingFactoryAndUse(profile(), &BuildFakeExtensionSystem));
  FakeEventRouter* event_router =
      static_cast<FakeEventRouter*>(fake_extension_system->event_router());

  EasyUnlockServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), &BuildTestEasyUnlockService);

  AutoPairingResult result;

  // Dispatch OnStartAutoPairing event on EasyUnlockService::StartAutoPairing.
  EasyUnlockService* service = EasyUnlockService::Get(profile());
  service->StartAutoPairing(base::Bind(&AutoPairingResult::SetResult,
                                       base::Unretained(&result)));
  EXPECT_EQ(1, event_router->event_count());
  EXPECT_EQ(extension_misc::kEasyUnlockAppId,
            event_router->last_extension_id());
  EXPECT_EQ(
      extensions::api::easy_unlock_private::OnStartAutoPairing::kEventName,
      event_router->last_event_name());

  // Test SetAutoPairingResult call with failure.
  scoped_refptr<EasyUnlockPrivateSetAutoPairingResultFunction> function(
      new EasyUnlockPrivateSetAutoPairingResultFunction());
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(),
      "[{\"success\":false, \"errorMessage\":\"fake_error\"}]",
      browser(),
      extension_function_test_utils::NONE));
  EXPECT_FALSE(result.success);
  EXPECT_EQ("fake_error", result.error);

  // Test SetAutoPairingResult call with success.
  service->StartAutoPairing(base::Bind(&AutoPairingResult::SetResult,
                                       base::Unretained(&result)));
  function = new EasyUnlockPrivateSetAutoPairingResultFunction();
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(),
      "[{\"success\":true}]",
      browser(),
      extension_function_test_utils::NONE));
  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.error.empty());
}

}  // namespace
