// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_connect.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::AnyNumber;
using testing::NiceMock;
using testing::Return;

namespace chromeos {

namespace {

const char kWiFi1ServicePath[] = "/service/wifi1";
const char kWiFi1Guid[] = "wifi1_guid";

const char kCellular1DevicePath[] = "/device/stub_cellular_device1";
const char kCellular1ServicePath[] = "/service/cellular1";
const char kCellular1Guid[] = "cellular1_guid";

const char kTether1Guid[] = "tether1_guid";

class MockDelegate : public NetworkConnect::Delegate {
 public:
  MockDelegate() {}
  ~MockDelegate() override {}

  MOCK_METHOD1(ShowNetworkConfigure, void(const std::string& network_id));
  MOCK_METHOD1(ShowNetworkSettings, void(const std::string& network_id));
  MOCK_METHOD1(ShowEnrollNetwork, bool(const std::string& network_id));
  MOCK_METHOD0(ShowMobileSimDialog, void());
  MOCK_METHOD1(ShowMobileSetupDialog, void(const std::string& network_id));
  MOCK_METHOD2(ShowNetworkConnectError,
               void(const std::string& error_name,
                    const std::string& network_id));
  MOCK_METHOD1(ShowMobileActivationError, void(const std::string& network_id));
};

class MockTetherDelegate : public NetworkConnect::TetherDelegate {
 public:
  MockTetherDelegate() {}
  ~MockTetherDelegate() override {}

  MOCK_METHOD1(ConnectToNetwork, void(const std::string& guid));
};

}  // namespace

class NetworkConnectTest : public testing::Test {
 public:
  NetworkConnectTest() {}
  ~NetworkConnectTest() override {}

  void SetUp() override {
    testing::Test::SetUp();
    DBusThreadManager::Initialize();
    LoginState::Initialize();
    SetupDefaultShillState();
    NetworkHandler::Initialize();
    base::RunLoop().RunUntilIdle();

    mock_delegate_.reset(new MockDelegate());
    ON_CALL(*mock_delegate_, ShowEnrollNetwork(_)).WillByDefault(Return(true));

    mock_tether_delegate_.reset(new MockTetherDelegate());

    NetworkConnect::Initialize(mock_delegate_.get());
  }

  void TearDown() override {
    NetworkConnect::Shutdown();
    mock_delegate_.reset();
    LoginState::Shutdown();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
    testing::Test::TearDown();
  }

