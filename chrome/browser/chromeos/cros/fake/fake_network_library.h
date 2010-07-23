// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_FAKE_NETWORK_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_FAKE_NETWORK_LIBRARY_H_

#include <string>

#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/network_library.h"

namespace chromeos {

class FakeNetworkLibrary : public NetworkLibrary {
 public:
  FakeNetworkLibrary() : ip_address_("1.1.1.1") {}
  virtual ~FakeNetworkLibrary() {}
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}

  virtual const EthernetNetwork& ethernet_network() const { return ethernet_; }
  virtual bool ethernet_connecting() const { return false; }
  virtual bool ethernet_connected() const { return true; }

  virtual const std::string& wifi_name() const { return EmptyString(); }
  virtual bool wifi_connecting() const { return false; }
  virtual bool wifi_connected() const { return false; }
  virtual int wifi_strength() const { return 0; }

  virtual const std::string& cellular_name() const { return EmptyString(); }
  virtual bool cellular_connecting() const { return false; }
  virtual bool cellular_connected() const { return false; }
  virtual int cellular_strength() const { return 0; }

  virtual bool Connected() const {
    return true;
  }
  virtual bool Connecting() const {
    return false;
  }
  virtual const std::string& IPAddress() const {
    return ip_address_;
  }

  virtual const WifiNetworkVector& wifi_networks() const {
    return wifi_networks_;
  }

  virtual const WifiNetworkVector& remembered_wifi_networks() const {
    return wifi_networks_;
  }

  virtual const CellularNetworkVector& cellular_networks() const {
    return cellular_networks_;
  }

  virtual const CellularNetworkVector& remembered_cellular_networks() const {
    return cellular_networks_;
  }

  virtual bool FindWifiNetworkByPath(const std::string& path,
                                     WifiNetwork* network) const {
    return false;
  }
  virtual bool FindCellularNetworkByPath(const std::string& path,
                                         CellularNetwork* network) const {
    return false;
  }

  virtual void RequestWifiScan() {}
  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result) {
    return true;
  }
  virtual bool ConnectToPreferredNetworkIfAvailable() {
    return false;
  }
  virtual bool PreferredNetworkConnected() {
    return false;
  }
  virtual bool PreferredNetworkFailed() {
    return false;
  }
  virtual void ConnectToWifiNetwork(WifiNetwork network,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath) {}
  virtual void ConnectToWifiNetwork(const std::string& ssid,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath,
                                    bool auto_connect) {}
  virtual void ConnectToCellularNetwork(CellularNetwork network) {}
  virtual void DisconnectFromWirelessNetwork(const WirelessNetwork& network) {}
  virtual void SaveCellularNetwork(const CellularNetwork& network) {}
  virtual void SaveWifiNetwork(const WifiNetwork& network) {}
  virtual void ForgetWirelessNetwork(const WirelessNetwork& network) {}

  virtual bool ethernet_available() const { return true; }
  virtual bool wifi_available() const { return false; }
  virtual bool cellular_available() const { return false; }

  virtual bool ethernet_enabled() const { return true; }
  virtual bool wifi_enabled() const { return false; }
  virtual bool cellular_enabled() const { return false; }

  virtual bool offline_mode() const { return false; }

  virtual void EnableEthernetNetworkDevice(bool enable) {}
  virtual void EnableWifiNetworkDevice(bool enable) {}
  virtual void EnableCellularNetworkDevice(bool enable) {}
  virtual void EnableOfflineMode(bool enable) {}
  virtual NetworkIPConfigVector GetIPConfigs(const std::string& device_path) {
    NetworkIPConfigVector ipconfig_vector;
    return ipconfig_vector;
  }
  virtual std::string GetHtmlInfo(int refresh) {
    return "";
  }

 private:
  std::string ip_address_;
  EthernetNetwork ethernet_;
  WifiNetworkVector wifi_networks_;
  CellularNetworkVector cellular_networks_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_FAKE_NETWORK_LIBRARY_H_
