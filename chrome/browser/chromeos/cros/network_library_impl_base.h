// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_IMPL_BASE_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_IMPL_BASE_H_

#include <list>
#include <set>

#include "chrome/browser/chromeos/cros/network_library.h"

namespace chromeos {

class NetworkLoginObserver;

class NetworkLibraryImplBase : public NetworkLibrary {
 public:
  NetworkLibraryImplBase();
  virtual ~NetworkLibraryImplBase();

  //////////////////////////////////////////////////////////////////////////////
  // NetworkLibraryImplBase virtual functions.

  // Functions for monitoring networks & devices.
  virtual void MonitorNetworkStart(const std::string& service_path) = 0;
  virtual void MonitorNetworkStop(const std::string& service_path) = 0;
  virtual void MonitorNetworkDeviceStart(const std::string& device_path) = 0;
  virtual void MonitorNetworkDeviceStop(const std::string& device_path) = 0;

  // Called from ConnectToWifiNetwork.
  // Calls ConnectToWifiNetworkUsingConnectData if network request succeeds.
  virtual void CallRequestWifiNetworkAndConnect(
      const std::string& ssid, ConnectionSecurity security) = 0;
  // Called from ConnectToVirtualNetwork*.
  // Calls ConnectToVirtualNetworkUsingConnectData if network request succeeds.
  virtual void CallRequestVirtualNetworkAndConnect(
      const std::string& service_name,
      const std::string& server_hostname,
      ProviderType provider_type) = 0;
  // Call to configure a wifi service. The identifier is either a service_path
  // or a GUID. |info| is a dictionary of property values.
  virtual void CallConfigureService(const std::string& identifier,
                                    const DictionaryValue* info) = 0;
  // Called from NetworkConnectStart.
  // Calls NetworkConnectCompleted when the connection attept completes.
  virtual void CallConnectToNetwork(Network* network) = 0;
  // Called from DeleteRememberedNetwork.
  virtual void CallDeleteRememberedNetwork(
      const std::string& profile_path, const std::string& service_path) = 0;

  // Called from Enable*NetworkDevice.
  // Asynchronously enables or disables the specified device type.
  virtual void CallEnableNetworkDeviceType(
      ConnectionType device, bool enable) = 0;

  // Called from DeleteRememberedNetwork for VPN services.
  // Asynchronously disconnects and removes the service.
  virtual void CallRemoveNetwork(const Network* network) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // NetworkLibrary implementation.

  // virtual Init implemented in derived classes.
  // virtual IsCros implemented in derived classes.

  virtual void AddNetworkManagerObserver(
      NetworkManagerObserver* observer) OVERRIDE;
  virtual void RemoveNetworkManagerObserver(
      NetworkManagerObserver* observer) OVERRIDE;
  virtual void AddNetworkObserver(const std::string& service_path,
                                  NetworkObserver* observer) OVERRIDE;
  virtual void RemoveNetworkObserver(const std::string& service_path,
                                     NetworkObserver* observer) OVERRIDE;
  virtual void RemoveObserverForAllNetworks(
      NetworkObserver* observer) OVERRIDE;
  virtual void AddNetworkDeviceObserver(
      const std::string& device_path,
      NetworkDeviceObserver* observer) OVERRIDE;
  virtual void RemoveNetworkDeviceObserver(
      const std::string& device_path,
      NetworkDeviceObserver* observer) OVERRIDE;

  virtual void Lock() OVERRIDE;
  virtual void Unlock() OVERRIDE;
  virtual bool IsLocked() OVERRIDE;

  virtual void AddCellularDataPlanObserver(
      CellularDataPlanObserver* observer) OVERRIDE;
  virtual void RemoveCellularDataPlanObserver(
      CellularDataPlanObserver* observer) OVERRIDE;
  virtual void AddPinOperationObserver(
      PinOperationObserver* observer) OVERRIDE;
  virtual void RemovePinOperationObserver(
      PinOperationObserver* observer) OVERRIDE;
  virtual void AddUserActionObserver(
      UserActionObserver* observer) OVERRIDE;
  virtual void RemoveUserActionObserver(
      UserActionObserver* observer) OVERRIDE;

