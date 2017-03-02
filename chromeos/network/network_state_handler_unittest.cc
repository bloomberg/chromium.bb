// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state_handler.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

const char kShillManagerClientStubWifiDevice[] = "/device/stub_wifi_device1";
const char kShillManagerClientStubCellularDevice[] =
    "/device/stub_cellular_device1";
const char kShillManagerClientStubDefaultService[] = "/service/eth1";
const char kShillManagerClientStubDefaultWifi[] = "/service/wifi1";
const char kShillManagerClientStubWifi2[] = "/service/wifi2";
const char kShillManagerClientStubCellular[] = "/service/cellular1";

const char kTetherGuid1[] = "tether1";
const char kTetherGuid2[] = "tether2";
const char kTetherName1[] = "Device1";
const char kTetherName2[] = "Device2";

using chromeos::DeviceState;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;

class TestObserver : public chromeos::NetworkStateHandlerObserver {
 public:
  explicit TestObserver(NetworkStateHandler* handler)
      : handler_(handler),
        device_list_changed_count_(0),
        device_count_(0),
        network_list_changed_count_(0),
        network_count_(0),
        default_network_change_count_(0) {
  }

  ~TestObserver() override {}

  void DeviceListChanged() override {
    NetworkStateHandler::DeviceStateList devices;
    handler_->GetDeviceList(&devices);
    device_count_ = devices.size();
    ++device_list_changed_count_;
  }

  void NetworkListChanged() override {
    NetworkStateHandler::NetworkStateList networks;
    handler_->GetNetworkListByType(chromeos::NetworkTypePattern::Default(),
                                   false /* configured_only */,
                                   false /* visible_only */,
                                   0 /* no limit */,
                                   &networks);
    network_count_ = networks.size();
    if (network_count_ == 0) {
      default_network_ = "";
      default_network_connection_state_ = "";
    }
    ++network_list_changed_count_;
  }

  void DefaultNetworkChanged(const NetworkState* network) override {
    EXPECT_TRUE(!network || network->IsConnectedState());
    ++default_network_change_count_;
    default_network_ = network ? network->path() : "";
    default_network_connection_state_ =
        network ? network->connection_state() : "";
    DVLOG(1) << "DefaultNetworkChanged: " << default_network_
             << " State: " << default_network_connection_state_;
  }

  void NetworkConnectionStateChanged(const NetworkState* network) override {
    network_connection_state_[network->path()] = network->connection_state();
    connection_state_changes_[network->path()]++;
  }

  void NetworkPropertiesUpdated(const NetworkState* network) override {
    DCHECK(network);
    property_updates_[network->path()]++;
  }

  void DevicePropertiesUpdated(const DeviceState* device) override {
    DCHECK(device);
    device_property_updates_[device->path()]++;
  }

  size_t device_list_changed_count() { return device_list_changed_count_; }
  size_t device_count() { return device_count_; }
  size_t network_list_changed_count() { return network_list_changed_count_; }
  size_t network_count() { return network_count_; }
  size_t default_network_change_count() {
    return default_network_change_count_;
  }
  void reset_change_counts() {
    DVLOG(1) << "=== RESET CHANGE COUNTS ===";
    default_network_change_count_ = 0;
    device_list_changed_count_ = 0;
    network_list_changed_count_ = 0;
    connection_state_changes_.clear();
  }
  void reset_updates() {
    property_updates_.clear();
    device_property_updates_.clear();
  }
  std::string default_network() { return default_network_; }
  std::string default_network_connection_state() {
    return default_network_connection_state_;
  }

  int PropertyUpdatesForService(const std::string& service_path) {
    return property_updates_[service_path];
  }

  int PropertyUpdatesForDevice(const std::string& device_path) {
    return device_property_updates_[device_path];
  }

  int ConnectionStateChangesForService(const std::string& service_path) {
    return connection_state_changes_[service_path];
  }

