// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/easy_unlock_app_manager.h"
#include "chrome/browser/signin/easy_unlock_service_factory.h"
#include "chrome/browser/signin/easy_unlock_service_regular.h"
#include "chrome/common/extensions/api/easy_unlock_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_easy_unlock_client.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/proximity_auth/switches.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/aura/env.h"

namespace {

namespace api = extensions::api::easy_unlock_private;

using extensions::EasyUnlockPrivateGenerateEcP256KeyPairFunction;
using extensions::EasyUnlockPrivatePerformECDHKeyAgreementFunction;
using extensions::EasyUnlockPrivateCreateSecureMessageFunction;
using extensions::EasyUnlockPrivateUnwrapSecureMessageFunction;
using extensions::EasyUnlockPrivateSetAutoPairingResultFunction;

class TestableGetRemoteDevicesFunction
    : public extensions::EasyUnlockPrivateGetRemoteDevicesFunction {
 public:
  TestableGetRemoteDevicesFunction() {}

  // EasyUnlockPrivateGetRemoteDevicesFunction:
  std::string GetUserPrivateKey() override { return "user private key"; }

  std::vector<cryptauth::ExternalDeviceInfo> GetUnlockKeys() override {
    cryptauth::ExternalDeviceInfo unlock_key1;
    unlock_key1.set_friendly_device_name("test phone 1");
    unlock_key1.set_public_key("public key 1");
    unlock_key1.set_bluetooth_address("AA:BB:CC:DD:EE:FF");
    unlock_key1.set_unlock_key(true);

    cryptauth::ExternalDeviceInfo unlock_key2;
    unlock_key2.set_friendly_device_name("test phone 2");
    unlock_key2.set_public_key("public key 2");
    unlock_key2.set_bluetooth_address("FF:EE:DD:CC:BB:AA");
    unlock_key2.set_unlock_key(true);

    std::vector<cryptauth::ExternalDeviceInfo> unlock_keys;
    unlock_keys.push_back(unlock_key1);
    unlock_keys.push_back(unlock_key2);
    return unlock_keys;
  }

 private:
  ~TestableGetRemoteDevicesFunction() override {}

  DISALLOW_COPY_AND_ASSIGN(TestableGetRemoteDevicesFunction);
};

class TestableGetPermitAccessFunction
    : public extensions::EasyUnlockPrivateGetPermitAccessFunction {
 public:
  TestableGetPermitAccessFunction() {}

  // EasyUnlockPrivateGetPermitAccessFunction:
  void GetKeyPairForExperiment(std::string* user_public_key,
                               std::string* user_private_key) override {
    *user_public_key = "user public key";
    *user_private_key = "user private key";
  }

 private:
  ~TestableGetPermitAccessFunction() override {}

  DISALLOW_COPY_AND_ASSIGN(TestableGetPermitAccessFunction);
};

// Converts a string to a base::Value value whose buffer contains the
// string data without the trailing '\0'.
std::unique_ptr<base::Value> StringToBinaryValue(const std::string& value) {
  return base::Value::CreateWithCopiedBuffer(value.data(), value.length());
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
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        proximity_auth::switches::kDisableBluetoothLowEnergyDiscovery);

    chromeos::DBusThreadManager::Initialize();
    if (aura::Env::GetInstance()->mode() == aura::Env::Mode::LOCAL) {
      bluez::BluezDBusManager::Initialize(
          chromeos::DBusThreadManager::Get()->GetSystemBus(),
          chromeos::DBusThreadManager::Get()->IsUsingFakes());
    }
    client_ = chromeos::DBusThreadManager::Get()->GetEasyUnlockClient();