  virtual const EthernetNetwork* ethernet_network() const OVERRIDE;
  virtual bool ethernet_connecting() const OVERRIDE;
  virtual bool ethernet_connected() const OVERRIDE;
  virtual const WifiNetwork* wifi_network() const OVERRIDE;
  virtual bool wifi_connecting() const OVERRIDE;
  virtual bool wifi_connected() const OVERRIDE;
  virtual const CellularNetwork* cellular_network() const OVERRIDE;
  virtual bool cellular_connecting() const OVERRIDE;
  virtual bool cellular_connected() const OVERRIDE;
  virtual const WimaxNetwork* wimax_network() const OVERRIDE;
  virtual bool wimax_connecting() const OVERRIDE;
  virtual bool wimax_connected() const OVERRIDE;
  virtual const Network* mobile_network() const OVERRIDE;
  virtual bool mobile_connecting() const OVERRIDE;
  virtual bool mobile_connected() const OVERRIDE;
  virtual const VirtualNetwork* virtual_network() const OVERRIDE;
  virtual bool virtual_network_connecting() const OVERRIDE;
  virtual bool virtual_network_connected() const OVERRIDE;
  virtual bool Connected() const OVERRIDE;
  virtual bool Connecting() const OVERRIDE;
  virtual const WifiNetworkVector& wifi_networks() const OVERRIDE;
  virtual const WifiNetworkVector& remembered_wifi_networks() const OVERRIDE;
  virtual const CellularNetworkVector& cellular_networks() const OVERRIDE;
  virtual const WimaxNetworkVector& wimax_networks() const OVERRIDE;
  virtual const VirtualNetworkVector& virtual_networks() const OVERRIDE;
  virtual const VirtualNetworkVector&
      remembered_virtual_networks() const OVERRIDE;
  virtual const Network* active_network() const OVERRIDE;
  virtual const Network* connected_network() const OVERRIDE;
  virtual const Network* connecting_network() const OVERRIDE;
  virtual bool ethernet_available() const OVERRIDE;
  virtual bool wifi_available() const OVERRIDE;
  virtual bool wimax_available() const OVERRIDE;
  virtual bool cellular_available() const OVERRIDE;
  virtual bool mobile_available() const OVERRIDE;
  virtual bool ethernet_enabled() const OVERRIDE;
  virtual bool wifi_enabled() const OVERRIDE;
  virtual bool wimax_enabled() const OVERRIDE;
  virtual bool cellular_enabled() const OVERRIDE;
  virtual bool mobile_enabled() const OVERRIDE;
  virtual bool ethernet_busy() const OVERRIDE;
  virtual bool wifi_busy() const OVERRIDE;
  virtual bool wimax_busy() const OVERRIDE;
  virtual bool cellular_busy() const OVERRIDE;
  virtual bool mobile_busy() const OVERRIDE;
  virtual bool wifi_scanning() const OVERRIDE;
  virtual bool offline_mode() const OVERRIDE;
  virtual std::string GetCheckPortalList() const OVERRIDE;
  // virtual SetCheckPortalList implemented in derived classes.
  // virtual SetDefaultCheckPortalList implemented in derived classes.
  virtual const std::string& IPAddress() const OVERRIDE;

  virtual const NetworkDevice* FindNetworkDeviceByPath(
      const std::string& path) const OVERRIDE;
  NetworkDevice* FindNetworkDeviceByPath(const std::string& path);
  virtual const NetworkDevice* FindMobileDevice() const OVERRIDE;
  virtual const NetworkDevice* FindWimaxDevice() const OVERRIDE;
  virtual const NetworkDevice* FindCellularDevice() const OVERRIDE;
  virtual const NetworkDevice* FindEthernetDevice() const OVERRIDE;
  virtual const NetworkDevice* FindWifiDevice() const OVERRIDE;
  virtual Network* FindNetworkByPath(const std::string& path) const OVERRIDE;
  virtual Network* FindNetworkByUniqueId(
      const std::string& unique_id) const OVERRIDE;
  WirelessNetwork* FindWirelessNetworkByPath(const std::string& path) const;
  virtual WifiNetwork* FindWifiNetworkByPath(
      const std::string& path) const OVERRIDE;
  virtual WimaxNetwork* FindWimaxNetworkByPath(
      const std::string& path) const OVERRIDE;
  virtual CellularNetwork* FindCellularNetworkByPath(
      const std::string& path) const OVERRIDE;
  virtual VirtualNetwork* FindVirtualNetworkByPath(
      const std::string& path) const OVERRIDE;
  Network* FindRememberedFromNetwork(const Network* network) const;
  virtual Network* FindRememberedNetworkByPath(
      const std::string& path) const OVERRIDE;
  virtual Network* FindRememberedNetworkByUniqueId(
      const std::string& unique_id) const OVERRIDE;

