// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_settings_service.h"
#include "chrome/browser/chromeos/net/proxy_config_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::Return;

namespace arc {

namespace {
constexpr char kONCPolicy[] =
    "{ \"NetworkConfigurations\": ["
    "    { \"GUID\": \"stub_ethernet_guid\","
    "      \"Type\": \"Ethernet\","
    "      \"Name\": \"My Ethernet\","
    "      \"Ethernet\": {"
    "        \"Authentication\": \"None\" },"
    "      \"ProxySettings\": {"
    "        \"PAC\": \"http://domain.com/x\","
    "        \"Type\": \"PAC\" }"
    "    }"
    "  ],"
    "  \"Type\": \"UnencryptedConfiguration\""
    "}";
constexpr char kONCPacUrl[] = "http://domain.com/x";
constexpr char kDefaultServicePath[] = "stub_ethernet";
constexpr char kWifiServicePath[] = "stub_wifi";
constexpr char kUserProfilePath[] = "user_profile";

// Returns an amount of |broadcasts| matched with |proxy_settings|.
int CountProxyBroadcasts(
    const std::vector<FakeIntentHelperInstance::Broadcast>& broadcasts,
    const base::DictionaryValue* proxy_settings) {
  size_t count = 0;
  for (const FakeIntentHelperInstance::Broadcast& broadcast : broadcasts) {
    if (broadcast.action == "org.chromium.arc.intent_helper.SET_PROXY") {
      EXPECT_TRUE(
          base::JSONReader::Read(broadcast.extras)->Equals(proxy_settings));
      count++;
    }
  }
  return count;
}

void RunUntilIdle() {
  DCHECK(base::MessageLoop::current());
  base::RunLoop loop;
  loop.RunUntilIdle();
}

}  // namespace

class ArcSettingsServiceTest : public InProcessBrowserTest {
 public:
  ArcSettingsServiceTest() = default;

  // InProcessBrowserTest:
  ~ArcSettingsServiceTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    fake_intent_helper_instance_.reset(new FakeIntentHelperInstance());

    ArcServiceManager::SetArcBridgeServiceForTesting(
        base::MakeUnique<FakeArcBridgeService>());
  }

  void SetUpOnMainThread() override {
    SetupNetworkEnvironment();
    RunUntilIdle();

    ArcBridgeService::Get()->intent_helper()->SetInstance(
        fake_intent_helper_instance_.get());
  }

  void TearDownOnMainThread() override {
    ArcBridgeService::Get()->intent_helper()->SetInstance(nullptr);
  }

  void UpdatePolicy(const policy::PolicyMap& policy) {
    provider_.UpdateChromePolicy(policy);
    RunUntilIdle();
  }

 protected:
  void DisconnectNetworkService(const std::string& service_path) {
    chromeos::ShillServiceClient::TestInterface* service_test =
        chromeos::DBusThreadManager::Get()
            ->GetShillServiceClient()
            ->GetTestInterface();
    base::StringValue value(shill::kStateIdle);
    service_test->SetServiceProperty(service_path, shill::kStateProperty,
                                     value);
  }

  void SetProxyConfigForNetworkService(
      const std::string& service_path,
      const base::DictionaryValue* proxy_config) {
    ProxyConfigDictionary proxy_config_dict(proxy_config);
    const chromeos::NetworkState* network = chromeos::NetworkHandler::Get()
                                                ->network_state_handler()
                                                ->GetNetworkState(service_path);
    ASSERT_TRUE(network);
    chromeos::proxy_config::SetProxyConfigForNetwork(proxy_config_dict,
                                                     *network);
  }

  std::unique_ptr<FakeIntentHelperInstance> fake_intent_helper_instance_;

