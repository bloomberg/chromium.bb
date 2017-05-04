// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/wifi_hotspot_connector.h"

#include <memory>
#include <sstream>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test.h"
#include "chromeos/network/shill_property_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace tether {

namespace {

const char kSsid[] = "ssid";
const char kPassword[] = "password";

const char kOtherWifiServiceGuid[] = "otherWifiServiceGuid";

const char kTetherNetworkGuid[] = "tetherNetworkGuid";
const char kTetherNetworkGuid2[] = "tetherNetworkGuid2";

std::string CreateConfigurationJsonString(const std::string& guid) {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << guid << "\","
     << "  \"Type\": \"" << shill::kTypeWifi << "\","
     << "  \"State\": \"" << shill::kStateIdle << "\""
     << "}";
  return ss.str();
}

}  // namespace

class WifiHotspotConnectorTest : public NetworkStateTest {
 public:
  class TestNetworkConnect : public NetworkConnect {
   public:
    explicit TestNetworkConnect(NetworkStateTest* network_state_test)
        : network_state_test_(network_state_test) {}
    ~TestNetworkConnect() override {}

    base::DictionaryValue* last_configuration() {
      return last_configuration_.get();
    }

    std::string last_service_path_created() {
      return last_service_path_created_;
    }

    std::string network_id_to_connect() { return network_id_to_connect_; }

    // NetworkConnect:
    void DisconnectFromNetworkId(const std::string& network_id) override {}
    bool MaybeShowConfigureUI(const std::string& network_id,
                              const std::string& connect_error) override {
      return false;
    }
    void SetTechnologyEnabled(const chromeos::NetworkTypePattern& technology,
                              bool enabled_state) override {}
    void ShowMobileSetup(const std::string& network_id) override {}
    void ConfigureNetworkIdAndConnect(
        const std::string& network_id,
        const base::DictionaryValue& shill_properties,
        bool shared) override {}
    void CreateConfigurationAndConnect(base::DictionaryValue* shill_properties,
                                       bool shared) override {}

    void CreateConfiguration(base::DictionaryValue* shill_properties,
                             bool shared) override {
      EXPECT_FALSE(shared);
      last_configuration_ =
          base::MakeUnique<base::DictionaryValue>(*shill_properties);

      std::string wifi_guid;
      EXPECT_TRUE(
          last_configuration_->GetString(shill::kGuidProperty, &wifi_guid));

      last_service_path_created_ = network_state_test_->ConfigureService(
          CreateConfigurationJsonString(wifi_guid));
    }

    void ConnectToNetworkId(const std::string& network_id) override {
      network_id_to_connect_ = network_id;
    }

   private:
    NetworkStateTest* network_state_test_;
    std::unique_ptr<base::DictionaryValue> last_configuration_;
    std::string last_service_path_created_;
    std::string network_id_to_connect_;
  };

  WifiHotspotConnectorTest() {}
  ~WifiHotspotConnectorTest() override {}

  void SetUp() override {
    other_wifi_service_path_.clear();
    connection_callback_responses_.clear();

    DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();
    network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);

    SetUpShillState();

    test_network_connect_ = base::WrapUnique(new TestNetworkConnect(this));

    network_state_handler()->AddTetherNetworkState(
        kTetherNetworkGuid, "tetherNetworkName" /* name */,
        "tetherNetworkCarrier" /* carrier */, 100 /* full battery */,
        100 /* full signal strength */, false /* has_connected_to_host */);

    network_state_handler()->AddTetherNetworkState(
        kTetherNetworkGuid2, "tetherNetworkName2" /* name */,
        "tetherNetworkCarrier2" /* carrier */, 100 /* full battery */,
        100 /* full signal strength */, false /* has_connected_to_host */);

    wifi_hotspot_connector_ = base::WrapUnique(new WifiHotspotConnector(
        network_state_handler(), test_network_connect_.get()));

