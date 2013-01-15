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
  virtual void SetCarrier(const std::string& carrier,
                          const NetworkOperationCallback& completed) OVERRIDE;
  virtual bool IsCellularAlwaysInRoaming() OVERRIDE;
  virtual void RequestNetworkScan() OVERRIDE;

  virtual void RefreshIPConfig(Network* network) OVERRIDE;

  virtual void DisconnectFromNetwork(const Network* network) OVERRIDE;

  virtual void EnableOfflineMode(bool enable) OVERRIDE;

  virtual void GetIPConfigs(
      const std::string& device_path,
      HardwareAddressFormat format,
      const NetworkGetIPConfigsCallback& callback) OVERRIDE;
  virtual NetworkIPConfigVector GetIPConfigsAndBlock(
      const std::string& device_path,
      std::string* hardware_address,
      HardwareAddressFormat format) OVERRIDE;
  virtual void SetIPParameters(const std::string& service_path,
                               const std::string& address,
                               const std::string& netmask,
                               const std::string& gateway,
                               const std::string& name_servers,
                               int dhcp_usage_mask) OVERRIDE;
  virtual void RequestNetworkServiceProperties(
      const std::string& service_path,
      const NetworkServicePropertiesCallback& callback) OVERRIDE;

  // For testing only:
  // Returns the configurations applied in LoadOncNetworks. The key is the
  // network's service path which is mapped to the Shill dictionary.
  const std::map<std::string, base::DictionaryValue*>& GetConfigurations();

 private:
  void CompleteWifiInit();
  void CompleteCellularInit();
  void AddStubNetwork(Network* network, NetworkProfileType profile_type);
  void AddStubRememberedNetwork(Network* network);
  void ConnectToNetwork(Network* network);
  void ScanCompleted();
  void SendNetworkServiceProperties(
      const std::string& service_path,
      const NetworkServicePropertiesCallback& callback);

  std::string ip_address_;
  std::string hardware_address_;
  NetworkIPConfigVector ip_configs_;
  std::string pin_;
  bool pin_required_;
  bool pin_entered_;
  int network_priority_order_;
  WifiNetworkVector disabled_wifi_networks_;
  CellularNetworkVector disabled_cellular_networks_;
  WimaxNetworkVector disabled_wimax_networks_;
  std::map<std::string, base::DictionaryValue*> service_configurations_;
  base::WeakPtrFactory<NetworkLibraryImplStub> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryImplStub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_IMPL_STUB_H_