 private:
  void SetupNetworkEnvironment() {
    chromeos::ShillProfileClient::TestInterface* profile_test =
        chromeos::DBusThreadManager::Get()
            ->GetShillProfileClient()
            ->GetTestInterface();
    chromeos::ShillServiceClient::TestInterface* service_test =
        chromeos::DBusThreadManager::Get()
            ->GetShillServiceClient()
            ->GetTestInterface();

    profile_test->AddProfile(kUserProfilePath, "user");

    service_test->ClearServices();

    service_test->AddService(kDefaultServicePath, "stub_ethernet_guid", "eth0",
                             shill::kTypeEthernet, shill::kStateOnline,
                             true /* add_to_visible */);
    service_test->SetServiceProperty(kDefaultServicePath,
                                     shill::kProfileProperty,
                                     base::StringValue(kUserProfilePath));

    service_test->AddService(kWifiServicePath, "stub_wifi_guid", "wifi0",
                             shill::kTypeWifi, shill::kStateOnline,
                             true /* add_to_visible */);
    service_test->SetServiceProperty(kWifiServicePath, shill::kProfileProperty,
                                     base::StringValue(kUserProfilePath));
  }

  policy::MockConfigurationPolicyProvider provider_;

  DISALLOW_COPY_AND_ASSIGN(ArcSettingsServiceTest);
};

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, ProxyModePolicyTest) {
  fake_intent_helper_instance_->clear_broadcasts();

  policy::PolicyMap policy;
  policy.Set(
      policy::key::kProxyMode, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::MakeUnique<base::StringValue>(ProxyPrefs::kAutoDetectProxyModeName),
      nullptr);
  UpdatePolicy(policy);

  std::unique_ptr<base::DictionaryValue> expected_proxy_config(
      base::MakeUnique<base::DictionaryValue>());
  expected_proxy_config->SetString("mode",
                                   ProxyPrefs::kAutoDetectProxyModeName);
  expected_proxy_config->SetString("pacUrl", "http://wpad/wpad.dat");
  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 expected_proxy_config.get()),
            1);
}

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, ONCProxyPolicyTest) {
  fake_intent_helper_instance_->clear_broadcasts();

  policy::PolicyMap policy;
  policy.Set(policy::key::kOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::MakeUnique<base::StringValue>(kONCPolicy), nullptr);
  UpdatePolicy(policy);

  std::unique_ptr<base::DictionaryValue> expected_proxy_config(
      base::MakeUnique<base::DictionaryValue>());
  expected_proxy_config->SetString("mode", ProxyPrefs::kPacScriptProxyModeName);
  expected_proxy_config->SetString("pacUrl", kONCPacUrl);

  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 expected_proxy_config.get()),
            1);
}

