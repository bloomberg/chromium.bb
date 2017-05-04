// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/network_connection_handler_tether_delegate.h"

#include <memory>

#include "chromeos/components/tether/tether_connector.h"
#include "chromeos/components/tether/tether_disconnector.h"
#include "chromeos/network/network_connection_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;

namespace chromeos {

namespace tether {

namespace {

class TestNetworkConnectionHandler : public NetworkConnectionHandler {
 public:
  TestNetworkConnectionHandler() : NetworkConnectionHandler() {}
  ~TestNetworkConnectionHandler() override {}

  void CallTetherConnect(const std::string& tether_network_guid,
                         const base::Closure& success_callback,
                         const network_handler::ErrorCallback& error_callback) {
    InitiateTetherNetworkConnection(tether_network_guid, success_callback,
                                    error_callback);
  }

  void CallTetherDisconnect(
      const std::string& tether_network_guid,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback) {
    InitiateTetherNetworkDisconnection(tether_network_guid, success_callback,
                                       error_callback);
  }

  // NetworkConnectionHandler:
  void ConnectToNetwork(const std::string& service_path,
                        const base::Closure& success_callback,
                        const network_handler::ErrorCallback& error_callback,
                        bool check_error_state) override {}

  void DisconnectNetwork(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback) override {}

  bool HasConnectingNetwork(const std::string& service_path) override {
    return false;
  }

  bool HasPendingConnectRequest() override { return false; }

  void Init(NetworkStateHandler* network_state_handler,
            NetworkConfigurationHandler* network_configuration_handler,
            ManagedNetworkConfigurationHandler*
                managed_network_configuration_handler) override {}
};

class MockTetherConnector : public TetherConnector {
 public:
  MockTetherConnector()
      : TetherConnector(nullptr /* network_state_handler */,
                        nullptr /* wifi_hotspot_connector */,
                        nullptr /* active_host */,
                        nullptr /* tether_host_fetcher */,
                        nullptr /* connection_manager */,
                        nullptr /* tether_host_response_recorder */,
                        nullptr /* device_id_tether_network_guid_map */) {}
  ~MockTetherConnector() override {}

  MOCK_METHOD3(
      ConnectToNetwork,
      void(const std::string& tether_network_guid,
           const base::Closure& success_callback,
           const network_handler::StringResultCallback& error_callback));
};

class MockTetherDisconnector : public TetherDisconnector {
 public:
  MockTetherDisconnector()
      : TetherDisconnector(nullptr /* network_connection_handler */,
                           nullptr /* network_state_handler */,
                           nullptr /* active_host */,
                           nullptr /* ble_connection_manager */,
                           nullptr /* network_configuration_remover */,
                           nullptr /* tether_connector */,
                           nullptr /* device_id_tether_network_guid_map */,
                           nullptr /* tether_host_fetcher */) {}
  ~MockTetherDisconnector() override {}

  MOCK_METHOD3(
      DisconnectFromNetwork,
      void(const std::string& tether_network_guid,
           const base::Closure& success_callback,
           const network_handler::StringResultCallback& error_callback));
};

}  // namespace

class NetworkConnectionHandlerTetherDelegateTest : public testing::Test {
 protected:
  NetworkConnectionHandlerTetherDelegateTest() {}

  void SetUp() override {
    test_network_connection_handler_ =
        base::WrapUnique(new TestNetworkConnectionHandler());
    mock_tether_connector_ =
        base::WrapUnique(new NiceMock<MockTetherConnector>());
    mock_tether_disconnector_ =
        base::WrapUnique(new NiceMock<MockTetherDisconnector>());

    delegate_ = base::MakeUnique<NetworkConnectionHandlerTetherDelegate>(
        test_network_connection_handler_.get(), mock_tether_connector_.get(),
        mock_tether_disconnector_.get());
  }

  std::unique_ptr<TestNetworkConnectionHandler>
      test_network_connection_handler_;
  std::unique_ptr<NiceMock<MockTetherConnector>> mock_tether_connector_;
  std::unique_ptr<NiceMock<MockTetherDisconnector>> mock_tether_disconnector_;

  std::unique_ptr<NetworkConnectionHandlerTetherDelegate> delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionHandlerTetherDelegateTest);
};

TEST_F(NetworkConnectionHandlerTetherDelegateTest, TestConnect) {
  EXPECT_CALL(*mock_tether_connector_, ConnectToNetwork(_, _, _));

  test_network_connection_handler_->CallTetherConnect(
      "tetherNetworkGuid", base::Closure(), network_handler::ErrorCallback());
}

TEST_F(NetworkConnectionHandlerTetherDelegateTest, TestDisconnect) {
  EXPECT_CALL(*mock_tether_disconnector_, DisconnectFromNetwork(_, _, _));

  test_network_connection_handler_->CallTetherDisconnect(
      "tetherNetworkGuid", base::Closure(), network_handler::ErrorCallback());
}

}  // namespace tether

}  // namespace cryptauth