  std::string NetworkConnectionStateForService(
      const std::string& service_path) {
    return network_connection_state_[service_path];
  }

 private:
  NetworkStateHandler* handler_;
  size_t device_list_changed_count_;
  size_t device_count_;
  size_t network_list_changed_count_;
  size_t network_count_;
  size_t default_network_change_count_;
  std::string default_network_;
  std::string default_network_connection_state_;
  std::map<std::string, int> property_updates_;
  std::map<std::string, int> device_property_updates_;
  std::map<std::string, int> connection_state_changes_;
  std::map<std::string, std::string> network_connection_state_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

namespace chromeos {

class NetworkStateHandlerTest : public testing::Test {
 public:
  NetworkStateHandlerTest()
      : device_test_(nullptr),
        manager_test_(nullptr),
        profile_test_(nullptr),
        service_test_(nullptr) {}
  ~NetworkStateHandlerTest() override {}

  void SetUp() override {
    // Initialize DBusThreadManager with a stub implementation.
    DBusThreadManager::Initialize();
    SetupDefaultShillState();
    network_state_handler_.reset(new NetworkStateHandler);
    test_observer_.reset(new TestObserver(network_state_handler_.get()));
    network_state_handler_->AddObserver(test_observer_.get(), FROM_HERE);
    network_state_handler_->InitShillPropertyHandler();
    base::RunLoop().RunUntilIdle();
    test_observer_->reset_change_counts();
  }

  void TearDown() override {
    network_state_handler_->RemoveObserver(test_observer_.get(), FROM_HERE);
    network_state_handler_->Shutdown();
    test_observer_.reset();
    network_state_handler_.reset();
    DBusThreadManager::Shutdown();
  }

 protected:
  void AddService(const std::string& service_path,
                  const std::string& guid,
                  const std::string& name,
                  const std::string& type,
                  const std::string& state) {
    service_test_->AddService(service_path, guid, name, type, state,
                              true /* add_to_visible */);
  }

  void SetupDefaultShillState() {
    base::RunLoop().RunUntilIdle();  // Process any pending updates
    device_test_ =
        DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
    ASSERT_TRUE(device_test_);
    device_test_->ClearDevices();
    device_test_->AddDevice(kShillManagerClientStubWifiDevice,
                            shill::kTypeWifi, "stub_wifi_device1");
    device_test_->AddDevice(kShillManagerClientStubCellularDevice,
                            shill::kTypeCellular, "stub_cellular_device1");

    manager_test_ =
        DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface();
    ASSERT_TRUE(manager_test_);

    profile_test_ =
        DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
    ASSERT_TRUE(profile_test_);
    profile_test_->ClearProfiles();

    service_test_ =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    ASSERT_TRUE(service_test_);
    service_test_->ClearServices();
    AddService(kShillManagerClientStubDefaultService,
               "eth1_guid",
               "eth1",
               shill::kTypeEthernet,
               shill::kStateOnline);
    AddService(kShillManagerClientStubDefaultWifi,
               "wifi1_guid",
               "wifi1",
               shill::kTypeWifi,
               shill::kStateOnline);
    AddService(kShillManagerClientStubWifi2,
               "wifi2_guid",
               "wifi2",
               shill::kTypeWifi,
               shill::kStateIdle);
    AddService(kShillManagerClientStubCellular,
               "cellular1_guid",
               "cellular1",
               shill::kTypeCellular,
               shill::kStateIdle);
  }

  void UpdateManagerProperties() { base::RunLoop().RunUntilIdle(); }

  void SetServiceProperty(const std::string& service_path,
                          const std::string& key,
                          const base::Value& value) {
    DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
        dbus::ObjectPath(service_path), key, value,
        base::Bind(&base::DoNothing), base::Bind(&ErrorCallbackFunction));
  }