// Proxy policy has a higher priority than proxy default settings.
IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, TwoSourcesTest) {
  fake_intent_helper_instance_->clear_broadcasts();

  policy::PolicyMap policy;
  // Proxy policy.
  policy.Set(policy::key::kProxyMode, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::MakeUnique<base::StringValue>(
                 ProxyPrefs::kFixedServersProxyModeName),
             nullptr);
  policy.Set(policy::key::kProxyServer, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::MakeUnique<base::StringValue>("proxy:8888"), nullptr);
  UpdatePolicy(policy);

  std::unique_ptr<base::DictionaryValue> proxy_config(
      base::MakeUnique<base::DictionaryValue>());
  proxy_config->SetString("mode", ProxyPrefs::kAutoDetectProxyModeName);
  ProxyConfigDictionary proxy_config_dict(proxy_config.get());
  const chromeos::NetworkState* network = chromeos::NetworkHandler::Get()
                                              ->network_state_handler()
                                              ->DefaultNetwork();
  ASSERT_TRUE(network);
  chromeos::proxy_config::SetProxyConfigForNetwork(proxy_config_dict, *network);
  RunUntilIdle();

  std::unique_ptr<base::DictionaryValue> expected_proxy_config(
      base::MakeUnique<base::DictionaryValue>());
  expected_proxy_config->SetString("mode",
                                   ProxyPrefs::kFixedServersProxyModeName);
  expected_proxy_config->SetString("host", "proxy");
  expected_proxy_config->SetInteger("port", 8888);
  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 expected_proxy_config.get()),
            1);
}

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, ProxyPrefTest) {
  fake_intent_helper_instance_->clear_broadcasts();

  std::unique_ptr<base::DictionaryValue> proxy_config(
      base::MakeUnique<base::DictionaryValue>());
  proxy_config->SetString("mode", ProxyPrefs::kPacScriptProxyModeName);
  proxy_config->SetString("pac_url", "http://proxy");
  browser()->profile()->GetPrefs()->Set(proxy_config::prefs::kProxy,
                                        *proxy_config.get());
  RunUntilIdle();

  std::unique_ptr<base::DictionaryValue> expected_proxy_config(
      base::MakeUnique<base::DictionaryValue>());
  expected_proxy_config->SetString("mode", ProxyPrefs::kPacScriptProxyModeName);
  expected_proxy_config->SetString("pacUrl", "http://proxy");
  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 expected_proxy_config.get()),
            1);
}

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, DefaultNetworkProxyConfigTest) {
  fake_intent_helper_instance_->clear_broadcasts();

  std::unique_ptr<base::DictionaryValue> proxy_config(
      base::MakeUnique<base::DictionaryValue>());
  proxy_config->SetString("mode", ProxyPrefs::kFixedServersProxyModeName);
  proxy_config->SetString("server", "proxy:8080");
  SetProxyConfigForNetworkService(kDefaultServicePath, proxy_config.get());
  RunUntilIdle();

  std::unique_ptr<base::DictionaryValue> expected_proxy_config(
      base::MakeUnique<base::DictionaryValue>());
  expected_proxy_config->SetString("mode",
                                   ProxyPrefs::kFixedServersProxyModeName);
  expected_proxy_config->SetString("host", "proxy");
  expected_proxy_config->SetInteger("port", 8080);
  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 expected_proxy_config.get()),
            1);
}

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, DefaultNetworkDisconnectedTest) {
  fake_intent_helper_instance_->clear_broadcasts();
  // Set proxy confog for default network.
  std::unique_ptr<base::DictionaryValue> default_proxy_config(
      base::MakeUnique<base::DictionaryValue>());
  default_proxy_config->SetString("mode",
                                  ProxyPrefs::kFixedServersProxyModeName);
  default_proxy_config->SetString("server", "default/proxy:8080");
  SetProxyConfigForNetworkService(kDefaultServicePath,
                                  default_proxy_config.get());
  RunUntilIdle();

  // Set proxy confog for WI-FI network.
  std::unique_ptr<base::DictionaryValue> wifi_proxy_config(
      base::MakeUnique<base::DictionaryValue>());
  wifi_proxy_config->SetString("mode", ProxyPrefs::kFixedServersProxyModeName);
  wifi_proxy_config->SetString("server", "wifi/proxy:8080");
  SetProxyConfigForNetworkService(kWifiServicePath, wifi_proxy_config.get());
  RunUntilIdle();

  // Observe default network proxy config broadcast.
  std::unique_ptr<base::DictionaryValue> expected_default_proxy_config(
      base::MakeUnique<base::DictionaryValue>());
  expected_default_proxy_config->SetString(
      "mode", ProxyPrefs::kFixedServersProxyModeName);
  expected_default_proxy_config->SetString("host", "default/proxy");
  expected_default_proxy_config->SetInteger("port", 8080);
  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 expected_default_proxy_config.get()),
            1);

  // Disconnect default network.
  fake_intent_helper_instance_->clear_broadcasts();
  DisconnectNetworkService(kDefaultServicePath);
  RunUntilIdle();

  // Observe WI-FI network proxy config broadcast.
  std::unique_ptr<base::DictionaryValue> expected_wifi_proxy_config(
      base::MakeUnique<base::DictionaryValue>());
  expected_wifi_proxy_config->SetString("mode",
                                        ProxyPrefs::kFixedServersProxyModeName);
  expected_wifi_proxy_config->SetString("host", "wifi/proxy");
  expected_wifi_proxy_config->SetInteger("port", 8080);

  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 expected_wifi_proxy_config.get()),
            1);
}

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, NoNetworkConnectedTest) {
  // Disconnect all networks.
  fake_intent_helper_instance_->clear_broadcasts();
  DisconnectNetworkService(kDefaultServicePath);
  DisconnectNetworkService(kWifiServicePath);
  RunUntilIdle();

  EXPECT_EQ(
      CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(), nullptr),
      0);
}

}  // namespace arc