  virtual const base::DictionaryValue* FindOncForNetwork(
      const std::string& unique_id) const OVERRIDE;

  virtual const CellularDataPlanVector* GetDataPlans(
      const std::string& path) const OVERRIDE;
  virtual const CellularDataPlan* GetSignificantDataPlan(
      const std::string& path) const OVERRIDE;
  virtual void SignalCellularPlanPayment() OVERRIDE;
  virtual bool HasRecentCellularPlanPayment() OVERRIDE;
  virtual const std::string& GetCellularHomeCarrierId() const OVERRIDE;

  // virtual ChangePin implemented in derived classes.
  // virtual ChangeRequiredPin implemented in derived classes.
  // virtual EnterPin implemented in derived classes.
  // virtual UnblockPin implemented in derived classes.

  // virtual RequestCellularScan implemented in derived classes.
  // virtual RequestCellularRegister implemented in derived classes.
  // virtual SetCellularDataRoamingAllowed implemented in derived classes.
  // virtual IsCellularAlwaysInRoaming implemented in derived classes.
  // virtual RequestNetworkScan implemented in derived classes.
  // virtual GetWifiAccessPoints implemented in derived classes.

  virtual bool HasProfileType(NetworkProfileType type) const OVERRIDE;
  virtual void SetNetworkProfile(const std::string& service_path,
                                 NetworkProfileType type) OVERRIDE;
  virtual bool CanConnectToNetwork(const Network* network) const OVERRIDE;

  // Connect to an existing network.
  virtual void ConnectToWifiNetwork(WifiNetwork* wifi) OVERRIDE;
  virtual void ConnectToWifiNetwork(WifiNetwork* wifi, bool shared) OVERRIDE;
  virtual void ConnectToWimaxNetwork(WimaxNetwork* wimax) OVERRIDE;
  virtual void ConnectToWimaxNetwork(WimaxNetwork* wimax, bool shared) OVERRIDE;
  virtual void ConnectToCellularNetwork(CellularNetwork* cellular) OVERRIDE;
  virtual void ConnectToVirtualNetwork(VirtualNetwork* vpn) OVERRIDE;

  // Request a network and connect to it.
  virtual void ConnectToUnconfiguredWifiNetwork(
      const std::string& ssid,
      ConnectionSecurity security,
      const std::string& passphrase,
      const EAPConfigData* eap_config,
      bool save_credentials,
      bool shared) OVERRIDE;

  virtual void ConnectToUnconfiguredVirtualNetwork(
      const std::string& service_name,
      const std::string& server_hostname,
      ProviderType provider_type,
      const VPNConfigData& config) OVERRIDE;

  // virtual DisconnectFromNetwork implemented in derived classes.
  virtual void ForgetNetwork(const std::string& service_path) OVERRIDE;
  virtual void EnableEthernetNetworkDevice(bool enable) OVERRIDE;
  virtual void EnableWifiNetworkDevice(bool enable) OVERRIDE;
  virtual void EnableMobileNetworkDevice(bool enable) OVERRIDE;
  virtual void EnableWimaxNetworkDevice(bool enable) OVERRIDE;
  virtual void EnableCellularNetworkDevice(bool enable) OVERRIDE;
  // virtual EnableOfflineMode implemented in derived classes.
  // virtual GetIPConfigs implemented in derived classes.
  // virtual SetIPConfig implemented in derived classes.
  virtual void SwitchToPreferredNetwork() OVERRIDE;
  virtual bool LoadOncNetworks(const std::string& onc_blob,
                               const std::string& passphrase,
                               NetworkUIData::ONCSource source,
                               std::string* error) OVERRIDE;
  virtual bool SetActiveNetwork(ConnectionType type,
                                const std::string& service_path) OVERRIDE;

 protected:
  typedef ObserverList<NetworkObserver> NetworkObserverList;
  typedef std::map<std::string, NetworkObserverList*> NetworkObserverMap;

  typedef ObserverList<NetworkDeviceObserver> NetworkDeviceObserverList;
  typedef std::map<std::string, NetworkDeviceObserverList*>
      NetworkDeviceObserverMap;

  typedef std::map<std::string, Network*> NetworkMap;
  typedef std::map<std::string, int> PriorityMap;
  typedef std::map<std::string, NetworkDevice*> NetworkDeviceMap;
  typedef std::map<std::string, CellularDataPlanVector*> CellularDataPlanMap;
  typedef std::map<std::string, const base::DictionaryValue*> NetworkOncMap;
  typedef std::map<NetworkUIData::ONCSource,
                   std::set<std::string> > NetworkSourceMap;

