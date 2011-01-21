// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_NETWORK_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_NETWORK_LIBRARY_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/cros/network_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockNetworkLibrary : public NetworkLibrary {
 public:
  MockNetworkLibrary() {}
  virtual ~MockNetworkLibrary() {}
  MOCK_METHOD1(AddNetworkManagerObserver, void(NetworkManagerObserver*));
  MOCK_METHOD1(RemoveNetworkManagerObserver, void(NetworkManagerObserver*));
  MOCK_METHOD2(AddNetworkObserver, void(const std::string&, NetworkObserver*));
  MOCK_METHOD2(RemoveNetworkObserver, void(const std::string&,
                                           NetworkObserver*));
  MOCK_METHOD1(RemoveObserverForAllNetworks, void(NetworkObserver*));
  MOCK_METHOD1(AddCellularDataPlanObserver, void(CellularDataPlanObserver*));
  MOCK_METHOD1(RemoveCellularDataPlanObserver, void(CellularDataPlanObserver*));
  MOCK_METHOD0(Lock, void(void));
  MOCK_METHOD0(Unlock, void(void));
  MOCK_METHOD0(IsLocked, bool(void));
  MOCK_CONST_METHOD0(ethernet_network, const EthernetNetwork*(void));
  MOCK_CONST_METHOD0(ethernet_connecting, bool(void));
  MOCK_CONST_METHOD0(ethernet_connected, bool(void));

  MOCK_CONST_METHOD0(wifi_network, const WifiNetwork*(void));
  MOCK_CONST_METHOD0(wifi_connecting, bool(void));
  MOCK_CONST_METHOD0(wifi_connected, bool(void));

  MOCK_CONST_METHOD0(cellular_network, const CellularNetwork*(void));
  MOCK_CONST_METHOD0(cellular_connecting, bool(void));
  MOCK_CONST_METHOD0(cellular_connected, bool(void));

  MOCK_CONST_METHOD0(Connected, bool(void));
  MOCK_CONST_METHOD0(Connecting, bool(void));

  MOCK_CONST_METHOD0(IPAddress, const std::string&(void));
  MOCK_CONST_METHOD0(wifi_networks, const WifiNetworkVector&(void));
  MOCK_CONST_METHOD0(remembered_wifi_networks, const WifiNetworkVector&(void));
  MOCK_CONST_METHOD0(cellular_networks, const CellularNetworkVector&(void));

  MOCK_METHOD1(FindWifiNetworkByPath, WifiNetwork*(const std::string&));
  MOCK_METHOD1(FindCellularNetworkByPath, CellularNetwork*(const std::string&));

  MOCK_METHOD0(RequestWifiScan, void(void));
  MOCK_METHOD1(GetWifiAccessPoints, bool(WifiAccessPointVector*));
  MOCK_METHOD4(ConnectToWifiNetwork, bool(WifiNetwork*,
                                          const std::string&,
                                          const std::string&,
                                          const std::string&));
  MOCK_METHOD6(ConnectToWifiNetwork, bool(ConnectionSecurity security,
                                          const std::string&,
                                          const std::string&,
                                          const std::string&,
                                          const std::string&,
                                          bool));
  MOCK_METHOD1(ConnectToCellularNetwork, bool(const CellularNetwork*));
  MOCK_METHOD1(RefreshCellularDataPlans, void(const CellularNetwork* network));
  MOCK_METHOD0(SignalCellularPlanPayment, void(void));
  MOCK_METHOD0(HasRecentCellularPlanPayment, bool(void));

  MOCK_METHOD1(DisconnectFromWirelessNetwork, void(const WirelessNetwork*));
  MOCK_METHOD1(SaveCellularNetwork, void(const CellularNetwork*));
  MOCK_METHOD1(SaveWifiNetwork, void(const WifiNetwork*));
  MOCK_METHOD1(ForgetWifiNetwork, void(const std::string&));

  MOCK_CONST_METHOD0(ethernet_available, bool(void));
  MOCK_CONST_METHOD0(wifi_available, bool(void));
  MOCK_CONST_METHOD0(cellular_available, bool(void));

  MOCK_CONST_METHOD0(ethernet_enabled, bool(void));
  MOCK_CONST_METHOD0(wifi_enabled, bool(void));
  MOCK_CONST_METHOD0(cellular_enabled, bool(void));

  MOCK_CONST_METHOD0(wifi_scanning, bool(void));

  MOCK_CONST_METHOD0(active_network, const Network*(void));
  MOCK_CONST_METHOD0(offline_mode, bool(void));

  MOCK_METHOD1(EnableEthernetNetworkDevice, void(bool));
  MOCK_METHOD1(EnableWifiNetworkDevice, void(bool));
  MOCK_METHOD1(EnableCellularNetworkDevice, void(bool));
  MOCK_METHOD1(EnableOfflineMode, void(bool));
  MOCK_METHOD2(GetIPConfigs, NetworkIPConfigVector(const std::string&,
                                                   std::string*));
  MOCK_METHOD1(GetHtmlInfo, std::string(int));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_NETWORK_LIBRARY_H_