  base::MessageLoopForUI message_loop_;
  std::unique_ptr<NetworkStateHandler> network_state_handler_;
  std::unique_ptr<TestObserver> test_observer_;
  ShillDeviceClient::TestInterface* device_test_;
  ShillManagerClient::TestInterface* manager_test_;
  ShillProfileClient::TestInterface* profile_test_;
  ShillServiceClient::TestInterface* service_test_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStateHandlerTest);
};

TEST_F(NetworkStateHandlerTest, NetworkStateHandlerStub) {
  // Ensure that the device and network list are the expected size.
  const size_t kNumShillManagerClientStubImplDevices = 2;
  EXPECT_EQ(kNumShillManagerClientStubImplDevices,
            test_observer_->device_count());
  const size_t kNumShillManagerClientStubImplServices = 4;
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            test_observer_->network_count());
  // Ensure that the first stub network is the default network.
  EXPECT_EQ(kShillManagerClientStubDefaultService,
            test_observer_->default_network());
  ASSERT_TRUE(network_state_handler_->DefaultNetwork());
  EXPECT_EQ(kShillManagerClientStubDefaultService,
            network_state_handler_->DefaultNetwork()->path());
  EXPECT_EQ(kShillManagerClientStubDefaultService,
            network_state_handler_->ConnectedNetworkByType(
                NetworkTypePattern::Ethernet())->path());
  EXPECT_EQ(kShillManagerClientStubDefaultWifi,
            network_state_handler_->ConnectedNetworkByType(
                NetworkTypePattern::WiFi())->path());
  EXPECT_EQ(kShillManagerClientStubCellular,
            network_state_handler_->FirstNetworkByType(
                NetworkTypePattern::Mobile())->path());
  EXPECT_EQ(
      kShillManagerClientStubCellular,
      network_state_handler_->FirstNetworkByType(NetworkTypePattern::Cellular())
          ->path());
  EXPECT_EQ(shill::kStateOnline,
            test_observer_->default_network_connection_state());
}

TEST_F(NetworkStateHandlerTest, GetNetworkList) {
  // Ensure that the network list is the expected size.
  const size_t kNumShillManagerClientStubImplServices = 4;
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            test_observer_->network_count());
  // Add a non-visible network to the profile.
  const std::string profile = "/profile/profile1";
  const std::string wifi_favorite_path = "/service/wifi_faviorite";
  service_test_->AddService(wifi_favorite_path,
                            "wifi_faviorite_guid",
                            "wifi_faviorite",
                            shill::kTypeWifi,
                            shill::kStateIdle,
                            false /* add_to_visible */);
  profile_test_->AddProfile(profile, "" /* userhash */);
  EXPECT_TRUE(profile_test_->AddService(profile, wifi_favorite_path));
  UpdateManagerProperties();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kNumShillManagerClientStubImplServices + 1,
            test_observer_->network_count());

  // Get all networks.
  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(NetworkTypePattern::Default(),
                                               false /* configured_only */,
                                               false /* visible_only */,
                                               0 /* no limit */,
                                               &networks);
  EXPECT_EQ(kNumShillManagerClientStubImplServices + 1, networks.size());
  // Limit number of results.
  network_state_handler_->GetNetworkListByType(NetworkTypePattern::Default(),
                                               false /* configured_only */,
                                               false /* visible_only */,
                                               2 /* limit */,
                                               &networks);
  EXPECT_EQ(2u, networks.size());
  // Get all wifi networks.
  network_state_handler_->GetNetworkListByType(NetworkTypePattern::WiFi(),
                                               false /* configured_only */,
                                               false /* visible_only */,
                                               0 /* no limit */,
                                               &networks);
  EXPECT_EQ(3u, networks.size());
  // Get visible networks.
  network_state_handler_->GetNetworkListByType(NetworkTypePattern::Default(),
                                               false /* configured_only */,
                                               true /* visible_only */,
                                               0 /* no limit */,
                                               &networks);
  EXPECT_EQ(kNumShillManagerClientStubImplServices, networks.size());
  network_state_handler_->GetVisibleNetworkList(&networks);
  EXPECT_EQ(kNumShillManagerClientStubImplServices, networks.size());
  // Get configured (profile) networks.
  network_state_handler_->GetNetworkListByType(NetworkTypePattern::Default(),
                                               true /* configured_only */,
                                               false /* visible_only */,
                                               0 /* no limit */,
                                               &networks);
  EXPECT_EQ(1u, networks.size());
}