 protected:
  void SetupDefaultShillState() {
    base::RunLoop().RunUntilIdle();
    device_test_ =
        DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
    device_test_->ClearDevices();
    device_test_->AddDevice("/device/stub_wifi_device1", shill::kTypeWifi,
                            "stub_wifi_device1");
    device_test_->AddDevice(kCellular1DevicePath, shill::kTypeCellular,
                            "stub_cellular_device1");
    device_test_->SetDeviceProperty(
        kCellular1DevicePath, shill::kTechnologyFamilyProperty,
        base::StringValue(shill::kNetworkTechnologyGsm));

    service_test_ =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test_->ClearServices();
    const bool add_to_visible = true;

    // Create a wifi network and set to online.
    service_test_->AddService(kWiFi1ServicePath, kWiFi1Guid, "wifi1",
                              shill::kTypeWifi, shill::kStateIdle,
                              add_to_visible);
    service_test_->SetServiceProperty(kWiFi1ServicePath,
                                      shill::kSecurityClassProperty,
                                      base::StringValue(shill::kSecurityWep));
    service_test_->SetServiceProperty(
        kWiFi1ServicePath, shill::kConnectableProperty, base::Value(true));
    service_test_->SetServiceProperty(kWiFi1ServicePath,
                                      shill::kPassphraseProperty,
                                      base::StringValue("password"));

    // Create a cellular network.
    service_test_->AddService(kCellular1ServicePath, kCellular1Guid,
                              "cellular1", shill::kTypeCellular,
                              shill::kStateIdle, add_to_visible);
    service_test_->SetServiceProperty(
        kCellular1ServicePath, shill::kConnectableProperty, base::Value(true));
    service_test_->SetServiceProperty(
        kCellular1ServicePath, shill::kActivationStateProperty,
        base::StringValue(shill::kActivationStateActivated));
    service_test_->SetServiceProperty(kCellular1ServicePath,
                                      shill::kOutOfCreditsProperty,
                                      base::Value(false));

    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<MockDelegate> mock_delegate_;
  std::unique_ptr<MockTetherDelegate> mock_tether_delegate_;
  base::MessageLoop message_loop_;
  ShillDeviceClient::TestInterface* device_test_;
  ShillServiceClient::TestInterface* service_test_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkConnectTest);
};

TEST_F(NetworkConnectTest, ConnectToNetworkId_NoConfiguration) {
  EXPECT_CALL(*mock_delegate_, ShowNetworkConfigure(_)).Times(0);
  EXPECT_CALL(*mock_delegate_, ShowNetworkConnectError(_, _)).Times(0);

  NetworkConnect::Get()->ConnectToNetworkId("bad guid");
}

TEST_F(NetworkConnectTest, ConfigureAndConnectToNetwork_NoConfiguration) {
  EXPECT_CALL(*mock_delegate_,
              ShowNetworkConnectError(NetworkConnectionHandler::kErrorNotFound,
                                      "bad guid"));

  base::DictionaryValue properties;
  NetworkConnect::Get()->ConfigureNetworkIdAndConnect("bad guid", properties,
                                                      true);
}

TEST_F(NetworkConnectTest,
       ConfigureAndConnectToNetwork_NotSharedButNoProfilePath) {
  EXPECT_CALL(*mock_delegate_,
              ShowNetworkConnectError(
                  NetworkConnectionHandler::kErrorConfigureFailed, kWiFi1Guid));

  base::DictionaryValue properties;
  NetworkConnect::Get()->ConfigureNetworkIdAndConnect(kWiFi1Guid, properties,
                                                      false);
}

TEST_F(NetworkConnectTest, ShowConfigureUI) {
  EXPECT_CALL(*mock_delegate_, ShowNetworkConfigure(kWiFi1Guid)).Times(5);

  NetworkConnect::Get()->MaybeShowConfigureUI(
      kWiFi1Guid, NetworkConnectionHandler::kErrorBadPassphrase);
  NetworkConnect::Get()->MaybeShowConfigureUI(
      kWiFi1Guid, NetworkConnectionHandler::kErrorPassphraseRequired);
  NetworkConnect::Get()->MaybeShowConfigureUI(
      kWiFi1Guid, NetworkConnectionHandler::kErrorConfigurationRequired);
  NetworkConnect::Get()->MaybeShowConfigureUI(
      kWiFi1Guid, NetworkConnectionHandler::kErrorAuthenticationRequired);
  NetworkConnect::Get()->MaybeShowConfigureUI(
      kWiFi1Guid, NetworkConnectionHandler::kErrorConnectFailed);
}

TEST_F(NetworkConnectTest, ShowConfigureUI_BadErrorCode) {
  EXPECT_CALL(*mock_delegate_, ShowNetworkConfigure(kWiFi1Guid)).Times(0);

  NetworkConnect::Get()->MaybeShowConfigureUI(kWiFi1Guid,
                                              "incorrect error code");
}

TEST_F(NetworkConnectTest, ShowConfigureUI_CertRequired_ShowEnrollNetwork) {
  EXPECT_CALL(*mock_delegate_, ShowEnrollNetwork(_)).Times(AnyNumber());
  EXPECT_CALL(*mock_delegate_, ShowNetworkConfigure(kWiFi1Guid)).Times(0);

  NetworkConnect::Get()->MaybeShowConfigureUI(
      kWiFi1Guid, NetworkConnectionHandler::kErrorCertificateRequired);
}

TEST_F(NetworkConnectTest,
       ShowConfigureUI_CertRequired_DoNotShowEnrollNetwork) {
  EXPECT_CALL(*mock_delegate_, ShowEnrollNetwork(_)).Times(AnyNumber());
  EXPECT_CALL(*mock_delegate_, ShowNetworkConfigure(kWiFi1Guid));

  ON_CALL(*mock_delegate_, ShowEnrollNetwork(kWiFi1Guid))
      .WillByDefault(Return(false));

  NetworkConnect::Get()->MaybeShowConfigureUI(
      kWiFi1Guid, NetworkConnectionHandler::kErrorCertificateRequired);
}

// ShowNetworkSettings only applies to cellular networks.
TEST_F(NetworkConnectTest, ShowNetworkSettings) {
  EXPECT_CALL(*mock_delegate_, ShowNetworkSettings(kCellular1Guid));

  NetworkConnect::Get()->MaybeShowConfigureUI(
      kCellular1Guid, NetworkConnectionHandler::kErrorConnectFailed);
}

TEST_F(NetworkConnectTest, ShowNetworkSettings_CellOutOfCredits) {
  EXPECT_CALL(*mock_delegate_, ShowNetworkSettings(kCellular1Guid)).Times(0);
  EXPECT_CALL(*mock_delegate_, ShowMobileSetupDialog(kCellular1Guid));

  service_test_->SetServiceProperty(
      kCellular1ServicePath, shill::kOutOfCreditsProperty, base::Value(true));
  base::RunLoop().RunUntilIdle();

  NetworkConnect::Get()->MaybeShowConfigureUI(
      kCellular1Guid, NetworkConnectionHandler::kErrorConnectFailed);
}

TEST_F(NetworkConnectTest, ActivateCellular) {
  EXPECT_CALL(*mock_delegate_, ShowMobileSetupDialog(kCellular1Guid));

  service_test_->SetServiceProperty(
      kCellular1ServicePath, shill::kActivationStateProperty,
      base::StringValue(shill::kActivationStateNotActivated));
  base::RunLoop().RunUntilIdle();

  NetworkConnect::Get()->ConnectToNetworkId(kCellular1Guid);
}

TEST_F(NetworkConnectTest, ActivateCellular_Error) {
  EXPECT_CALL(*mock_delegate_, ShowMobileActivationError(kCellular1Guid));

  service_test_->SetServiceProperty(
      kCellular1ServicePath, shill::kActivationStateProperty,
      base::StringValue(shill::kActivationStateNotActivated));
  service_test_->SetServiceProperty(
      kCellular1ServicePath, shill::kActivationTypeProperty,
      base::StringValue(shill::kActivationTypeNonCellular));
  base::RunLoop().RunUntilIdle();

  NetworkConnect::Get()->ConnectToNetworkId(kCellular1Guid);
}

TEST_F(NetworkConnectTest, ShowMobileSimDialog) {
  EXPECT_CALL(*mock_delegate_, ShowMobileSimDialog());

  NetworkConnect::Get()->SetTechnologyEnabled(NetworkTypePattern::Cellular(),
                                              false);

  device_test_->SetDeviceProperty(
      kCellular1DevicePath, shill::kSIMPresentProperty, base::Value(true));
  device_test_->SetSimLocked(kCellular1DevicePath, true);

  base::RunLoop().RunUntilIdle();

  NetworkConnect::Get()->SetTechnologyEnabled(NetworkTypePattern::Cellular(),
                                              true);
}

TEST_F(NetworkConnectTest, ShowMobileSimDialog_SimAbsent) {
  EXPECT_CALL(*mock_delegate_, ShowMobileSimDialog()).Times(0);

  NetworkConnect::Get()->SetTechnologyEnabled(NetworkTypePattern::Cellular(),
                                              false);

  device_test_->SetDeviceProperty(
      kCellular1DevicePath, shill::kSIMPresentProperty, base::Value(false));
  device_test_->SetSimLocked(kCellular1DevicePath, true);

  base::RunLoop().RunUntilIdle();

  NetworkConnect::Get()->SetTechnologyEnabled(NetworkTypePattern::Cellular(),
                                              true);
}

TEST_F(NetworkConnectTest, ShowMobileSimDialog_SimUnlocked) {
  EXPECT_CALL(*mock_delegate_, ShowMobileSimDialog()).Times(0);

  NetworkConnect::Get()->SetTechnologyEnabled(NetworkTypePattern::Cellular(),
                                              false);

  device_test_->SetDeviceProperty(
      kCellular1DevicePath, shill::kSIMPresentProperty, base::Value(true));
  device_test_->SetSimLocked(kCellular1DevicePath, false);

  base::RunLoop().RunUntilIdle();

  NetworkConnect::Get()->SetTechnologyEnabled(NetworkTypePattern::Cellular(),
                                              true);
}

TEST_F(NetworkConnectTest, ConnectToTetherNetwork) {
  EXPECT_CALL(*mock_tether_delegate_, ConnectToNetwork(kTether1Guid));

  NetworkHandler::Get()->network_state_handler()->AddTetherNetworkState(
      kTether1Guid, "TetherNetwork");

  NetworkConnect::Get()->SetTetherDelegate(mock_tether_delegate_.get());
  NetworkConnect::Get()->ConnectToNetworkId(kTether1Guid);
}

TEST_F(NetworkConnectTest, ConnectToTetherNetwork_TetherNetworkDoesNotExist) {
  EXPECT_CALL(*mock_tether_delegate_, ConnectToNetwork(_)).Times(0);

  NetworkConnect::Get()->SetTetherDelegate(mock_tether_delegate_.get());
  NetworkConnect::Get()->ConnectToNetworkId(kTether1Guid);
}

TEST_F(NetworkConnectTest, ConnectToTetherNetwork_TetherDelegateNotSet) {
  EXPECT_CALL(*mock_tether_delegate_, ConnectToNetwork(_)).Times(0);

  NetworkHandler::Get()->network_state_handler()->AddTetherNetworkState(
      kTether1Guid, "TetherNetwork");

  NetworkConnect::Get()->ConnectToNetworkId(kTether1Guid);
}

}  // namespace chromeos