    extensions::ExtensionApiUnittest::SetUp();
  }

  void TearDown() override {
    extensions::ExtensionApiUnittest::TearDown();

    if (aura::Env::GetInstance()->mode() == aura::Env::Mode::LOCAL)
      bluez::BluezDBusManager::Shutdown();
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
      return std::string();
    }

    if (result_list->GetSize() != 1u) {
      LOG(ERROR) << "Invalid number of results.";
      return std::string();
    }

    const auto& result_binary_value = result_list->GetList()[0];
    if (!result_binary_value.is_blob()) {
      LOG(ERROR) << "Result not a binary value.";
      return std::string();
    }

    return std::string(result_binary_value.GetBlob().data(),
                       result_binary_value.GetBlob().size());
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

  const base::Value& public_key = result_list->GetList()[0];
  const base::Value& private_key = result_list->GetList()[1];
  ASSERT_TRUE(public_key.is_blob());
  ASSERT_TRUE(private_key.is_blob());

  EXPECT_TRUE(chromeos::FakeEasyUnlockClient::IsEcP256KeyPair(
      std::string(private_key.GetBlob().data(), private_key.GetBlob().size()),
      std::string(public_key.GetBlob().data(), public_key.GetBlob().size())));
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

  std::unique_ptr<base::ListValue> args(new base::ListValue);
  args->Append(StringToBinaryValue(private_key_1));
  args->Append(StringToBinaryValue(public_key_2));

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), std::move(args), browser(),
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

  std::unique_ptr<base::ListValue> args(new base::ListValue);
  args->Append(StringToBinaryValue("PAYLOAD"));
  args->Append(StringToBinaryValue("KEY"));
  auto options = std::make_unique<base::DictionaryValue>();
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
  args->Append(std::move(options));

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), std::move(args), browser(),
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

  std::unique_ptr<base::ListValue> args(new base::ListValue);
  args->Append(StringToBinaryValue("PAYLOAD"));
  args->Append(StringToBinaryValue("KEY"));
  auto options = std::make_unique<base::DictionaryValue>();
  args->Append(std::move(options));

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), std::move(args), browser(),
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

  std::unique_ptr<base::ListValue> args(new base::ListValue);
  args->Append(StringToBinaryValue("PAYLOAD"));
  args->Append(StringToBinaryValue("KEY"));
  auto options = std::make_unique<base::DictionaryValue>();
  options->Set("associatedData",
               StringToBinaryValue("ASSOCIATED_DATA"));
  options->Set("verificationKeyId",
               StringToBinaryValue("VERIFICATION_KEY_ID"));
  options->SetString(
      "signType",
      api::ToString(api::SIGNATURE_TYPE_ECDSA_P256_SHA256));
  args->Append(std::move(options));

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), std::move(args), browser(),
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

  std::unique_ptr<base::ListValue> args(new base::ListValue);
  args->Append(StringToBinaryValue("MESSAGE"));
  args->Append(StringToBinaryValue("KEY"));
  auto options = std::make_unique<base::DictionaryValue>();
  options->Set("associatedData", StringToBinaryValue("ASSOCIATED_DATA"));
  options->SetString(
      "encryptType",
      api::ToString(api::ENCRYPTION_TYPE_AES_256_CBC));
  options->SetString(
      "signType",
      api::ToString(api::SIGNATURE_TYPE_HMAC_SHA256));
  args->Append(std::move(options));

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), std::move(args), browser(),
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

  std::unique_ptr<base::ListValue> args(new base::ListValue);
  args->Append(StringToBinaryValue("MESSAGE"));
  args->Append(StringToBinaryValue("KEY"));
  auto options = std::make_unique<base::DictionaryValue>();
  args->Append(std::move(options));

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), std::move(args), browser(),
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

  std::unique_ptr<base::ListValue> args(new base::ListValue);
  args->Append(StringToBinaryValue("MESSAGE"));
  args->Append(StringToBinaryValue("KEY"));
  auto options = std::make_unique<base::DictionaryValue>();
  options->Set("associatedData",
               StringToBinaryValue("ASSOCIATED_DATA"));
  options->SetString(
      "signType",
      api::ToString(api::SIGNATURE_TYPE_ECDSA_P256_SHA256));
  args->Append(std::move(options));

  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), std::move(args), browser(),
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
std::unique_ptr<KeyedService> BuildTestEasyUnlockService(
    content::BrowserContext* context) {
  std::unique_ptr<EasyUnlockServiceRegular> service(
      new EasyUnlockServiceRegular(Profile::FromBrowserContext(context)));
  service->Initialize(
      EasyUnlockAppManager::Create(extensions::ExtensionSystem::Get(context),
                                   -1 /* manifest id */, base::FilePath()));
  return std::move(service);
}

TEST_F(EasyUnlockPrivateApiTest, AutoPairing) {
  extensions::TestEventRouter* event_router =
      extensions::CreateAndUseTestEventRouter(profile());
  event_router->set_expected_extension_id(extension_misc::kEasyUnlockAppId);

  EasyUnlockServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), &BuildTestEasyUnlockService);

  AutoPairingResult result;

  // Dispatch OnStartAutoPairing event on EasyUnlockService::StartAutoPairing.
  EasyUnlockService* service = EasyUnlockService::Get(profile());
  service->StartAutoPairing(base::Bind(&AutoPairingResult::SetResult,
                                       base::Unretained(&result)));
  EXPECT_EQ(1,
            event_router->GetEventCount(extensions::api::easy_unlock_private::
                                            OnStartAutoPairing::kEventName));

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

// Checks that the chrome.easyUnlockPrivate.getRemoteDevices API returns the
// stored value if the kEnableBluetoothLowEnergyDiscovery switch is not set.
TEST_F(EasyUnlockPrivateApiTest, GetRemoteDevicesNonExperimental) {
  EasyUnlockServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), &BuildTestEasyUnlockService);

  scoped_refptr<TestableGetRemoteDevicesFunction> function(
      new TestableGetRemoteDevicesFunction());
  std::unique_ptr<base::Value> value(
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), "[]", profile()));
  ASSERT_TRUE(value.get());
  ASSERT_EQ(base::Value::Type::LIST, value->type());

  base::ListValue* list_value = static_cast<base::ListValue*>(value.get());
  EXPECT_EQ(0u, list_value->GetSize());
}

// Checks that the chrome.easyUnlockPrivate.getPermitAccess API returns the
// stored value if the kEnableBluetoothLowEnergyDiscovery switch is not set.
TEST_F(EasyUnlockPrivateApiTest, GetPermitAccessNonExperimental) {
  EasyUnlockServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), &BuildTestEasyUnlockService);

  scoped_refptr<TestableGetPermitAccessFunction> function(
      new TestableGetPermitAccessFunction());
  std::unique_ptr<base::Value> value(
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), "[]", profile()));
  EXPECT_FALSE(value);
}

}  // namespace