TEST_F(NetworkStateHandlerTest, GetTetherNetworkList) {
  NetworkStateHandler::NetworkStateList tether_networks;

  network_state_handler_->GetTetherNetworkList(0 /* no limit */,
                                               &tether_networks);
  EXPECT_EQ(0u, tether_networks.size());

  network_state_handler_->AddTetherNetworkState(kTetherGuid1, kTetherName1);

  network_state_handler_->GetTetherNetworkList(0 /* no limit */,
                                               &tether_networks);
  EXPECT_EQ(1u, tether_networks.size());

  network_state_handler_->AddTetherNetworkState(kTetherGuid2, kTetherName2);

  network_state_handler_->GetTetherNetworkList(0 /* no limit */,
                                               &tether_networks);
  EXPECT_EQ(2u, tether_networks.size());

  network_state_handler_->GetTetherNetworkList(1 /* no limit */,
                                               &tether_networks);
  EXPECT_EQ(1u, tether_networks.size());
}

TEST_F(NetworkStateHandlerTest, NetworkListChanged) {
  size_t stub_network_count = test_observer_->network_count();
  // Set up two additional visible networks.
  const std::string wifi3 = "/service/wifi3";
  const std::string wifi4 = "/service/wifi4";
  service_test_->SetServiceProperties(
      wifi3, "wifi3_guid", "wifi3",
      shill::kTypeWifi, shill::kStateIdle, true /* visible */);
  service_test_->SetServiceProperties(
      wifi4, "wifi4_guid", "wifi4",
      shill::kTypeWifi, shill::kStateIdle, true /* visible */);
  // Add the services to the Manager. Only notify when the second service is
  // added.
  manager_test_->AddManagerService(wifi3, false);
  manager_test_->AddManagerService(wifi4, true);
  UpdateManagerProperties();
  // Expect two service updates and one list update.
  EXPECT_EQ(stub_network_count + 2, test_observer_->network_count());
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(wifi3));
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(wifi4));
  EXPECT_EQ(1u, test_observer_->network_list_changed_count());
}

TEST_F(NetworkStateHandlerTest, GetVisibleNetworks) {
  // Ensure that the network list is the expected size.
  const size_t kNumShillManagerClientStubImplServices = 4;
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            test_observer_->network_count());
  // Add a non-visible network to the profile.
  const std::string profile = "/profile/profile1";
  const std::string wifi_favorite_path = "/service/wifi_faviorite";
  service_test_->AddService(wifi_favorite_path,
                            "wifi_faviorite_guid",
                            "wifi_faviorite",
                            shill::kTypeWifi,
                            shill::kStateIdle,
                            false /* add_to_visible */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kNumShillManagerClientStubImplServices + 1,
            test_observer_->network_count());

  // Get visible networks.
  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetVisibleNetworkList(&networks);
  EXPECT_EQ(kNumShillManagerClientStubImplServices, networks.size());

  // Change the visible state of a network.
  SetServiceProperty(kShillManagerClientStubWifi2, shill::kVisibleProperty,
                     base::Value(false));
  base::RunLoop().RunUntilIdle();
  network_state_handler_->GetVisibleNetworkList(&networks);
  EXPECT_EQ(kNumShillManagerClientStubImplServices - 1, networks.size());
}

