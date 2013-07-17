// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::_;

CrosInProcessBrowserTest::CrosInProcessBrowserTest() :
    mock_network_library_(NULL) {
}

CrosInProcessBrowserTest::~CrosInProcessBrowserTest() {
}

void CrosInProcessBrowserTest::InitStatusAreaMocks() {
  if (mock_network_library_)
    return;
  mock_network_library_ = new StrictMock<MockNetworkLibrary>();
  NetworkLibrary::SetForTesting(mock_network_library_);
}

void CrosInProcessBrowserTest::SetStatusAreaMocksExpectations() {
  // We don't care how often these are called, just set their return values:
  EXPECT_CALL(*mock_network_library_, AddNetworkProfileObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, AddNetworkManagerObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, AddNetworkDeviceObserver(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, RemoveNetworkProfileObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, RemoveNetworkManagerObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, RemoveNetworkDeviceObserver(_, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, RemoveObserverForAllNetworks(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_network_library_, FindCellularDevice())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const NetworkDevice*)(NULL))));
  EXPECT_CALL(*mock_network_library_, FindEthernetDevice())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const NetworkDevice*)(NULL))));
  EXPECT_CALL(*mock_network_library_, ethernet_available())
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)));
  EXPECT_CALL(*mock_network_library_, wifi_available())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, wimax_available())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, cellular_available())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, ethernet_enabled())
      .Times(AnyNumber())
      .WillRepeatedly((Return(true)));
  EXPECT_CALL(*mock_network_library_, wifi_enabled())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, wimax_enabled())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, cellular_enabled())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, active_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const Network*)(NULL))));
  EXPECT_CALL(*mock_network_library_, active_nonvirtual_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const Network*)(NULL))));
  EXPECT_CALL(*mock_network_library_, ethernet_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const EthernetNetwork*)(NULL))));
  EXPECT_CALL(*mock_network_library_, wifi_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const WifiNetwork*)(NULL))));
  EXPECT_CALL(*mock_network_library_, wimax_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const WimaxNetwork*)(NULL))));
  EXPECT_CALL(*mock_network_library_, cellular_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const CellularNetwork*)(NULL))));
  EXPECT_CALL(*mock_network_library_, virtual_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const VirtualNetwork*)(NULL))));
  EXPECT_CALL(*mock_network_library_, wifi_networks())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(wifi_networks_)));
  EXPECT_CALL(*mock_network_library_, wimax_networks())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(wimax_networks_)));
  EXPECT_CALL(*mock_network_library_, cellular_networks())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(cellular_networks_)));
  EXPECT_CALL(*mock_network_library_, virtual_networks())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(virtual_networks_)));
  EXPECT_CALL(*mock_network_library_, connected_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const Network*)(NULL))));
  EXPECT_CALL(*mock_network_library_, connecting_network())
      .Times(AnyNumber())
      .WillRepeatedly((Return((const Network*)(NULL))));
  EXPECT_CALL(*mock_network_library_, virtual_network_connected())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, wifi_scanning())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, cellular_initializing())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
  EXPECT_CALL(*mock_network_library_, LoadOncNetworks(_, _))
        .Times(AnyNumber());

  // Set specific expectations for interesting functions:

  // NetworkMenuButton::OnNetworkChanged() calls:
  EXPECT_CALL(*mock_network_library_, Connected())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, Connecting())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, cellular_connected())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();

  // NetworkMenu::InitMenuItems() calls:
  EXPECT_CALL(*mock_network_library_, ethernet_connected())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_network_library_, ethernet_connecting())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)))
      .RetiresOnSaturation();
}

void CrosInProcessBrowserTest::SetUpInProcessBrowserTestFixture() {
  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();

  InitStatusAreaMocks();
  SetStatusAreaMocksExpectations();
}

void CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture() {
  // Prevent bogus gMock leak check from firing.
  NetworkLibrary::Shutdown();
}

}  // namespace chromeos
