// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_api.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/common/extensions/api/easy_unlock_private.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_easy_unlock_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

namespace api = extensions::api::easy_unlock_private;

using extensions::api::EasyUnlockPrivateGenerateEcP256KeyPairFunction;
using extensions::api::EasyUnlockPrivatePerformECDHKeyAgreementFunction;
using extensions::api::EasyUnlockPrivateCreateSecureMessageFunction;
using extensions::api::EasyUnlockPrivateUnwrapSecureMessageFunction;

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
  virtual ~EasyUnlockPrivateApiTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    chromeos::DBusThreadManager::Initialize();
    client_ = chromeos::DBusThreadManager::Get()->GetEasyUnlockClient();

    extensions::ExtensionApiUnittest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
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

}  // namespace

