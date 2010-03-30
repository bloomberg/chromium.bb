// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mock_login_library.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/network_selection_view.h"
#include "chrome/browser/chromeos/login/network_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

class NetworkScreenTest : public WizardInProcessBrowserTest {
 public:
  NetworkScreenTest(): WizardInProcessBrowserTest("network") {
  }

 protected:
  virtual void SetUpInProcessBrowserTestFixture() {
    InitStatusAreaMocks();

    mock_login_library_ = new MockLoginLibrary();
    test_api()->SetLoginLibrary(mock_login_library_);
    EXPECT_CALL(*mock_login_library_, EmitLoginPromptReady())
        .Times(1);

    // Minimal set of expectations needed on NetworkScreen initialization.
    // Status bar expectations are defined with RetiresOnSaturation() so
    // these mocks will be active once status bar is initialized.
    EXPECT_CALL(*mock_network_library_, ethernet_connected())
        .Times(1)
        .WillOnce((Return(false)));
    EXPECT_CALL(*mock_network_library_, ethernet_connecting())
        .Times(1)
        .WillOnce((Return(false)));
    EXPECT_CALL(*mock_network_library_, wifi_networks())
        .Times(1)
        .WillOnce((ReturnRef(wifi_networks_)));
    EXPECT_CALL(*mock_network_library_, cellular_networks())
        .Times(1)
        .WillOnce((ReturnRef(cellular_networks_)));
    EXPECT_CALL(*mock_network_library_, AddObserver(_))
        .Times(1);
    EXPECT_CALL(*mock_network_library_, RemoveObserver(_))
        .Times(1);

    SetStatusAreaMocksExpectations();
  }

  virtual void TearDownInProcessBrowserTestFixture() {
    CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    test_api()->SetLoginLibrary(NULL);
  }

  void EthernetExpectations(bool connected, bool connecting) {
    EXPECT_CALL(*mock_network_library_, ethernet_connected())
        .Times(connected ? 2 : 1)
        .WillRepeatedly((Return(connected)));
    EXPECT_CALL(*mock_network_library_, ethernet_connecting())
        .Times(connecting ? 2 : 1)
        .WillRepeatedly((Return(connecting)));
  }

  void WifiExpectations(bool connected, bool connecting) {
    EXPECT_CALL(*mock_network_library_, wifi_connected())
        .Times(1)
        .WillRepeatedly((Return(connected)));
    EXPECT_CALL(*mock_network_library_, wifi_connecting())
        .Times(1)
        .WillRepeatedly((Return(connecting)));
  }

  void WifiCellularNetworksExpectations() {
    EXPECT_CALL(*mock_network_library_, wifi_networks())
       .Times(1)
       .WillOnce((ReturnRef(wifi_networks_)));
    EXPECT_CALL(*mock_network_library_, cellular_networks())
       .Times(1)
       .WillOnce((ReturnRef(cellular_networks_)));
  }

  MockLoginLibrary* mock_login_library_;

  CellularNetworkVector cellular_networks_;
  WifiNetworkVector wifi_networks_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkScreenTest);
};

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, TestBasic) {
  ASSERT_TRUE(controller() != NULL);
  NetworkScreen* network_screen = controller()->GetNetworkScreen();
  ASSERT_TRUE(network_screen != NULL);
  ASSERT_EQ(network_screen, controller()->current_screen());

  NetworkSelectionView* network_view = network_screen->view();
  ASSERT_TRUE(network_view != NULL);
  ASSERT_EQ(1, network_screen->GetItemCount());
  EXPECT_EQ(l10n_util::GetString(IDS_STATUSBAR_NO_NETWORKS_MESSAGE),
            network_screen->GetItemAt(0));
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, TestOobeNetworksConnected) {
  NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  NetworkScreen* network_screen = controller()->GetNetworkScreen();
  ASSERT_TRUE(network_screen != NULL);
  NetworkSelectionView* network_view = network_screen->view();
  ASSERT_TRUE(network_view != NULL);

  EthernetExpectations(true, false);
  WifiCellularNetworksExpectations();
  network_screen->NetworkChanged(network_library);

  // When OOBE flow is active network selection should be explicit.
  ASSERT_EQ(network_screen, controller()->current_screen());
  ASSERT_EQ(2, network_screen->GetItemCount());
  EXPECT_EQ(l10n_util::GetString(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET),
            network_screen->GetItemAt(1));

  std::string ip_address;
  std::string wifi_ssid("WiFi network");
  WifiNetwork wifi;
  wifi.connected = true;
  wifi.ssid = wifi_ssid;
  wifi_networks_.push_back(wifi);

  EthernetExpectations(false, false);
  WifiExpectations(true, false);
  WifiCellularNetworksExpectations();
  EXPECT_CALL(*mock_network_library_, wifi_ssid())
      .Times(1)
      .WillOnce((ReturnRef(wifi_ssid)));
  network_screen->NetworkChanged(network_library);
  ASSERT_EQ(network_screen, controller()->current_screen());
  ASSERT_EQ(2, network_screen->GetItemCount());
  EXPECT_EQ(ASCIIToWide(wifi_ssid), network_screen->GetItemAt(1));

  EthernetExpectations(true, false);
  WifiExpectations(true, false);
  WifiCellularNetworksExpectations();
  EXPECT_CALL(*mock_network_library_, wifi_ssid())
      .Times(1)
      .WillOnce((ReturnRef(wifi_ssid)));
  network_screen->NetworkChanged(network_library);
  ASSERT_EQ(network_screen, controller()->current_screen());
  ASSERT_EQ(3, network_screen->GetItemCount());
  EXPECT_EQ(l10n_util::GetString(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET),
            network_screen->GetItemAt(1));
  EXPECT_EQ(ASCIIToWide(wifi_ssid), network_screen->GetItemAt(2));
}

}  // namespace chromeos
