// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_NETWORK_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_NETWORK_LIBRARY_H_

#include <string>

#include "chrome/browser/chromeos/cros/network_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockNetworkLibrary : public NetworkLibrary {
 public:
  MockNetworkLibrary() {}
  virtual ~MockNetworkLibrary() {}
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_CONST_METHOD0(ethernet_network, const EthernetNetwork&(void));
  MOCK_CONST_METHOD0(ethernet_connecting, bool(void));
  MOCK_CONST_METHOD0(ethernet_connected, bool(void));
  MOCK_CONST_METHOD0(wifi_ssid, const std::string&(void));
  MOCK_CONST_METHOD0(wifi_connecting, bool(void));
  MOCK_CONST_METHOD0(wifi_connected, bool(void));
  MOCK_CONST_METHOD0(wifi_strength, int(void));

  MOCK_CONST_METHOD0(cellular_name, const std::string&(void));
  MOCK_CONST_METHOD0(cellular_connecting, bool(void));
  MOCK_CONST_METHOD0(cellular_connected, bool(void));
  MOCK_CONST_METHOD0(cellular_strength, int(void));

  MOCK_CONST_METHOD0(Connected, bool(void));
  MOCK_CONST_METHOD0(Connecting, bool(void));

  MOCK_CONST_METHOD0(IPAddress, const std::string&(void));
  MOCK_CONST_METHOD0(wifi_networks, const WifiNetworkVector&(void));
  MOCK_CONST_METHOD0(cellular_networks, const CellularNetworkVector&(void));

  MOCK_METHOD2(ConnectToWifiNetwork, void(WifiNetwork,
                                          const string16&));
  MOCK_METHOD2(ConnectToWifiNetwork, void(const string16&,
                                          const string16&));
  MOCK_METHOD1(ConnectToCellularNetwork, void(CellularNetwork));

  MOCK_CONST_METHOD0(ethernet_available, bool(void));
  MOCK_CONST_METHOD0(wifi_available, bool(void));
  MOCK_CONST_METHOD0(cellular_available, bool(void));

  MOCK_CONST_METHOD0(ethernet_enabled, bool(void));
  MOCK_CONST_METHOD0(wifi_enabled, bool(void));
  MOCK_CONST_METHOD0(cellular_enabled, bool(void));

  MOCK_CONST_METHOD0(offline_mode, bool(void));

  MOCK_METHOD1(EnableEthernetNetworkDevice, void(bool));
  MOCK_METHOD1(EnableWifiNetworkDevice, void(bool));
  MOCK_METHOD1(EnableCellularNetworkDevice, void(bool));
  MOCK_METHOD1(EnableOfflineMode, void(bool));
  MOCK_METHOD1(GetIPConfigs, NetworkIPConfigVector(const std::string&));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_NETWORK_LIBRARY_H_