    mock_timer_ = new base::MockTimer(true /* retain_user_task */,
                                      false /* is_repeating */);
    wifi_hotspot_connector_->SetTimerForTest(base::WrapUnique(mock_timer_));
  }

  void SetUpShillState() {
    // Add a Wi-Fi service unrelated to the one under test. In some tests, a
    // connection from another device is tested.
    other_wifi_service_path_ = ConfigureService(
        CreateConfigurationJsonString(std::string(kOtherWifiServiceGuid)));
  }

  void TearDown() override {
    wifi_hotspot_connector_.reset();
    ShutdownNetworkState();
    NetworkStateTest::TearDown();
    DBusThreadManager::Shutdown();
  }

  void VerifyTimerSet() {
    EXPECT_TRUE(mock_timer_->IsRunning());
    EXPECT_EQ(base::TimeDelta::FromSeconds(
                  WifiHotspotConnector::kConnectionTimeoutSeconds),
              mock_timer_->GetCurrentDelay());
  }

  void VerifyTimerStopped() { EXPECT_FALSE(mock_timer_->IsRunning()); }

  void InvokeTimerTask() {
    VerifyTimerSet();
    mock_timer_->Fire();
  }

  void NotifyConnectable(const std::string& service_path) {
    SetServiceProperty(service_path, std::string(shill::kConnectableProperty),
                       base::Value(true));
  }

  void NotifyConnected(const std::string& service_path) {
    SetServiceProperty(service_path, std::string(shill::kStateProperty),
                       base::Value(shill::kStateReady));
  }

  // Verifies the last configuration has the expected SSID and password, and
  // returns the Wi-Fi GUID for the configuration.
  std::string VerifyLastConfiguration(const std::string& expected_ssid,
                                      const std::string& expected_password) {
    base::DictionaryValue* last_configuration =
        test_network_connect_->last_configuration();
    EXPECT_TRUE(last_configuration);

    std::string ssid = shill_property_util::GetSSIDFromProperties(
        *last_configuration, false, nullptr);
    EXPECT_EQ(expected_ssid, ssid);

    if (expected_password.empty()) {
      std::string security;
      EXPECT_TRUE(last_configuration->GetString(shill::kSecurityClassProperty,
                                                &security));
      EXPECT_EQ(std::string(shill::kSecurityNone), security);
    } else {
      std::string security;
      EXPECT_TRUE(last_configuration->GetString(shill::kSecurityClassProperty,
                                                &security));
      EXPECT_EQ(std::string(shill::kSecurityPsk), security);

      std::string password;
      EXPECT_TRUE(
          last_configuration->GetString(shill::kPassphraseProperty, &password));
      EXPECT_EQ(expected_password, password);
    }

    std::string wifi_guid;
    EXPECT_TRUE(
        last_configuration->GetString(shill::kGuidProperty, &wifi_guid));
    return wifi_guid;
  }

  void VerifyTetherAndWifiNetworkAssociation(const std::string& wifi_guid,
                                             const std::string& tether_guid) {
    const NetworkState* wifi_network_state =
        network_state_handler()->GetNetworkStateFromGuid(wifi_guid);
    ASSERT_TRUE(wifi_network_state);
    EXPECT_EQ(tether_guid, wifi_network_state->tether_guid());

    const NetworkState* tether_network_state =
        network_state_handler()->GetNetworkStateFromGuid(tether_guid);
    ASSERT_TRUE(tether_network_state);
    EXPECT_EQ(wifi_guid, tether_network_state->tether_guid());
  }

  void VerifyNetworkNotAssociated(const std::string& guid) {
    const NetworkState* network_state =
        network_state_handler()->GetNetworkStateFromGuid(guid);
    ASSERT_TRUE(network_state);
    EXPECT_TRUE(network_state->tether_guid().empty());
  }

  void WifiConnectionCallback(const std::string& wifi_guid) {
    connection_callback_responses_.push_back(wifi_guid);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::string other_wifi_service_path_;
  std::vector<std::string> connection_callback_responses_;

  base::MockTimer* mock_timer_;
  std::unique_ptr<TestNetworkConnect> test_network_connect_;

  std::unique_ptr<WifiHotspotConnector> wifi_hotspot_connector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WifiHotspotConnectorTest);
};