TEST_F(NetworkStateHandlerTest, TechnologyChanged) {
  // Disable a technology. Will immediately set the state to AVAILABLE and
  // notify observers.
  network_state_handler_->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false, network_handler::ErrorCallback());
  EXPECT_EQ(1u, test_observer_->device_list_changed_count());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::WiFi()));

  // Run the message loop. An additional notification will be received when
  // Shill updates the enabled technologies. The state should remain AVAILABLE.
  test_observer_->reset_change_counts();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->device_list_changed_count());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::WiFi()));

  // Enable a technology. Will immediately set the state to ENABLING and
  // notify observers.
  test_observer_->reset_change_counts();
  network_state_handler_->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), true, network_handler::ErrorCallback());
  EXPECT_EQ(1u, test_observer_->device_list_changed_count());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLING,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::WiFi()));

  // Run the message loop. State should change to ENABLED.
  test_observer_->reset_change_counts();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->device_list_changed_count());
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::WiFi()));
}

TEST_F(NetworkStateHandlerTest, TechnologyState) {
  manager_test_->RemoveTechnology(shill::kTypeWimax);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_UNAVAILABLE,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::Wimax()));

  manager_test_->AddTechnology(shill::kTypeWimax, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_AVAILABLE,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::Wimax()));

  manager_test_->SetTechnologyInitializing(shill::kTypeWimax, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_UNINITIALIZED,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::Wimax()));

  manager_test_->SetTechnologyInitializing(shill::kTypeWimax, false);
  network_state_handler_->SetTechnologyEnabled(
      NetworkTypePattern::Wimax(), true, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::Wimax()));

  manager_test_->RemoveTechnology(shill::kTypeWimax);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_UNAVAILABLE,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::Wimax()));
}

TEST_F(NetworkStateHandlerTest, TetherTechnologyState) {
  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_UNAVAILABLE,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::Tether()));

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kEnableTether);

  EXPECT_EQ(
      NetworkStateHandler::TECHNOLOGY_ENABLED,
      network_state_handler_->GetTechnologyState(NetworkTypePattern::Tether()));
}

TEST_F(NetworkStateHandlerTest, ServicePropertyChanged) {
  // Set a service property.
  const std::string eth1 = kShillManagerClientStubDefaultService;
  const NetworkState* ethernet = network_state_handler_->GetNetworkState(eth1);
  ASSERT_TRUE(ethernet);
  EXPECT_EQ("", ethernet->security_class());
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(eth1));
  base::StringValue security_class_value("TestSecurityClass");
  SetServiceProperty(eth1, shill::kSecurityClassProperty, security_class_value);
  base::RunLoop().RunUntilIdle();
  ethernet = network_state_handler_->GetNetworkState(eth1);
  EXPECT_EQ("TestSecurityClass", ethernet->security_class());
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(eth1));

  // Changing a service to the existing value should not trigger an update.
  SetServiceProperty(eth1, shill::kSecurityClassProperty, security_class_value);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(eth1));
}

TEST_F(NetworkStateHandlerTest, GetState) {
  const std::string profile = "/profile/profile1";
  const std::string wifi_path = kShillManagerClientStubDefaultWifi;

  // Add a wifi service to a Profile.
  profile_test_->AddProfile(profile, "" /* userhash */);
  EXPECT_TRUE(profile_test_->AddService(profile, wifi_path));
  UpdateManagerProperties();

  // Ensure that a NetworkState exists.
  const NetworkState* wifi_network =
      network_state_handler_->GetNetworkStateFromServicePath(
          wifi_path, true /* configured_only */);
  ASSERT_TRUE(wifi_network);

  // Test looking up by GUID.
  ASSERT_FALSE(wifi_network->guid().empty());
  const NetworkState* wifi_network_guid =
      network_state_handler_->GetNetworkStateFromGuid(wifi_network->guid());
  EXPECT_EQ(wifi_network, wifi_network_guid);

  // Remove the service, verify that there is no longer a NetworkState for it.
  service_test_->RemoveService(wifi_path);
  UpdateManagerProperties();
  EXPECT_FALSE(network_state_handler_->GetNetworkState(wifi_path));
}

