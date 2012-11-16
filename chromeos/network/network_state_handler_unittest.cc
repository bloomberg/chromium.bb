// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state_handler.h"

#include <map>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

using chromeos::NetworkState;

class TestObserver : public chromeos::NetworkStateHandlerObserver {
 public:
  TestObserver()
      : manager_changed_count_(0),
        network_count_(0) {
  }

  virtual ~TestObserver() {
  }

  virtual void NetworkManagerChanged() {
    ++manager_changed_count_;
  }

  virtual void NetworkListChanged(
      const chromeos::NetworkStateHandler::NetworkStateList& networks) {
    network_count_ = networks.size();
  }

  virtual void ActiveNetworkChanged(const NetworkState* network) {
    active_network_ = network ? network->path() : "";
    active_network_state_ = network ? network->state() : "";
  }

  virtual void ActiveNetworkStateChanged(const NetworkState* network) {
    active_network_state_ = network ? network->state() : "";
  }

  virtual void NetworkServiceChanged(const NetworkState* network) {
    DCHECK(network);
    std::map<std::string, int>::iterator iter =
        property_changes_.find(network->path());
    if (iter == property_changes_.end())
      property_changes_[network->path()] = 1;
    else
      iter->second++;
  }

  size_t manager_changed_count() { return manager_changed_count_; }
  size_t network_count() { return network_count_; }
  std::string active_network() { return active_network_; }
  std::string active_network_state() { return active_network_state_; }

  int PropertyChangesForService(const std::string& service_path) {
    std::map<std::string, int>::iterator iter =
        property_changes_.find(service_path);
    if (iter == property_changes_.end())
      return 0;
    return iter->second;
  }

 private:
  size_t manager_changed_count_;
  size_t network_count_;
  std::string active_network_;
  std::string active_network_state_;
  std::map<std::string, int> property_changes_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

namespace chromeos {

class NetworkStateHandlerTest : public testing::Test {
 public:
  NetworkStateHandlerTest() {}
  virtual ~NetworkStateHandlerTest() {}

  virtual void SetUp() OVERRIDE {
    // Initialize DBusThreadManager with a stub implementation.
    DBusThreadManager::InitializeWithStub();
  }

  virtual void TearDown() OVERRIDE {
    network_state_handler_.reset();
    test_observer_.reset();
    DBusThreadManager::Shutdown();
  }

  void SetupNetworkStateHandler() {
    test_observer_.reset(new TestObserver);
    network_state_handler_.reset(new NetworkStateHandler);
    network_state_handler_->AddObserver(test_observer_.get());
    network_state_handler_->InitShillPropertyHandler();
  }

 protected:
  MessageLoopForUI message_loop_;
  scoped_ptr<NetworkStateHandler> network_state_handler_;
  scoped_ptr<TestObserver> test_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStateHandlerTest);
};

TEST_F(NetworkStateHandlerTest, NetworkStateHandlerStub) {
  // This relies on the stub dbus implementations for ShillManagerClient,
  SetupNetworkStateHandler();
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->manager_changed_count());
  // Ensure that the network list is the expected size.
  const size_t kNumShillManagerClientStubImplServices = 4;
  EXPECT_EQ(kNumShillManagerClientStubImplServices,
            test_observer_->network_count());
  // Ensure that the first stub network is the active network.
  const std::string kShillManagerClientStubActiveService = "stub_ethernet";
  EXPECT_EQ(kShillManagerClientStubActiveService,
            test_observer_->active_network());
  EXPECT_EQ(kShillManagerClientStubActiveService,
            network_state_handler_->ActiveNetwork()->path());
  EXPECT_EQ(kShillManagerClientStubActiveService,
            network_state_handler_->ConnectedNetworkByType(
                flimflam::kTypeEthernet)->path());
  EXPECT_EQ(flimflam::kStateOnline, test_observer_->active_network_state());
}

TEST_F(NetworkStateHandlerTest, NetworkStateHandlerTechnologyChanged) {
  // This relies on the stub dbus implementations for ShillManagerClient,
  SetupNetworkStateHandler();
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1u, test_observer_->manager_changed_count());
  // Enable a technology.
  EXPECT_FALSE(network_state_handler_->TechnologyEnabled(flimflam::kTypeWimax));
  network_state_handler_->SetTechnologyEnabled(flimflam::kTypeWimax, true);
  message_loop_.RunUntilIdle();
  // Ensure we get a manager changed callback when we change a property.
  EXPECT_EQ(2u, test_observer_->manager_changed_count());
  EXPECT_TRUE(network_state_handler_->TechnologyEnabled(flimflam::kTypeWimax));
}

TEST_F(NetworkStateHandlerTest, NetworkStateHandlerServicePropertyChanged) {
  // This relies on the stub dbus implementations for ShillManagerClient,
  SetupNetworkStateHandler();
  message_loop_.RunUntilIdle();
  // Set a service property.
  const std::string eth0 = "stub_ethernet";
  EXPECT_EQ("", network_state_handler_->GetNetworkState(eth0)->security());
  EXPECT_EQ(1, test_observer_->PropertyChangesForService(eth0));
  base::StringValue security_value("TestSecurity");
  DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
      dbus::ObjectPath(eth0),
      flimflam::kSecurityProperty, security_value,
      base::Bind(&base::DoNothing), base::Bind(&ErrorCallbackFunction));
  message_loop_.RunUntilIdle();
  EXPECT_EQ("TestSecurity",
            network_state_handler_->GetNetworkState(eth0)->security());
  EXPECT_EQ(2, test_observer_->PropertyChangesForService(eth0));
}

TEST_F(NetworkStateHandlerTest, NetworkStateHandlerActiveServiceChanged) {
  // This relies on the stub dbus implementations for ShillManagerClient,
  SetupNetworkStateHandler();
  message_loop_.RunUntilIdle();

  // Change the active network by inserting wifi1 at the front of the list.
  ShillManagerClient::TestInterface* manager_test =
      DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface();
  ASSERT_TRUE(manager_test);
  const std::string wifi1 = "stub_wifi1";
  manager_test->AddServiceAtIndex(wifi1, 0, true);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(wifi1, test_observer_->active_network());
  EXPECT_EQ(flimflam::kStateOnline, test_observer_->active_network_state());

  // Change the state of wifi1, ensure that triggers the active state changed
  // observer.
  ShillServiceClient::TestInterface* service_test =
      DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
  ASSERT_TRUE(service_test);
  base::StringValue state_value(flimflam::kStateConfiguration);
  service_test->SetServiceProperty(wifi1, flimflam::kStateProperty,
                                   state_value);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(wifi1, test_observer_->active_network());
  EXPECT_EQ(flimflam::kStateConfiguration,
            test_observer_->active_network_state());
}

}  // namespace chromeos
