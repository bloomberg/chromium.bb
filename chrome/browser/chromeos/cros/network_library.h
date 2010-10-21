// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
#pragma once

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/platform_thread.h"
#include "base/singleton.h"
#include "base/timer.h"
#include "cros/chromeos_network.h"

namespace chromeos {

// Cellular network is considered low data when less than 60 minues.
static const int kCellularDataLowSecs = 60 * 60;

// Cellular network is considered low data when less than 30 minues.
static const int kCellularDataVeryLowSecs = 30 * 60;

// Cellular network is considered low data when less than 100MB.
static const int kCellularDataLowBytes = 100 * 1024 * 1024;

// Cellular network is considered very low data when less than 50MB.
static const int kCellularDataVeryLowBytes = 50 * 1024 * 1024;

class Network {
 public:
  const std::string& service_path() const { return service_path_; }
  const std::string& device_path() const { return device_path_; }
  const std::string& ip_address() const { return ip_address_; }
  ConnectionType type() const { return type_; }
  ConnectionState connection_state() const { return state_; }
  bool connecting() const { return state_ == STATE_ASSOCIATION ||
      state_ == STATE_CONFIGURATION || state_ == STATE_CARRIER; }
  bool connected() const { return state_ == STATE_READY; }
  bool connecting_or_connected() const { return connecting() || connected(); }
  bool failed() const { return state_ == STATE_FAILURE; }
  ConnectionError error() const { return error_; }
  ConnectionState state() const { return state_; }

  void set_service_path(const std::string& service_path) {
      service_path_ = service_path; }
  void set_connecting(bool connecting) { state_ = (connecting ?
      STATE_ASSOCIATION : STATE_IDLE); }
  void set_connected(bool connected) { state_ = (connected ?
      STATE_READY : STATE_IDLE); }

  // Clear the fields.
  virtual void Clear();

  // Configure the Network from a ServiceInfo object.
  virtual void ConfigureFromService(const ServiceInfo& service);

  // Return a string representation of the state code.
  std::string GetStateString() const;

  // Return a string representation of the error code.
  std::string GetErrorString() const;

 protected:
  Network()
      : type_(TYPE_UNKNOWN),
        state_(STATE_UNKNOWN),
        error_(ERROR_UNKNOWN) {}
  virtual ~Network() {}

  std::string service_path_;
  std::string device_path_;
  std::string ip_address_;
  ConnectionType type_;
  ConnectionState state_;
  ConnectionError error_;
};

class EthernetNetwork : public Network {
 public:
  EthernetNetwork() : Network() {
    type_ = TYPE_ETHERNET;
  }
};

class WirelessNetwork : public Network {
 public:
  // WirelessNetwork are sorted by name.
  bool operator< (const WirelessNetwork& other) const {
    return name_ < other.name();
  }

  // We frequently want to compare networks by service path.
  struct ServicePathEq {
    explicit ServicePathEq(const std::string& path_in) : path(path_in) {}
    bool operator()(const WirelessNetwork& a) {
      return a.service_path().compare(path) == 0;
    }
    const std::string& path;
  };

  const std::string& name() const { return name_; }
  int strength() const { return strength_; }
  bool auto_connect() const { return auto_connect_; }
  bool favorite() const { return favorite_; }

  void set_name(const std::string& name) { name_ = name; }
  void set_strength(int strength) { strength_ = strength; }
  void set_auto_connect(bool auto_connect) { auto_connect_ = auto_connect; }
  void set_favorite(bool favorite) { favorite_ = favorite; }

  // Network overrides.
  virtual void Clear();
  virtual void ConfigureFromService(const ServiceInfo& service);

 protected:
  WirelessNetwork()
      : Network(),
        strength_(0),
        auto_connect_(false),
        favorite_(false) {}

  std::string name_;
  int strength_;
  bool auto_connect_;
  bool favorite_;
};

class CellularNetwork : public WirelessNetwork {
 public:
  enum DataLeft {
    DATA_NORMAL,
    DATA_LOW,
    DATA_VERY_LOW,
    DATA_NONE
  };

  CellularNetwork();
  explicit CellularNetwork(const ServiceInfo& service)
      : WirelessNetwork() {
    ConfigureFromService(service);
  }

  // Starts device activation process. Returns false if the device state does
  // not permit activation.
  bool StartActivation() const;
  const ActivationState activation_state() const { return activation_state_; }
  const NetworkTechnology network_technology() const {
    return network_technology_;
  }
  const NetworkRoamingState roaming_state() const { return roaming_state_; }
  bool restricted_pool() const { return restricted_pool_; }
  const std::string& service_name() const { return service_name_; }
  const std::string& operator_name() const { return operator_name_; }
  const std::string& operator_code() const { return operator_code_; }
  const std::string& payment_url() const { return payment_url_; }
  const std::string& meid() const { return meid_; }
  const std::string& imei() const { return imei_; }
  const std::string& imsi() const { return imsi_; }
  const std::string& esn() const { return esn_; }
  const std::string& mdn() const { return mdn_; }
  const std::string& min() const { return min_; }
  const std::string& model_id() const { return model_id_; }
  const std::string& manufacturer() const { return manufacturer_; }
  const std::string& firmware_revision() const { return firmware_revision_; }
  const std::string& hardware_revision() const { return hardware_revision_; }
  const std::string& last_update() const { return last_update_; }
  const unsigned int prl_version() const { return prl_version_; }
  bool is_gsm() const;
  DataLeft data_left() const;