TEST_F(NetworkStateHandlerTest, TetherNetworkState) {
  EXPECT_EQ(0u, test_observer_->network_list_changed_count());

  network_state_handler_->AddTetherNetworkState(kTetherGuid1, kTetherName1);

  EXPECT_EQ(1u, test_observer_->network_list_changed_count());

  const NetworkState* tether_network =
      network_state_handler_->GetNetworkStateFromGuid(kTetherGuid1);
  ASSERT_TRUE(tether_network);
  EXPECT_EQ(kTetherName1, tether_network->name());
  EXPECT_EQ(kTetherGuid1, tether_network->path());

  network_state_handler_->RemoveTetherNetworkState(kTetherGuid1);

  EXPECT_EQ(2u, test_observer_->network_list_changed_count());

  ASSERT_FALSE(network_state_handler_->GetNetworkStateFromGuid(kTetherGuid1));
}

TEST_F(NetworkStateHandlerTest, NetworkConnectionStateChanged) {
  const std::string eth1 = kShillManagerClientStubDefaultService;
  EXPECT_EQ(0, test_observer_->ConnectionStateChangesForService(eth1));

  // Change a network state.
  base::StringValue connection_state_idle_value(shill::kStateIdle);
  service_test_->SetServiceProperty(eth1, shill::kStateProperty,
                                   connection_state_idle_value);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(shill::kStateIdle,
            test_observer_->NetworkConnectionStateForService(eth1));
  EXPECT_EQ(1, test_observer_->ConnectionStateChangesForService(eth1));
  // Confirm that changing the connection state to the same value does *not*
  // signal the observer.
  service_test_->SetServiceProperty(eth1, shill::kStateProperty,
                                   connection_state_idle_value);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_observer_->ConnectionStateChangesForService(eth1));
}

TEST_F(NetworkStateHandlerTest, DefaultServiceDisconnected) {
  const std::string eth1 = kShillManagerClientStubDefaultService;
  const std::string wifi1 = kShillManagerClientStubDefaultWifi;

  EXPECT_EQ(0u, test_observer_->default_network_change_count());
  // Disconnect ethernet.
  base::StringValue connection_state_idle_value(shill::kStateIdle);
  service_test_->SetServiceProperty(eth1, shill::kStateProperty,
                                    connection_state_idle_value);
  base::RunLoop().RunUntilIdle();
  // Expect two changes: first when eth1 becomes disconnected, second when
  // wifi1 becomes the default.
  EXPECT_EQ(2u, test_observer_->default_network_change_count());
  EXPECT_EQ(wifi1, test_observer_->default_network());

  // Disconnect wifi.
  test_observer_->reset_change_counts();
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    connection_state_idle_value);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->default_network_change_count());
  EXPECT_EQ("", test_observer_->default_network());
}

TEST_F(NetworkStateHandlerTest, DefaultServiceConnected) {
  const std::string eth1 = kShillManagerClientStubDefaultService;
  const std::string wifi1 = kShillManagerClientStubDefaultWifi;

  // Disconnect ethernet and wifi.
  base::StringValue connection_state_idle_value(shill::kStateIdle);
  service_test_->SetServiceProperty(eth1, shill::kStateProperty,
                                    connection_state_idle_value);
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    connection_state_idle_value);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::string(), test_observer_->default_network());

  // Connect ethernet, should become the default network.
  test_observer_->reset_change_counts();
  base::StringValue connection_state_ready_value(shill::kStateReady);
  service_test_->SetServiceProperty(eth1, shill::kStateProperty,
                                    connection_state_ready_value);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(eth1, test_observer_->default_network());
  EXPECT_EQ(shill::kStateReady,
            test_observer_->default_network_connection_state());
  EXPECT_EQ(1u, test_observer_->default_network_change_count());
}

