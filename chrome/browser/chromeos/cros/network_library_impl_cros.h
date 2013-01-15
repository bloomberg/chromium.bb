// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_IMPL_CROS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_IMPL_CROS_H_

#include "base/time.h"
#include "chrome/browser/chromeos/cros/network_library_impl_base.h"

namespace chromeos {

class CrosNetworkWatcher;

class NetworkLibraryImplCros : public NetworkLibraryImplBase  {
 public:
  NetworkLibraryImplCros();
  virtual ~NetworkLibraryImplCros();

  virtual void Init() OVERRIDE;
  virtual bool IsCros() const OVERRIDE;

  //////////////////////////////////////////////////////////////////////////////
  // NetworkLibraryImplBase implementation.

  virtual void MonitorNetworkStart(const std::string& service_path) OVERRIDE;
  virtual void MonitorNetworkStop(const std::string& service_path) OVERRIDE;
  virtual void MonitorNetworkDeviceStart(
      const std::string& device_path) OVERRIDE;
  virtual void MonitorNetworkDeviceStop(
      const std::string& device_path) OVERRIDE;

  virtual void CallConfigureService(const std::string& identifier,
                                    const base::DictionaryValue* info) OVERRIDE;
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

  //////////////////////////////////////////////////////////////////////////////
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
  virtual void RequestCellularRegister(const std::string& network_id) OVERRIDE;
  virtual void SetCellularDataRoamingAllowed(bool new_value) OVERRIDE;
  virtual void SetCarrier(const std::string& carrier,
                          const NetworkOperationCallback& completed) OVERRIDE;
  virtual bool IsCellularAlwaysInRoaming() OVERRIDE;
  virtual void RequestNetworkScan() OVERRIDE;

  virtual void RefreshIPConfig(Network* network) OVERRIDE;

  virtual void DisconnectFromNetwork(const Network* network) OVERRIDE;
  virtual void CallEnableNetworkDeviceType(
      ConnectionType device, bool enable) OVERRIDE;
  virtual void CallRemoveNetwork(const Network* network) OVERRIDE;

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

  //////////////////////////////////////////////////////////////////////////////
  // Callbacks.
  void UpdateNetworkStatus(
      const std::string& path, const std::string& key, const Value& value);

  void UpdateNetworkDeviceStatus(
      const std::string& path, const std::string& key, const Value& value);
  // Cellular specific updates. Returns false if update was ignored / reverted
  // and notification should be skipped.
  bool UpdateCellularDeviceStatus(NetworkDevice* device, PropertyIndex index);

  void GetIPConfigsCallback(const NetworkGetIPConfigsCallback& callback,
                            HardwareAddressFormat format,
                            const NetworkIPConfigVector& ipconfig_vector,
                            const std::string& hardware_address);

  void PinOperationCallback(const std::string& path,
                            NetworkMethodErrorType error,
                            const std::string& error_message);

  void CellularRegisterCallback(const std::string& path,
                                NetworkMethodErrorType error,
                                const std::string& error_message);

  void NetworkConnectCallback(const std::string& service_path,
                              NetworkMethodErrorType error,
                              const std::string& error_message);

  void WifiServiceUpdateAndConnect(const std::string& service_path,
                                   const base::DictionaryValue* properties);
  void VPNServiceUpdateAndConnect(const std::string& service_path,
                                  const base::DictionaryValue* properties);

  void NetworkManagerStatusChangedHandler(const std::string& path,
                                          const std::string& key,
                                          const base::Value& value);
  void NetworkManagerUpdate(const std::string& manager_path,
                            const base::DictionaryValue* properties);

  void NetworkServiceUpdate(const std::string& service_path,
                            const base::DictionaryValue* properties);
  void RememberedNetworkServiceUpdate(const std::string& profile_path,
                                      const std::string& service_path,
                                      const base::DictionaryValue* properties);
  void NetworkDeviceUpdate(const std::string& device_path,
                           const base::DictionaryValue* properties);

 private:
  // Structure used to pass IP parameter info to a DoSetIPParameters callback,
  // since Bind only takes up to six parameters.
  struct IPParameterInfo;

  // Second half of setting IP Parameters.  SetIPParameters above kicks off
  // an async information fetch, and this completes the operation when that
  // fetch is complete.
  void SetIPParametersCallback(const IPParameterInfo& info,
                               const std::string& service_path,
                               const base::DictionaryValue* properties);

  // Second half of refreshing IPConfig for a network.  Refreshes all IP config
  // paths found in properties.
  void RefreshIPConfigCallback(const std::string& device_path,
                               const base::DictionaryValue* properties);

  // This processes all Manager update messages.
  bool NetworkManagerStatusChanged(const std::string& key, const Value* value);
  void ParseNetworkManager(const base::DictionaryValue& dict);
  void UpdateTechnologies(const base::ListValue* technologies, int* bitfieldp);
  void UpdateAvailableTechnologies(const base::ListValue* technologies);
  void UpdateEnabledTechnologies(const base::ListValue* technologies);
  void UpdateConnectedTechnologies(const base::ListValue* technologies);

  // Update network lists.
  void UpdateNetworkServiceList(const base::ListValue* services);
  void UpdateWatchedNetworkServiceList(const base::ListValue* services);
  Network* ParseNetwork(const std::string& service_path,
                        const base::DictionaryValue& info);

  void UpdateRememberedNetworks(const base::ListValue* profiles);
  void RequestRememberedNetworksUpdate();
  void UpdateProfile(const std::string& profile_path,
                     const base::DictionaryValue* properties);
  Network* ParseRememberedNetwork(const std::string& profile_path,
                                  const std::string& service_path,
                                  const base::DictionaryValue& info);

  // NetworkDevice list management functions.
  void UpdateNetworkDeviceList(const base::ListValue* devices);
  void ParseNetworkDevice(const std::string& device_path,
                          const base::DictionaryValue& info);

  // Compare two network profiles by their path.
  static bool AreProfilePathsEqual(const NetworkProfile& a,
                                   const NetworkProfile& b);

  // Empty device observer to ensure that device property updates are received.
  class NetworkLibraryDeviceObserver : public NetworkDeviceObserver {
   public:
    virtual ~NetworkLibraryDeviceObserver() {}
    virtual void OnNetworkDeviceChanged(
        NetworkLibrary* cros, const NetworkDevice* device) OVERRIDE {}
  };

  typedef std::map<std::string, CrosNetworkWatcher*> NetworkWatcherMap;

  base::WeakPtrFactory<NetworkLibraryImplCros> weak_ptr_factory_;

  // For monitoring network manager status changes.
  scoped_ptr<CrosNetworkWatcher> network_manager_watcher_;

  // Network device observer.
  scoped_ptr<NetworkLibraryDeviceObserver> network_device_observer_;

  // Map of monitored networks.
  NetworkWatcherMap monitored_networks_;

  // Map of monitored devices.
  NetworkWatcherMap monitored_devices_;

  base::Time wifi_scan_request_time_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryImplCros);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_IMPL_CROS_H_