  // WirelessNetwork overrides.
  virtual void Clear();
  virtual void ConfigureFromService(const ServiceInfo& service);

  const CellularDataPlanList& GetDataPlans() const {
    return data_plans_;
  }

  void SetDataPlans(const CellularDataPlanList& data_plans) {
    data_plans_ = data_plans;
  }
  // Return a string representation of network technology.
  std::string GetNetworkTechnologyString() const;
  // Return a string representation of activation state.
  std::string GetActivationStateString() const;
  // Return a string representation of roaming state.
  std::string GetRoamingStateString() const;

 protected:
  ActivationState activation_state_;
  NetworkTechnology network_technology_;
  NetworkRoamingState roaming_state_;
  bool restricted_pool_;
  std::string service_name_;
  // Carrier Info
  std::string operator_name_;
  std::string operator_code_;
  std::string payment_url_;
  // Device Info
  std::string meid_;
  std::string imei_;
  std::string imsi_;
  std::string esn_;
  std::string mdn_;
  std::string min_;
  std::string model_id_;
  std::string manufacturer_;
  std::string firmware_revision_;
  std::string hardware_revision_;
  std::string last_update_;
  unsigned int prl_version_;
  CellularDataPlanList data_plans_;
};

class WifiNetwork : public WirelessNetwork {
 public:
  WifiNetwork();
  explicit WifiNetwork(const ServiceInfo& service);

  bool encrypted() const { return encryption_ != SECURITY_NONE; }
  ConnectionSecurity encryption() const { return encryption_; }
  const std::string& passphrase() const { return passphrase_; }
  const std::string& identity() const { return identity_; }
  const std::string& cert_path() const { return cert_path_; }

  void set_encryption(ConnectionSecurity encryption) {
    encryption_ = encryption;
  }
  void set_passphrase(const std::string& passphrase) {
    passphrase_ = passphrase;
  }
  void set_identity(const std::string& identity) {
    identity_ = identity;
  }
  void set_cert_path(const std::string& cert_path) {
    cert_path_ = cert_path;
  }

  // WirelessNetwork overrides.
  virtual void Clear();
  virtual void ConfigureFromService(const ServiceInfo& service);

  // Return a string representation of the encryption code.
  // This not translated and should be only used for debugging purposes.
  std::string GetEncryptionString();

  // Return true if cert_path_ indicates that we have loaded the certificate.
  bool IsCertificateLoaded() const;

 protected:
  ConnectionSecurity encryption_;
  std::string passphrase_;
  std::string identity_;
  std::string cert_path_;
};

typedef std::vector<WifiNetwork> WifiNetworkVector;
typedef std::vector<CellularNetwork> CellularNetworkVector;

struct CellTower {
  enum RadioType {
    RADIOTYPE_GSM,
    RADIOTYPE_CDMA,
    RADIOTYPE_WCDMA,
  } radio_type;                   // GSM/WCDMA     CDMA
  int mobile_country_code;        //   MCC          MCC
  int mobile_network_code;        //   MNC          SID
  int location_area_code;         //   LAC          NID
  int cell_id;                    //   CID          BID
  base::Time timestamp;  // Timestamp when this cell was primary
  int signal_strength;   // Radio signal strength measured in dBm.
  int timing_advance;    // Represents the distance from the cell tower.
                         // Each unit is roughly 550 meters.
};

struct WifiAccessPoint {
  std::string mac_address;  // The mac address of the WiFi node.
  std::string name;         // The SSID of the WiFi node.
  base::Time timestamp;     // Timestamp when this AP was detected.
  int signal_strength;      // Radio signal strength measured in dBm.
  int signal_to_noise;      // Current signal to noise ratio measured in dB.
  int channel;              // Wifi channel number.
};

typedef std::vector<CellTower> CellTowerVector;
typedef std::vector<WifiAccessPoint> WifiAccessPointVector;

struct NetworkIPConfig {
  NetworkIPConfig(const std::string& device_path, IPConfigType type,
                  const std::string& address, const std::string& netmask,
                  const std::string& gateway, const std::string& name_servers)
      : device_path(device_path),
        type(type),
        address(address),
        netmask(netmask),
        gateway(gateway),
        name_servers(name_servers) {}

  // NetworkIPConfigs are sorted by tyoe.
  bool operator< (const NetworkIPConfig& other) const {
    return type < other.type;
  }

  std::string device_path;
  IPConfigType type;
  std::string address;  // This looks like "/device/0011aa22bb33"
  std::string netmask;
  std::string gateway;
  std::string name_servers;
};
typedef std::vector<NetworkIPConfig> NetworkIPConfigVector;

// This class handles the interaction with the ChromeOS network library APIs.
// Classes can add themselves as observers. Users can get an instance of the
// library like this: chromeos::CrosLibrary::Get()->GetNetworkLibrary()
class NetworkLibrary {
 public:
  class Observer {
   public:
    // Called when the network has changed. (wifi networks, and ethernet)
    virtual void NetworkChanged(NetworkLibrary* obj) = 0;
    // Called when the cellular data plan has changed.
    virtual void CellularDataPlanChanged(NetworkLibrary* obj) {}
  };

