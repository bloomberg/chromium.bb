// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "extensions/browser/api/networking_private/networking_private_api.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/common/value_builder.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace extensions {

namespace {

const char kUserHash[] = "test_user_hash";
const char kUserProfilePath[] = "/network_profile/user/shill";

const char kWifiDevicePath[] = "/device/stub_wifi_device";

const char kSharedWifiServicePath[] = "/service/shared_wifi";
const char kSharedWifiGuid[] = "shared_wifi_guid";
const char kSharedWifiName[] = "shared_wifi";

const char kPrivateWifiServicePath[] = "/service/private_wifi";
const char kPrivateWifiGuid[] = "private_wifi_guid";
const char kPrivateWifiName[] = "private_wifi";

}  // namespace

class NetworkingPrivateApiTest : public ApiUnitTest {
 public:
  NetworkingPrivateApiTest() {}
  ~NetworkingPrivateApiTest() override {}

  void SetUp() override {
    ApiUnitTest::SetUp();

    chromeos::DBusThreadManager::Initialize();
    chromeos::NetworkHandler::Initialize();

    chromeos::LoginState::Initialize();
    chromeos::LoginState::Get()->SetLoggedInStateAndPrimaryUser(
        chromeos::LoginState::LOGGED_IN_ACTIVE,
        chromeos::LoginState::LOGGED_IN_USER_KIOSK_APP, kUserHash);

    chromeos::DBusThreadManager* dbus_manager =
        chromeos::DBusThreadManager::Get();
    profile_test_ = dbus_manager->GetShillProfileClient()->GetTestInterface();
    service_test_ = dbus_manager->GetShillServiceClient()->GetTestInterface();
    device_test_ = dbus_manager->GetShillDeviceClient()->GetTestInterface();

    base::RunLoop().RunUntilIdle();

    device_test_->ClearDevices();
    service_test_->ClearServices();

    SetUpNetworks();
    SetUpNetworkPolicy();

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    chromeos::NetworkHandler::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
    chromeos::LoginState::Shutdown();

    ApiUnitTest::TearDown();
  }

  void SetUpNetworks() {
    profile_test_->AddProfile(kUserProfilePath, kUserHash);
    profile_test_->AddProfile(
        chromeos::ShillProfileClient::GetSharedProfilePath(), "");

    device_test_->AddDevice(kWifiDevicePath, shill::kTypeWifi, "wifi_device1");

    service_test_->AddService(kSharedWifiServicePath, kSharedWifiGuid,
                              kSharedWifiName, shill::kTypeWifi,
                              shill::kStateOnline, true /* visible */);
    service_test_->SetServiceProperty(kSharedWifiServicePath,
                                      shill::kDeviceProperty,
                                      base::Value(kWifiDevicePath));
    service_test_->SetServiceProperty(kSharedWifiServicePath,
                                      shill::kSecurityClassProperty,
                                      base::Value("psk"));
    service_test_->SetServiceProperty(kSharedWifiServicePath,
                                      shill::kPriorityProperty, base::Value(2));
    service_test_->SetServiceProperty(
        kSharedWifiServicePath, shill::kProfileProperty,
        base::Value(chromeos::ShillProfileClient::GetSharedProfilePath()));
    profile_test_->AddService(
        chromeos::ShillProfileClient::GetSharedProfilePath(),
        kSharedWifiServicePath);

    service_test_->AddService(kPrivateWifiServicePath, kPrivateWifiGuid,
                              kPrivateWifiName, shill::kTypeWifi,
                              shill::kStateOnline, true /* visible */);
    service_test_->SetServiceProperty(kPrivateWifiServicePath,
                                      shill::kDeviceProperty,
                                      base::Value(kWifiDevicePath));
    service_test_->SetServiceProperty(kPrivateWifiServicePath,
                                      shill::kSecurityClassProperty,
                                      base::Value("psk"));
    service_test_->SetServiceProperty(kPrivateWifiServicePath,
                                      shill::kPriorityProperty, base::Value(2));

    service_test_->SetServiceProperty(kPrivateWifiServicePath,
                                      shill::kProfileProperty,
                                      base::Value(kUserProfilePath));
    profile_test_->AddService(kUserProfilePath, kPrivateWifiServicePath);
  }

  void SetUpNetworkPolicy() {
    chromeos::ManagedNetworkConfigurationHandler* config_handler =
        chromeos::NetworkHandler::Get()
            ->managed_network_configuration_handler();

    config_handler->SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUserHash,
                              base::ListValue(), base::DictionaryValue());

    config_handler->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, "",
                              base::ListValue(), base::DictionaryValue());
  }

  void AddSharedNetworkToUserProfile() {
    service_test_->SetServiceProperty(kSharedWifiServicePath,
                                      shill::kProfileProperty,
                                      base::Value(kUserProfilePath));
    profile_test_->AddService(kUserProfilePath, kSharedWifiServicePath);

    base::RunLoop().RunUntilIdle();
  }

  int GetNetworkPriority(const chromeos::NetworkState* network) {
    base::DictionaryValue properties;
    network->GetStateProperties(&properties);
    int priority;
    if (!properties.GetInteger(shill::kPriorityProperty, &priority))
      return -1;
    return priority;
  }

  bool GetServiceProfile(const std::string& service_path,
                         std::string* profile_path) {
    base::DictionaryValue properties;
    return profile_test_->GetService(service_path, profile_path, &properties);
  }

 private:
  chromeos::ShillProfileClient::TestInterface* profile_test_;
  chromeos::ShillServiceClient::TestInterface* service_test_;
  chromeos::ShillDeviceClient::TestInterface* device_test_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateApiTest);
};