TEST_F(NetworkStateHandlerTest, DefaultServiceChanged) {
  const std::string eth1 = kShillManagerClientStubDefaultService;
  // The default service should be eth1.
  EXPECT_EQ(eth1, test_observer_->default_network());

  // Change the default network by changing Manager.DefaultService.
  // This should only generate one default network notification when the
  // DefaultService property changes.
  const std::string wifi1 = kShillManagerClientStubDefaultWifi;
  SetServiceProperty(eth1, shill::kStateProperty,
                     base::StringValue(shill::kStateIdle));
  manager_test_->SetManagerProperty(shill::kDefaultServiceProperty,
                                    base::StringValue(wifi1));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(wifi1, test_observer_->default_network());
  EXPECT_EQ(1u, test_observer_->default_network_change_count());

  // Change the state of the default network. This should generate a
  // default network notification.
  test_observer_->reset_change_counts();
  service_test_->SetServiceProperty(wifi1, shill::kStateProperty,
                                    base::StringValue(shill::kStateReady));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(shill::kStateReady,
            test_observer_->default_network_connection_state());
  EXPECT_EQ(1u, test_observer_->default_network_change_count());

  // Updating a property on the default network should also trigger
  // a default network change.
  test_observer_->reset_change_counts();
  SetServiceProperty(wifi1, shill::kSecurityClassProperty,
                     base::StringValue("TestSecurityClass"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->default_network_change_count());

  // No default network updates for signal strength changes.
  test_observer_->reset_change_counts();
  SetServiceProperty(wifi1, shill::kSignalStrengthProperty, base::Value(32));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, test_observer_->default_network_change_count());

  // Change the default network to a Connecting network, then set the
  // state to Connected. The DefaultNetworkChange notification should only
  // fire once when the network is connected.
  test_observer_->reset_change_counts();
  SetServiceProperty(wifi1, shill::kStateProperty,
                     base::StringValue(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->default_network_change_count());
  EXPECT_EQ(std::string(), test_observer_->default_network());

  const std::string wifi2 = kShillManagerClientStubWifi2;
  manager_test_->SetManagerProperty(shill::kDefaultServiceProperty,
                                    base::StringValue(wifi2));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->default_network_change_count());
  // Change the connection state of the default network, observer should fire.
  SetServiceProperty(wifi2, shill::kStateProperty,
                     base::StringValue(shill::kStateReady));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(wifi2, test_observer_->default_network());
  EXPECT_EQ(2u, test_observer_->default_network_change_count());
}

TEST_F(NetworkStateHandlerTest, RequestUpdate) {
  // Request an update for kShillManagerClientStubDefaultWifi.
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(
      kShillManagerClientStubDefaultWifi));
  network_state_handler_->RequestUpdateForNetwork(
      kShillManagerClientStubDefaultWifi);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, test_observer_->PropertyUpdatesForService(
      kShillManagerClientStubDefaultWifi));
}

TEST_F(NetworkStateHandlerTest, NetworkGuidInProfile) {
  const std::string profile = "/profile/profile1";
  const std::string wifi_path = "/service/wifi_with_guid";
  const std::string wifi_guid = "wifi_guid";
  const std::string wifi_name = "WifiWithGuid";
  const bool is_service_configured = true;

  // Add a network to the default Profile with a specified GUID.
  AddService(wifi_path, wifi_guid, wifi_name,
             shill::kTypeWifi, shill::kStateOnline);
  profile_test_->AddProfile(profile, "" /* userhash */);
  EXPECT_TRUE(profile_test_->AddService(profile, wifi_path));
  UpdateManagerProperties();

  // Verify that a NetworkState exists with a matching GUID.
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromServicePath(
          wifi_path, is_service_configured);
  ASSERT_TRUE(network);
  EXPECT_EQ(wifi_guid, network->guid());

  // Remove the service (simulating a network going out of range).
  service_test_->RemoveService(wifi_path);
  UpdateManagerProperties();
  EXPECT_FALSE(network_state_handler_->GetNetworkState(wifi_path));

  // Add the service (simulating a network coming back in range) and verify that
  // the NetworkState was created with the same GUID.
  AddService(wifi_path, "" /* guid */, wifi_name,
             shill::kTypeWifi, shill::kStateOnline);
  UpdateManagerProperties();
  network = network_state_handler_->GetNetworkStateFromServicePath(
      wifi_path, is_service_configured);
  ASSERT_TRUE(network);
  EXPECT_EQ(wifi_guid, network->guid());
}

