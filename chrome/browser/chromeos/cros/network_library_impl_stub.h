// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_IMPL_STUB_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_IMPL_STUB_H_

#include "chrome/browser/chromeos/cros/network_library_impl_base.h"

namespace chromeos {

class NetworkLibraryImplStub : public NetworkLibraryImplBase {
 public:
  NetworkLibraryImplStub();
  virtual ~NetworkLibraryImplStub();

  virtual void Init() OVERRIDE;
  virtual bool IsCros() const OVERRIDE;

  // NetworkLibraryImplBase implementation.

  virtual void MonitorNetworkStart(const std::string& service_path) OVERRIDE;
  virtual void MonitorNetworkStop(const std::string& service_path) OVERRIDE;
  virtual void MonitorNetworkDeviceStart(
      const std::string& device_path) OVERRIDE;
  virtual void MonitorNetworkDeviceStop(
      const std::string& device_path) OVERRIDE;

  virtual void CallConfigureService(const std::string& identifier,
                                    const DictionaryValue* info) OVERRIDE;
  virtual void CallConnectToNetwork(Network* network) OVERRIDE;
  virtual void CallRequestWifiNetworkAndConnect(
      const std::string& ssid, ConnectionSecurity security) OVERRIDE;
  virtual void CallRequestVirtualNetworkAndConnect(
      const std::string& service_name,
      const std::string& server_hostname,
      ProviderType provider_type) OVERRIDE;

  virtual void CallDeleteRememberedNetwork(
      const std::string& profile_path,
      const std::string& service_path) OVERRIDE;

  virtual void CallEnableNetworkDeviceType(
      ConnectionType device, bool enable) OVERRIDE;

  virtual void CallRemoveNetwork(const Network* network) OVERRIDE;

  // NetworkLibrary implementation.

  virtual void SetCheckPortalList(
      const std::string& check_portal_list) OVERRIDE;
  virtual void SetDefaultCheckPortalList() OVERRIDE;
  virtual void ChangePin(const std::string& old_pin,
                         const std::string& new_pin) OVERRIDE;
  virtual void ChangeRequirePin(bool require_pin,
                                const std::string& pin) OVERRIDE;
  virtual void EnterPin(const std::string& pin) OVERRIDE;
  virtual void UnblockPin(const std::string& puk,
                          const std::string& new_pin) OVERRIDE;

  virtual void RequestCellularScan() OVERRIDE;
  virtual void RequestCellularRegister(
      const std::string& network_id) OVERRIDE;
  virtual void SetCellularDataRoamingAllowed(bool new_value) OVERRIDE;
  virtual bool IsCellularAlwaysInRoaming() OVERRIDE;
  virtual void RequestNetworkScan() OVERRIDE;

  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result) OVERRIDE;

  virtual void DisconnectFromNetwork(const Network* network) OVERRIDE;

  virtual void EnableOfflineMode(bool enable) OVERRIDE;

  virtual NetworkIPConfigVector GetIPConfigs(
      const std::string& device_path,
      std::string* hardware_address,
      HardwareAddressFormat format) OVERRIDE;
  virtual void SetIPConfig(const NetworkIPConfig& ipconfig) OVERRIDE;

 private:
  void AddStubNetwork(Network* network, NetworkProfileType profile_type);
  void AddStubRememberedNetwork(Network* network);
  void ConnectToNetwork(Network* network);

  std::string ip_address_;
  std::string hardware_address_;
  NetworkIPConfigVector ip_configs_;
  std::string pin_;
  bool pin_required_;
  bool pin_entered_;
  int64 connect_delay_ms_;
  int network_priority_order_;
  WifiNetworkVector disabled_wifi_networks_;
  CellularNetworkVector disabled_cellular_networks_;
  WimaxNetworkVector disabled_wimax_networks_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryImplStub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_IMPL_STUB_H_