TEST_F(WifiHotspotConnectorTest, TestConnect_NetworkDoesNotBecomeConnectable) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid =
      VerifyLastConfiguration(std::string(kSsid), std::string(kPassword));
  EXPECT_FALSE(wifi_guid.empty());

  // Network does not become connectable.
  EXPECT_EQ("", test_network_connect_->network_id_to_connect());

  // Timeout timer fires.
  EXPECT_EQ(0u, connection_callback_responses_.size());
  InvokeTimerTask();
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_EQ("", connection_callback_responses_[0]);
}

TEST_F(WifiHotspotConnectorTest, TestConnect_AnotherNetworkBecomesConnectable) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid =
      VerifyLastConfiguration(std::string(kSsid), std::string(kPassword));
  EXPECT_FALSE(wifi_guid.empty());

  // Another network becomes connectable. This should not cause the connection
  // to start.
  NotifyConnectable(other_wifi_service_path_);
  VerifyNetworkNotAssociated(wifi_guid);
  std::string other_wifi_guid = network_state_handler()
                                    ->GetNetworkState(other_wifi_service_path_)
                                    ->guid();
  VerifyNetworkNotAssociated(other_wifi_guid);
  EXPECT_EQ("", test_network_connect_->network_id_to_connect());

  // Timeout timer fires.
  EXPECT_EQ(0u, connection_callback_responses_.size());
  InvokeTimerTask();
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_EQ("", connection_callback_responses_[0]);
}

TEST_F(WifiHotspotConnectorTest, TestConnect_CannotConnectToNetwork) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid =
      VerifyLastConfiguration(std::string(kSsid), std::string(kPassword));
  EXPECT_FALSE(wifi_guid.empty());

  // Network becomes connectable.
  NotifyConnectable(test_network_connect_->last_service_path_created());
  VerifyTetherAndWifiNetworkAssociation(wifi_guid, kTetherNetworkGuid);
  EXPECT_EQ(wifi_guid, test_network_connect_->network_id_to_connect());

  // Network connection does not occur.
  EXPECT_EQ(0u, connection_callback_responses_.size());

  // Timeout timer fires.
  InvokeTimerTask();
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_EQ("", connection_callback_responses_[0]);
}

TEST_F(WifiHotspotConnectorTest, TestConnect_Success) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), std::string(kPassword), kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid =
      VerifyLastConfiguration(std::string(kSsid), std::string(kPassword));
  EXPECT_FALSE(wifi_guid.empty());

  // Network becomes connectable.
  NotifyConnectable(test_network_connect_->last_service_path_created());
  VerifyTetherAndWifiNetworkAssociation(wifi_guid, kTetherNetworkGuid);
  EXPECT_EQ(wifi_guid, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(0u, connection_callback_responses_.size());

  // Connection to network successful.
  NotifyConnected(test_network_connect_->last_service_path_created());
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_EQ(wifi_guid, connection_callback_responses_[0]);
  VerifyTimerStopped();
}

TEST_F(WifiHotspotConnectorTest, TestConnect_Success_EmptyPassword) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      std::string(kSsid), "" /* password */, kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid = VerifyLastConfiguration(std::string(kSsid), "");
  EXPECT_FALSE(wifi_guid.empty());

  // Network becomes connectable.
  NotifyConnectable(test_network_connect_->last_service_path_created());
  VerifyTetherAndWifiNetworkAssociation(wifi_guid, kTetherNetworkGuid);
  EXPECT_EQ(wifi_guid, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(0u, connection_callback_responses_.size());

  // Connection to network successful.
  NotifyConnected(test_network_connect_->last_service_path_created());
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_EQ(wifi_guid, connection_callback_responses_[0]);
  VerifyTimerStopped();
}