TEST_F(NetworkingPrivateApiTest, SetSharedNetworkProperties) {
  EXPECT_EQ(networking_private::kErrorAccessToSharedConfig,
            RunFunctionAndReturnError(
                new NetworkingPrivateSetPropertiesFunction(),
                base::StringPrintf(
                    "[\"%s\", {\"WiFi\": {\"Passphrase\": \"passphrase\"}}]",
                    kSharedWifiGuid)));
}

TEST_F(NetworkingPrivateApiTest, SetPrivateNetworkPropertiesWebUI) {
  scoped_refptr<NetworkingPrivateSetPropertiesFunction> set_properties =
      new NetworkingPrivateSetPropertiesFunction();
  set_properties->set_source_context_type(Feature::WEBUI_CONTEXT);

  RunFunction(
      set_properties.get(),
      base::StringPrintf("[\"%s\", {\"Priority\": 0}]", kSharedWifiGuid));
  EXPECT_EQ(ExtensionFunction::SUCCEEDED, *set_properties->response_type());

  const chromeos::NetworkState* network =
      chromeos::NetworkHandler::Get()
          ->network_state_handler()
          ->GetNetworkStateFromGuid(kSharedWifiGuid);
  ASSERT_TRUE(network);
  EXPECT_FALSE(network->IsPrivate());
  EXPECT_EQ(0, GetNetworkPriority(network));
}

TEST_F(NetworkingPrivateApiTest, SetPrivateNetworkProperties) {
  scoped_refptr<NetworkingPrivateSetPropertiesFunction> set_properties =
      new NetworkingPrivateSetPropertiesFunction();
  RunFunction(
      set_properties.get(),
      base::StringPrintf("[\"%s\", {\"Priority\": 0}]", kPrivateWifiGuid));
  EXPECT_EQ(ExtensionFunction::SUCCEEDED, *set_properties->response_type());

  const chromeos::NetworkState* network =
      chromeos::NetworkHandler::Get()
          ->network_state_handler()
          ->GetNetworkStateFromGuid(kPrivateWifiGuid);
  ASSERT_TRUE(network);
  EXPECT_TRUE(network->IsPrivate());
  EXPECT_EQ(0, GetNetworkPriority(network));
}

TEST_F(NetworkingPrivateApiTest, CreateSharedNetwork) {
  EXPECT_EQ(
      networking_private::kErrorAccessToSharedConfig,
      RunFunctionAndReturnError(new NetworkingPrivateCreateNetworkFunction(),
                                "[true, {"
                                "  \"Type\": \"WiFi\","
                                "  \"WiFi\": {"
                                "    \"SSID\": \"New network\","
                                "    \"Security\": \"None\""
                                "}}]"));
}

TEST_F(NetworkingPrivateApiTest, CreateSharedNetworkWebUI) {
  scoped_refptr<NetworkingPrivateCreateNetworkFunction> create_network =
      new NetworkingPrivateCreateNetworkFunction();
  create_network->set_source_context_type(Feature::WEBUI_CONTEXT);

  std::unique_ptr<base::Value> result =
      RunFunctionAndReturnValue(create_network.get(),
                                "[true, {"
                                "  \"Priority\": 1,"
                                "  \"Type\": \"WiFi\","
                                "  \"WiFi\": {"
                                "    \"SSID\": \"New network\","
                                "    \"Security\": \"None\""
                                "}}]");

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_string());

  std::string guid = result->GetString();
  const chromeos::NetworkState* network = chromeos::NetworkHandler::Get()
                                              ->network_state_handler()
                                              ->GetNetworkStateFromGuid(guid);
  ASSERT_TRUE(network);
  EXPECT_FALSE(network->IsPrivate());
  ASSERT_EQ(1, GetNetworkPriority(network));
}

TEST_F(NetworkingPrivateApiTest, CreatePrivateNetwork) {
  std::unique_ptr<base::Value> result =
      RunFunctionAndReturnValue(new NetworkingPrivateCreateNetworkFunction(),
                                "[false, {"
                                "  \"Priority\": 1,"
                                "  \"Type\": \"WiFi\","
                                "  \"WiFi\": {"
                                "    \"SSID\": \"New WiFi\","
                                "    \"Security\": \"WPA-PSK\""
                                "}}]");

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_string());

  // Test the created config can be changed now.
  std::string guid = result->GetString();
  const chromeos::NetworkState* network = chromeos::NetworkHandler::Get()
                                              ->network_state_handler()
                                              ->GetNetworkStateFromGuid(guid);
  ASSERT_TRUE(network);
  EXPECT_TRUE(network->IsPrivate());
  EXPECT_EQ(1, GetNetworkPriority(network));

  scoped_refptr<NetworkingPrivateSetPropertiesFunction> set_properties =
      new NetworkingPrivateSetPropertiesFunction();

  RunFunction(set_properties.get(),
              base::StringPrintf("[\"%s\", {\"Priority\": 2}]", guid.c_str()));
  EXPECT_EQ(ExtensionFunction::SUCCEEDED, *set_properties->response_type());

  EXPECT_EQ(2, GetNetworkPriority(network));
}

}  // namespace extensions