TEST_F(NetworkStateHandlerTest, NetworkGuidNotInProfile) {
  const std::string wifi_path = "/service/wifi_with_guid";
  const std::string wifi_name = "WifiWithGuid";
  const bool is_service_configured = false;

  // Add a network without specifying a GUID or adding it to a profile.
  AddService(wifi_path, "" /* guid */, wifi_name,
             shill::kTypeWifi, shill::kStateOnline);
  UpdateManagerProperties();

  // Verify that a NetworkState exists with an assigned GUID.
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromServicePath(
          wifi_path, is_service_configured);
  ASSERT_TRUE(network);
  std::string wifi_guid = network->guid();
  EXPECT_FALSE(wifi_guid.empty());

  // Remove the service (simulating a network going out of range).
  service_test_->RemoveService(wifi_path);
  UpdateManagerProperties();
  EXPECT_FALSE(network_state_handler_->GetNetworkState(wifi_path));

  // Add the service (simulating a network coming back in range) and verify that
  // the NetworkState was created with the same GUID.
  AddService(wifi_path, "" /* guid */, wifi_name,
             shill::kTypeWifi, shill::kStateOnline);
  UpdateManagerProperties();
  network = network_state_handler_->GetNetworkStateFromServicePath(
      wifi_path, is_service_configured);
  ASSERT_TRUE(network);
  EXPECT_EQ(wifi_guid, network->guid());
}

TEST_F(NetworkStateHandlerTest, DeviceListChanged) {
  size_t stub_device_count = test_observer_->device_count();
  // Add an additional device.
  const std::string wifi_device = "/service/stub_wifi_device2";
  device_test_->AddDevice(wifi_device, shill::kTypeWifi, "stub_wifi_device2");
  UpdateManagerProperties();
  // Expect a device list update.
  EXPECT_EQ(stub_device_count + 1, test_observer_->device_count());
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForDevice(wifi_device));
  // Change a device property.
  device_test_->SetDeviceProperty(wifi_device, shill::kScanningProperty,
                                  base::Value(true));
  UpdateManagerProperties();
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForDevice(wifi_device));
}

TEST_F(NetworkStateHandlerTest, IPConfigChanged) {
  test_observer_->reset_updates();
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForDevice(
      kShillManagerClientStubWifiDevice));
  EXPECT_EQ(0, test_observer_->PropertyUpdatesForService(
      kShillManagerClientStubDefaultWifi));

  // Change IPConfigs property.
  ShillIPConfigClient::TestInterface* ip_config_test =
      DBusThreadManager::Get()->GetShillIPConfigClient()->GetTestInterface();
  const std::string kIPConfigPath = "test_ip_config";
  base::DictionaryValue ip_config_properties;
  ip_config_test->AddIPConfig(kIPConfigPath, ip_config_properties);
  base::ListValue device_ip_configs;
  device_ip_configs.AppendString(kIPConfigPath);
  device_test_->SetDeviceProperty(
      kShillManagerClientStubWifiDevice, shill::kIPConfigsProperty,
      device_ip_configs);
  service_test_->SetServiceProperty(
      kShillManagerClientStubDefaultWifi, shill::kIPConfigProperty,
      base::StringValue(kIPConfigPath));
  UpdateManagerProperties();
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForDevice(
      kShillManagerClientStubWifiDevice));
  EXPECT_EQ(1, test_observer_->PropertyUpdatesForService(
      kShillManagerClientStubDefaultWifi));
}

}  // namespace chromeos