  struct NetworkProfile {
    NetworkProfile(const std::string& p, NetworkProfileType t)
        : path(p), type(t) {}
    ~NetworkProfile() {}
    std::string path;
    NetworkProfileType type;
    typedef std::set<std::string> ServiceList;
    ServiceList services;
  };
  typedef std::list<NetworkProfile> NetworkProfileList;

  struct ConnectData {
    ConnectData() :
        security(SECURITY_NONE),
        eap_method(EAP_METHOD_UNKNOWN),
        eap_auth(EAP_PHASE_2_AUTH_AUTO),
        eap_use_system_cas(false),
        save_credentials(false),
        profile_type(PROFILE_NONE) {}
    ~ConnectData() {}
    ConnectionSecurity security;
    std::string service_name;  // For example, SSID.
    std::string username;
    std::string passphrase;
    std::string otp;
    std::string group_name;
    std::string server_hostname;
    std::string server_ca_cert_nss_nickname;
    std::string client_cert_pkcs11_id;
    EAPMethod eap_method;
    EAPPhase2Auth eap_auth;
    bool eap_use_system_cas;
    std::string eap_identity;
    std::string eap_anonymous_identity;
    std::string psk_key;
    bool save_credentials;
    NetworkProfileType profile_type;
  };

  enum NetworkConnectStatus {
    CONNECT_SUCCESS,
    CONNECT_BAD_PASSPHRASE,
    CONNECT_FAILED
  };

  // Finds device by connection type.
  const NetworkDevice* FindDeviceByType(ConnectionType type) const;
  // Called from ConnectTo*Network.
  void NetworkConnectStartWifi(
      WifiNetwork* network, NetworkProfileType profile_type);
  void NetworkConnectStartVPN(VirtualNetwork* network);
  void NetworkConnectStart(Network* network, NetworkProfileType profile_type);
  // Called from CallConnectToNetwork.
  void NetworkConnectCompleted(Network* network,
                               NetworkConnectStatus status);
  // Called from CallRequestWifiNetworkAndConnect.
  void ConnectToWifiNetworkUsingConnectData(WifiNetwork* wifi);
  // Called from CallRequestVirtualNetworkAndConnect.
  void ConnectToVirtualNetworkUsingConnectData(VirtualNetwork* vpn);
  // Called from GetSignificantDataPlan.
  const CellularDataPlan* GetSignificantDataPlanFromVector(
      const CellularDataPlanVector* plans) const;
  CellularNetwork::DataLeft GetDataLeft(
      CellularDataPlanVector* data_plan_vector);
  // Takes ownership of |data_plan|.
  void UpdateCellularDataPlan(const std::string& service_path,
                              CellularDataPlanVector* data_plan_vector);

  // Network list management functions.
  void ClearActiveNetwork(ConnectionType type);
  void UpdateActiveNetwork(Network* network);
  void AddNetwork(Network* network);
  void DeleteNetwork(Network* network);

  // Checks whether |network| has meanwhile been pruned by ONC policy. If so,
  // instructs flimflam to remove the network, deletes |network| and returns
  // false.
  bool ValidateRememberedNetwork(Network* network);

  // Adds |network| to the remembered networks data structures and returns true
  // if ValidateRememberedNetwork(network) returns true. Returns false
  // otherwise.
  bool ValidateAndAddRememberedNetwork(Network* network);

  void DeleteRememberedNetwork(const std::string& service_path);
  void ClearNetworks();
  void ClearRememberedNetworks();
  void DeleteNetworks();
  void DeleteRememberedNetworks();
  void DeleteDevice(const std::string& device_path);
  void DeleteDeviceFromDeviceObserversMap(const std::string& device_path);

  // Profile management functions.
  void AddProfile(const std::string& profile_path,
                  NetworkProfileType profile_type);
  NetworkProfile* GetProfileForType(NetworkProfileType type);
  void SetProfileType(Network* network, NetworkProfileType type);
  void SetProfileTypeFromPath(Network* network);
  std::string GetProfilePath(NetworkProfileType type);