TEST_F(WifiHotspotConnectorTest,
       TestConnect_SecondConnectionWhileWaitingForFirstToBecomeConnectable) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid1", "password1", "tetherNetworkGuid1",
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid1 = VerifyLastConfiguration("ssid1", "password1");
  EXPECT_FALSE(wifi_guid1.empty());
  std::string service_path1 =
      test_network_connect_->last_service_path_created();
  EXPECT_FALSE(service_path1.empty());

  // Before network becomes connectable, start the new connection.
  EXPECT_EQ(0u, connection_callback_responses_.size());
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid2", "password2", kTetherNetworkGuid2,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid2 = VerifyLastConfiguration("ssid2", "password2");
  EXPECT_FALSE(wifi_guid2.empty());
  std::string service_path2 =
      test_network_connect_->last_service_path_created();
  EXPECT_FALSE(service_path2.empty());
  VerifyNetworkNotAssociated(wifi_guid1);

  EXPECT_NE(service_path1, service_path2);

  // The original connection attempt should have gotten a "" response.
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_EQ("", connection_callback_responses_[0]);

  // First network becomes connectable.
  NotifyConnectable(service_path1);

  // A connection should not have started to that GUID.
  EXPECT_EQ("", test_network_connect_->network_id_to_connect());
  EXPECT_EQ(1u, connection_callback_responses_.size());

  // Second network becomes connectable.
  NotifyConnectable(service_path2);
  VerifyTetherAndWifiNetworkAssociation(wifi_guid2, kTetherNetworkGuid2);
  EXPECT_EQ(wifi_guid2, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(1u, connection_callback_responses_.size());

  // Connection to network successful.
  NotifyConnected(service_path2);
  EXPECT_EQ(2u, connection_callback_responses_.size());
  EXPECT_EQ(wifi_guid2, connection_callback_responses_[1]);
  VerifyTimerStopped();
}

TEST_F(WifiHotspotConnectorTest,
       TestConnect_SecondConnectionWhileWaitingForFirstToConnect) {
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid1", "password1", kTetherNetworkGuid,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  std::string wifi_guid1 = VerifyLastConfiguration("ssid1", "password1");
  EXPECT_FALSE(wifi_guid1.empty());
  std::string service_path1 =
      test_network_connect_->last_service_path_created();
  EXPECT_FALSE(service_path1.empty());

  // First network becomes connectable.
  NotifyConnectable(service_path1);
  VerifyTetherAndWifiNetworkAssociation(wifi_guid1, kTetherNetworkGuid);

  // After network becomes connectable, request a connection to second network.
  wifi_hotspot_connector_->ConnectToWifiHotspot(
      "ssid2", "password2", kTetherNetworkGuid2,
      base::Bind(&WifiHotspotConnectorTest::WifiConnectionCallback,
                 base::Unretained(this)));

  // The first Tether and Wi-Fi networks should no longer be associated.
  VerifyNetworkNotAssociated(kTetherNetworkGuid);
  VerifyNetworkNotAssociated(wifi_guid1);

  // The original connection attempt should have gotten a "" response.
  EXPECT_EQ(1u, connection_callback_responses_.size());
  EXPECT_EQ("", connection_callback_responses_[0]);

  std::string wifi_guid2 = VerifyLastConfiguration("ssid2", "password2");
  EXPECT_FALSE(wifi_guid2.empty());
  std::string service_path2 =
      test_network_connect_->last_service_path_created();
  EXPECT_FALSE(service_path2.empty());

  EXPECT_NE(service_path1, service_path2);

  // Second network becomes connectable.
  NotifyConnectable(service_path2);
  VerifyTetherAndWifiNetworkAssociation(wifi_guid2, kTetherNetworkGuid2);
  EXPECT_EQ(wifi_guid2, test_network_connect_->network_id_to_connect());
  EXPECT_EQ(1u, connection_callback_responses_.size());

  // Connection to network successful.
  NotifyConnected(service_path2);
  EXPECT_EQ(2u, connection_callback_responses_.size());
  EXPECT_EQ(wifi_guid2, connection_callback_responses_[1]);
  VerifyTimerStopped();
}

}  // namespace tether

}  // namespace chromeos