  virtual ~NetworkLibrary() {}
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Return the active Ethernet network (or a default structure if inactive).
  virtual const EthernetNetwork& ethernet_network() const = 0;
  virtual bool ethernet_connecting() const = 0;
  virtual bool ethernet_connected() const = 0;

  // Return the active Wifi network (or a default structure if none active).
  virtual const WifiNetwork& wifi_network() const = 0;
  virtual bool wifi_connecting() const = 0;
  virtual bool wifi_connected() const = 0;

  // Return the active Cellular network (or a default structure if none active).
  virtual const CellularNetwork& cellular_network() const = 0;
  virtual bool cellular_connecting() const = 0;
  virtual bool cellular_connected() const = 0;

  // Return true if any network is currently connected.
  virtual bool Connected() const = 0;

  // Return true if any network is currently connecting.
  virtual bool Connecting() const = 0;

  // Returns the current IP address if connected. If not, returns empty string.
  virtual const std::string& IPAddress() const = 0;

  // Returns the current list of wifi networks.
  virtual const WifiNetworkVector& wifi_networks() const = 0;

  // Returns the list of remembered wifi networks.
  virtual const WifiNetworkVector& remembered_wifi_networks() const = 0;

  // Returns the current list of cellular networks.
  virtual const CellularNetworkVector& cellular_networks() const = 0;

  // Returns the list of remembered cellular networks.
  virtual const CellularNetworkVector& remembered_cellular_networks() const = 0;

  // Search the current list of networks by path and if the network
  // is available, copy the result and return true.
  virtual bool FindWifiNetworkByPath(const std::string& path,
                                     WifiNetwork* result) const = 0;
  virtual bool FindCellularNetworkByPath(const std::string& path,
                                         CellularNetwork* result) const = 0;

  // Request a scan for new wifi networks.
  virtual void RequestWifiScan() = 0;

  // Reads out the results of the last wifi scan. These results are not
  // pre-cached in the library, so the call may block whilst the results are
  // read over IPC.
  // Returns false if an error occurred in reading the results. Note that
  // a true return code only indicates the result set was successfully read,
  // it does not imply a scan has successfully completed yet.
  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result) = 0;

  // TODO(joth): Add GetCellTowers to retrieve a CellTowerVector.

  // Force an update of the system info.
  virtual void UpdateSystemInfo() = 0;

  // Connect to the specified wireless network with password.
  virtual void ConnectToWifiNetwork(WifiNetwork network,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath) = 0;

  // Connect to the specified wifi ssid with password.
  virtual void ConnectToWifiNetwork(const std::string& ssid,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath,
                                    bool auto_connect) = 0;

  // Connect to the specified cellular network.
  virtual void ConnectToCellularNetwork(CellularNetwork network) = 0;

  // Initiates cellular data plan refresh. Plan data will be passed through
  // Network::Observer::CellularDataPlanChanged callback.
  virtual void RefreshCellularDataPlans(const CellularNetwork& network) = 0;

  // Disconnect from the specified wireless (either cellular or wifi) network.
  virtual void DisconnectFromWirelessNetwork(
      const WirelessNetwork& network) = 0;

  // Save network information including passwords (wifi) and auto-connect.
  virtual void SaveCellularNetwork(const CellularNetwork& network) = 0;
  virtual void SaveWifiNetwork(const WifiNetwork& network) = 0;

  // Forget the passed in wireless (either cellular or wifi) network.
  virtual void ForgetWirelessNetwork(const std::string& service_path) = 0;

  virtual bool ethernet_available() const = 0;
  virtual bool wifi_available() const = 0;
  virtual bool cellular_available() const = 0;

  virtual bool ethernet_enabled() const = 0;
  virtual bool wifi_enabled() const = 0;
  virtual bool cellular_enabled() const = 0;

  virtual bool offline_mode() const = 0;

  // Enables/disables the ethernet network device.
  virtual void EnableEthernetNetworkDevice(bool enable) = 0;

  // Enables/disables the wifi network device.
  virtual void EnableWifiNetworkDevice(bool enable) = 0;

  // Enables/disables the cellular network device.
  virtual void EnableCellularNetworkDevice(bool enable) = 0;

  // Enables/disables offline mode.
  virtual void EnableOfflineMode(bool enable) = 0;

  // Fetches IP configs and hardware address for a given device_path.
  // The hardware address is usually a MAC address like "0011AA22BB33".
  // |hardware_address| will be an empty string, if no hardware address is
  // found.
  virtual NetworkIPConfigVector GetIPConfigs(
      const std::string& device_path,
      std::string* hardware_address) = 0;

  // Fetches debug network info for display in about:network.
  // The page will have a meta refresh of |refresh| seconds if |refresh| > 0.
  virtual std::string GetHtmlInfo(int refresh) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static NetworkLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