  // Notifications.
  void NotifyNetworkManagerChanged(bool force_update);
  void SignalNetworkManagerObservers();
  void NotifyNetworkChanged(const Network* network);
  void NotifyNetworkDeviceChanged(NetworkDevice* device, PropertyIndex index);
  void NotifyCellularDataPlanChanged();
  void NotifyPinOperationCompleted(PinOperationError error);
  void NotifyUserConnectionInitiated(const Network* network);

  // TPM related functions.
  void GetTpmInfo();
  const std::string& GetTpmSlot();
  const std::string& GetTpmPin();

  // Network manager observer list.
  ObserverList<NetworkManagerObserver> network_manager_observers_;

  // Cellular data plan observer list.
  ObserverList<CellularDataPlanObserver> data_plan_observers_;

  // PIN operation observer list.
  ObserverList<PinOperationObserver> pin_operation_observers_;

  // User action observer list.
  ObserverList<UserActionObserver> user_action_observers_;

  // Network observer map.
  NetworkObserverMap network_observers_;

  // Network device observer map.
  NetworkDeviceObserverMap network_device_observers_;

  // Network login observer.
  scoped_ptr<NetworkLoginObserver> network_login_observer_;

  // List of profiles.
  NetworkProfileList profile_list_;

  // A service path based map of all visible Networks.
  NetworkMap network_map_;

  // A unique_id based map of all visible Networks.
  NetworkMap network_unique_id_map_;

  // A service path based map of all remembered Networks.
  NetworkMap remembered_network_map_;

  // A unique_id based map of all remembered Networks.
  NetworkMap remembered_network_unique_id_map_;

  // A list of services that we are awaiting updates for.
  PriorityMap network_update_requests_;

  // A device path based map of all NetworkDevices.
  NetworkDeviceMap device_map_;

  // A network service path based map of all CellularDataPlanVectors.
  CellularDataPlanMap data_plan_map_;

  // The ethernet network.
  EthernetNetwork* ethernet_;

  // The list of available wifi networks.
  WifiNetworkVector wifi_networks_;

  // The current connected (or connecting) wifi network.
  WifiNetwork* active_wifi_;

  // The remembered wifi networks.
  WifiNetworkVector remembered_wifi_networks_;

  // The list of available cellular networks.
  CellularNetworkVector cellular_networks_;

  // The list of available wimax networks.
  WimaxNetworkVector wimax_networks_;

  // The current connected (or connecting) cellular network.
  CellularNetwork* active_cellular_;

  // The current connected (or connecting) Wimax network.
  WimaxNetwork* active_wimax_;

  // The list of available virtual networks.
  VirtualNetworkVector virtual_networks_;

  // The current connected (or connecting) virtual network.
  VirtualNetwork* active_virtual_;

  // The remembered virtual networks.
  VirtualNetworkVector remembered_virtual_networks_;

  // The path of the active profile (for retrieving remembered services).
  std::string active_profile_path_;

  // The current available network devices. Bitwise flag of ConnectionTypes.
  int available_devices_;

  // The current enabled network devices. Bitwise flag of ConnectionTypes.
  int enabled_devices_;

  // The current busy network devices. Bitwise flag of ConnectionTypes.
  // Busy means device is switching from enable/disable state.
  int busy_devices_;

  // The current connected network devices. Bitwise flag of ConnectionTypes.
  int connected_devices_;

  // True if we are currently scanning for wifi networks.
  bool wifi_scanning_;

  // Currently not implemented. TODO(stevenjb): implement or eliminate.
  bool offline_mode_;

  // List of interfaces for which portal check is enabled.
  std::string check_portal_list_;

  // True if access network library is locked.
  bool is_locked_;

  // TPM module user slot and PIN, needed by flimflam to access certificates.
  std::string tpm_slot_;
  std::string tpm_pin_;

  // Type of pending SIM operation, SIM_OPERATION_NONE otherwise.
  SimOperationType sim_operation_;

 private:
  // List of networks to move to the user profile once logged in.
  std::list<std::string> user_networks_;

  // Weak pointer factory for canceling a network change callback.
  base::WeakPtrFactory<NetworkLibraryImplBase> notify_manager_weak_factory_;

  // Cellular plan payment time.
  base::Time cellular_plan_payment_time_;

  // Temporary connection data for async connect calls.
  ConnectData connect_data_;

  // Holds unique id to ONC mapping.
  NetworkOncMap network_onc_map_;

  // Keeps track of what networks ONC has configured. This is used to weed out
  // stray networks that flimflam still has on file, but are not known on the
  // Chrome side.
  NetworkSourceMap network_source_map_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryImplBase);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_IMPL_BASE_H_
