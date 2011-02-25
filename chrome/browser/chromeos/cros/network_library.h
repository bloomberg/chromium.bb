// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
#pragma once

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/scoped_vector.h"
#include "base/singleton.h"
#include "base/string16.h"
#include "base/timer.h"
#include "third_party/cros/chromeos_network.h"

class DictionaryValue;
class Value;

namespace chromeos {

// Cellular network is considered low data when less than 60 minues.
static const int kCellularDataLowSecs = 60 * 60;

// Cellular network is considered low data when less than 30 minues.
static const int kCellularDataVeryLowSecs = 30 * 60;

// Cellular network is considered low data when less than 100MB.
static const int kCellularDataLowBytes = 100 * 1024 * 1024;

// Cellular network is considered very low data when less than 50MB.
static const int kCellularDataVeryLowBytes = 50 * 1024 * 1024;

// Contains data related to the flimflam.Device interface,
// e.g. ethernet, wifi, cellular.
class NetworkDevice {
 public:
  explicit NetworkDevice(const std::string& device_path);
  void ParseInfo(const DictionaryValue* info);

  // Device info.
  const std::string& device_path() const { return device_path_; }
  const std::string& name() const { return name_; }
  ConnectionType type() const { return type_; }
  bool scanning() const { return scanning_; }
  const std::string& meid() const { return MEID_; }
  const std::string& imei() const { return IMEI_; }
  const std::string& imsi() const { return IMSI_; }
  const std::string& esn() const { return ESN_; }
  const std::string& mdn() const { return MDN_; }
  const std::string& min() const { return MIN_; }
  const std::string& model_id() const { return model_id_; }
  const std::string& manufacturer() const { return manufacturer_; }
  const std::string& firmware_revision() const { return firmware_revision_; }
  const std::string& hardware_revision() const { return hardware_revision_; }
  const std::string& last_update() const { return last_update_; }
  const unsigned int prl_version() const { return PRL_version_; }

 protected:
  // General device info.
  std::string device_path_;
  std::string name_;
  ConnectionType type_;
  bool scanning_;
  // Cellular specific device info.
  std::string carrier_;
  std::string MEID_;
  std::string IMEI_;
  std::string IMSI_;
  std::string ESN_;
  std::string MDN_;
  std::string MIN_;
  std::string model_id_;
  std::string manufacturer_;
  std::string firmware_revision_;
  std::string hardware_revision_;
  std::string last_update_;
  int PRL_version_;
};

// Contains data common to all network service types.
class Network {
 public:
  virtual ~Network() {}

  const std::string& service_path() const { return service_path_; }
  const std::string& name() const { return name_; }
  const std::string& device_path() const { return device_path_; }
  const std::string& ip_address() const { return ip_address_; }
  ConnectionType type() const { return type_; }
  ConnectionMode mode() const { return mode_; }
  ConnectionState connection_state() const { return state_; }
  bool connecting() const { return state_ == STATE_ASSOCIATION ||
                                   state_ == STATE_CONFIGURATION ||
                                   state_ == STATE_CARRIER; }
  bool configuring() const { return state_ == STATE_CONFIGURATION; }
  bool connected() const { return state_ == STATE_READY; }
  bool connecting_or_connected() const { return connecting() || connected(); }
  bool failed() const { return state_ == STATE_FAILURE; }
  bool failed_or_disconnected() const {
    return failed() || state_ == STATE_IDLE;
  }
  ConnectionError error() const { return error_; }
  ConnectionState state() const { return state_; }
  ConnectionState prev_state() const { return prev_state_; }
  // Is this network connectable. Some networks are not yet ready to be
  // connected. For example, an 8021X network without certificates.
  bool connectable() const { return connectable_; }
  // Is this the active network, i.e, the one through which
  // network traffic is being routed? A network can be connected,
  // but not be carrying traffic.
  bool is_active() const { return is_active_; }

  // Parse name/value pairs from libcros.
  virtual void ParseInfo(const DictionaryValue* info);

  // Return a string representation of the state code.
  std::string GetStateString() const;

  // Return a string representation of the error code.
  std::string GetErrorString() const;

 protected:
  Network(const std::string& service_path, ConnectionType type)
      : state_(STATE_UNKNOWN),
        error_(ERROR_UNKNOWN),
        connectable_(true),
        is_active_(false),
        service_path_(service_path),
        type_(type),
        prev_state_(STATE_UNKNOWN) {}

  std::string device_path_;
  std::string name_;
  std::string ip_address_;
  ConnectionMode mode_;
  ConnectionState state_;
  ConnectionError error_;
  bool connectable_;
  bool is_active_;

 private:
  void set_connecting(bool connecting) { state_ = (connecting ?
      STATE_ASSOCIATION : STATE_IDLE); }
  void set_connected(bool connected) { state_ = (connected ?
      STATE_READY : STATE_IDLE); }
  void set_state(ConnectionState state) { state_ = state; }
  void set_prev_state(ConnectionState state) { prev_state_ = state; }
  void set_connectable(bool connectable) { connectable_ = connectable; }
  void set_active(bool is_active) { is_active_ = is_active; }
  void set_error(ConnectionError error) { error_ = error; }

  // Initialize the IP address field
  void InitIPAddress();

  // These must not be modified after construction.
  std::string service_path_;
  ConnectionType type_;

  // Local state.
  ConnectionState prev_state_;

  friend class NetworkLibraryImpl;
};

// Class for networks of TYPE_ETHERNET.
class EthernetNetwork : public Network {
 public:
  explicit EthernetNetwork(const std::string& service_path) :
      Network(service_path, TYPE_ETHERNET) {
  }
};

// Base class for networks of TYPE_WIFI or TYPE_CELLULAR.
class WirelessNetwork : public Network {
 public:
  // WirelessNetwork are sorted by name.
  bool operator< (const WirelessNetwork& other) const {
    return name_ < other.name();
  }

  // We frequently want to compare networks by service path.
  struct ServicePathEq {
    explicit ServicePathEq(const std::string& path_in) : path(path_in) {}
    bool operator()(const WirelessNetwork* a) {
      return a->service_path().compare(path) == 0;
    }
    const std::string& path;
  };

  int strength() const { return strength_; }
  bool auto_connect() const { return auto_connect_; }
  bool favorite() const { return favorite_; }

  void set_auto_connect(bool auto_connect) { auto_connect_ = auto_connect; }
  // We don't have a setter for |favorite_| because to unfavorite a network is
  // equivalent to forget a network, so we call forget network on cros for
  // that.  See ForgetWifiNetwork().

  // Network overrides.
  virtual void ParseInfo(const DictionaryValue* info);

 protected:
  WirelessNetwork(const std::string& service_path, ConnectionType type)
      : Network(service_path, type),
        strength_(0),
        auto_connect_(false),
        favorite_(false) {}
  int strength_;
  bool auto_connect_;
  bool favorite_;

 private:
  // ChangeAutoConnectSaveTest accesses |favorite_|.
  FRIEND_TEST_ALL_PREFIXES(WifiConfigViewTest, ChangeAutoConnectSaveTest);

  void set_name(const std::string& name) { name_ = name; }
  void set_strength(int strength) { strength_ = strength; }

  friend class NetworkLibraryImpl;
};

// Class for networks of TYPE_CELLULAR.
class CellularDataPlan;
class CellularNetwork : public WirelessNetwork {
 public:
  enum DataLeft {
    DATA_UNKNOWN,
    DATA_NORMAL,
    DATA_LOW,
    DATA_VERY_LOW,
    DATA_NONE
  };

  virtual ~CellularNetwork();

  explicit CellularNetwork(const std::string& service_path)
      : WirelessNetwork(service_path, TYPE_CELLULAR),
        activation_state_(ACTIVATION_STATE_UNKNOWN),
        network_technology_(NETWORK_TECHNOLOGY_UNKNOWN),
        roaming_state_(ROAMING_STATE_UNKNOWN),
        connectivity_state_(CONN_STATE_UNKNOWN),
        data_left_(DATA_UNKNOWN) {
  }
  // Starts device activation process. Returns false if the device state does
  // not permit activation.
  bool StartActivation() const;
  const ActivationState activation_state() const { return activation_state_; }
  const NetworkTechnology network_technology() const {
    return network_technology_;
  }
  const NetworkRoamingState roaming_state() const { return roaming_state_; }
  const ConnectivityState connectivity_state() const {
    return connectivity_state_;
  }
  bool restricted_pool() const {
    return connectivity_state() == CONN_STATE_RESTRICTED;
  }
  bool needs_new_plan() const {
    return restricted_pool() && connected() &&
        activation_state() == ACTIVATION_STATE_ACTIVATED;
  }
  const std::string& operator_name() const { return operator_name_; }
  const std::string& operator_code() const { return operator_code_; }
  const std::string& payment_url() const { return payment_url_; }
  DataLeft data_left() const { return data_left_; }

  // Misc.
  bool is_gsm() const {
    return network_technology_ != NETWORK_TECHNOLOGY_EVDO &&
        network_technology_ != NETWORK_TECHNOLOGY_1XRTT &&
        network_technology_ != NETWORK_TECHNOLOGY_UNKNOWN;
  }

  // WirelessNetwork overrides.
  virtual void ParseInfo(const DictionaryValue* info);

  // Return a string representation of network technology.
  std::string GetNetworkTechnologyString() const;
  // Return a string representation of connectivity state.
  std::string GetConnectivityStateString() const;
  // Return a string representation of activation state.
  std::string GetActivationStateString() const;
  // Return a string representation of roaming state.
  std::string GetRoamingStateString() const;

  // Return a string representation of |activation_state|.
  static std::string ActivationStateToString(ActivationState activation_state);

 protected:

  ActivationState activation_state_;
  NetworkTechnology network_technology_;
  NetworkRoamingState roaming_state_;
  ConnectivityState connectivity_state_;
  // Carrier Info
  std::string operator_name_;
  std::string operator_code_;
  std::string payment_url_;
  // Cached values
  DataLeft data_left_;  // Updated when data plans are updated.

 private:
  void set_activation_state(ActivationState state) {
    activation_state_ = state;
  }
  void set_payment_url(const std::string& url) { payment_url_ = url; }
  void set_network_technology(NetworkTechnology technology) {
    network_technology_ = technology;
  }
  void set_roaming_state(NetworkRoamingState state) { roaming_state_ = state; }
  void set_connectivity_state(ConnectivityState connectivity_state) {
    connectivity_state_ = connectivity_state;
  }
  void set_data_left(DataLeft data_left) { data_left_ = data_left; }

  friend class NetworkLibraryImpl;
};
typedef std::vector<CellularNetwork*> CellularNetworkVector;

// Class for networks of TYPE_WIFI.
class WifiNetwork : public WirelessNetwork {
 public:
  explicit WifiNetwork(const std::string& service_path)
      : WirelessNetwork(service_path, TYPE_WIFI),
        encryption_(SECURITY_NONE) {
  }

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
  virtual void ParseInfo(const DictionaryValue* info);

  // Return a string representation of the encryption code.
  // This not translated and should be only used for debugging purposes.
  std::string GetEncryptionString();

  // Return true if a passphrase or other input is required to connect.
  bool IsPassphraseRequired() const;

  // Return true if cert_path_ indicates that we have loaded the certificate.
  bool IsCertificateLoaded() const;

 protected:
  ConnectionSecurity encryption_;
  std::string passphrase_;
  bool passphrase_required_;
  std::string identity_;
  std::string cert_path_;
};
typedef std::vector<WifiNetwork*> WifiNetworkVector;

// Cellular Data Plan management.
class CellularDataPlan {
 public:
  CellularDataPlan()
      : plan_name("Unknown"),
        plan_type(CELLULAR_DATA_PLAN_UNLIMITED),
        plan_data_bytes(0),
        data_bytes_used(0) { }
  explicit CellularDataPlan(const CellularDataPlanInfo &plan)
      : plan_name(plan.plan_name ? plan.plan_name : ""),
        plan_type(plan.plan_type),
        update_time(base::Time::FromInternalValue(plan.update_time)),
        plan_start_time(base::Time::FromInternalValue(plan.plan_start_time)),
        plan_end_time(base::Time::FromInternalValue(plan.plan_end_time)),
        plan_data_bytes(plan.plan_data_bytes),
        data_bytes_used(plan.data_bytes_used) { }
  // Formats cellular plan description.
  string16 GetPlanDesciption() const;
  // Evaluates cellular plans status and returns warning string if it is near
  // expiration.
  string16 GetRemainingWarning() const;
  // Formats remaining plan data description.
  string16 GetDataRemainingDesciption() const;
  // Formats plan expiration description.
  string16 GetPlanExpiration() const;
  // Formats plan usage info.
  string16 GetUsageInfo() const;
  // Returns a unique string for this plan that can be used for comparisons.
  std::string GetUniqueIdentifier() const;
  base::TimeDelta remaining_time() const;
  int64 remaining_minutes() const;
  int64 remaining_data() const;
  int64 remaining_mbytes() const;
  // TODO(stevenjb): Make these private with accessors and properly named.
  std::string plan_name;
  CellularDataPlanType plan_type;
  base::Time update_time;
  base::Time plan_start_time;
  base::Time plan_end_time;
  int64 plan_data_bytes;
  int64 data_bytes_used;
};
typedef ScopedVector<CellularDataPlan> CellularDataPlanVector;

// Geolocation data.
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

// IP Configuration.
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
  class NetworkManagerObserver {
   public:
    // Called when the state of the network manager has changed,
    // for example, networks have appeared or disappeared.
    virtual void OnNetworkManagerChanged(NetworkLibrary* obj) = 0;
  };

  class NetworkObserver {
   public:
    // Called when the state of a single network has changed,
    // for example signal strength or connection state.
    virtual void OnNetworkChanged(NetworkLibrary* cros,
                                  const Network* network) = 0;
  };

  class CellularDataPlanObserver {
   public:
    // Called when the cellular data plan has changed.
    virtual void OnCellularDataPlanChanged(NetworkLibrary* obj) = 0;
  };

  class UserActionObserver {
   public:
    // Called when user initiates a new connection.
    // Network is NULL when we don't have an associated Network object.
    virtual void OnConnectionInitiated(NetworkLibrary* cros,
                                       const Network* network) = 0;
  };

  virtual ~NetworkLibrary() {}

  virtual void AddNetworkManagerObserver(NetworkManagerObserver* observer) = 0;
  virtual void RemoveNetworkManagerObserver(
      NetworkManagerObserver* observer) = 0;

  // An attempt to add an observer that has already been added for a
  // give service path will be ignored.
  virtual void AddNetworkObserver(const std::string& service_path,
                                  NetworkObserver* observer) = 0;
  // Remove an observer of a single network
  virtual void RemoveNetworkObserver(const std::string& service_path,
                                     NetworkObserver* observer) = 0;
  // Stop |observer| from observing any networks
  virtual void RemoveObserverForAllNetworks(NetworkObserver* observer) = 0;

  // Temporarily locks down certain functionality in network library to prevent
  // unplanned side effects. During the lock down, Enable*Device() calls cannot
  // be made.
  virtual void Lock() = 0;
  // Removes temporarily lock of network library.
  virtual void Unlock() = 0;
  // Checks if access to network library is locked.
  virtual bool IsLocked() = 0;

  virtual void AddCellularDataPlanObserver(
      CellularDataPlanObserver* observer) = 0;
  virtual void RemoveCellularDataPlanObserver(
      CellularDataPlanObserver* observer) = 0;

  virtual void AddUserActionObserver(UserActionObserver* observer) = 0;
  virtual void RemoveUserActionObserver(UserActionObserver* observer) = 0;

  // Return the active Ethernet network (or a default structure if inactive).
  virtual const EthernetNetwork* ethernet_network() const = 0;
  virtual bool ethernet_connecting() const = 0;
  virtual bool ethernet_connected() const = 0;

  // Return the active Wifi network (or a default structure if none active).
  virtual const WifiNetwork* wifi_network() const = 0;
  virtual bool wifi_connecting() const = 0;
  virtual bool wifi_connected() const = 0;

  // Return the active Cellular network (or a default structure if none active).
  virtual const CellularNetwork* cellular_network() const = 0;
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

  // Return a pointer to the device, if it exists, or NULL.
  virtual const NetworkDevice* FindNetworkDeviceByPath(
      const std::string& path) const = 0;

  // Return a pointer to the network, if it exists, or NULL.
  virtual const WifiNetwork* FindWifiNetworkByPath(
      const std::string& path) const = 0;
  virtual const CellularNetwork* FindCellularNetworkByPath(
      const std::string& path) const = 0;

  // Retrieves the data plans associated with |path|, NULL if there are no
  // associated plans.
  virtual const CellularDataPlanVector* GetDataPlans(
      const std::string& path) const = 0;

  // This returns the significant data plan. If the user only has the
  // base data plan, then return that. If there is a base and a paid data plan,
  // then the significant one is the paid one. So return the paid plan.
  // If there are no data plans, then this method returns NULL.
  // This returns a pointer to a member of data_plans_, so if SetDataPlans()
  // gets called, the result becomes invalid.
  virtual const CellularDataPlan* GetSignificantDataPlan(
      const std::string& path) const = 0;

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

  // TODO(stevenjb): eliminate Network* version of Connect functions.
  // Instead, always use service_path and improve the error handling.
  // Connect to the specified wireless network with password.
  // Returns false if the attempt fails immediately (e.g. passphrase too short)
  // and sets network->error().
  virtual bool ConnectToWifiNetwork(WifiNetwork* network,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath) = 0;

  // Same as above but searches for an existing network by name.
  virtual bool ConnectToWifiNetwork(const std::string& service_path,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath) = 0;

  // Connect to the specified network with security, ssid, and password.
  // Returns false if the attempt fails immediately (e.g. passphrase too short).
  virtual bool ConnectToWifiNetwork(ConnectionSecurity security,
                                    const std::string& ssid,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath,
                                    bool auto_connect) = 0;

  // Connect to the specified cellular network.
  // Returns false if the attempt fails immediately.
  virtual bool ConnectToCellularNetwork(const CellularNetwork* network) = 0;

  // Initiates cellular data plan refresh. Plan data will be passed through
  // Network::Observer::CellularDataPlanChanged callback.
  virtual void RefreshCellularDataPlans(const CellularNetwork* network) = 0;

  // Records information that cellular play payment had happened.
  virtual void SignalCellularPlanPayment() = 0;

  // Returns true if cellular plan payment had been recorded recently.
  virtual bool HasRecentCellularPlanPayment() = 0;

  // Disconnect from the specified wireless (either cellular or wifi) network.
  virtual void DisconnectFromWirelessNetwork(
      const WirelessNetwork* network) = 0;

  // Save network information including passwords (wifi) and auto-connect.
  virtual void SaveCellularNetwork(const CellularNetwork* network) = 0;
  virtual void SaveWifiNetwork(const WifiNetwork* network) = 0;

  // Set the auto-connect property of a network.
  virtual void SetNetworkAutoConnect(const std::string& service_path,
                                     bool auto_connect) = 0;

  // Forget the wifi network corresponding to service_path.
  virtual void ForgetWifiNetwork(const std::string& service_path) = 0;

  virtual bool ethernet_available() const = 0;
  virtual bool wifi_available() const = 0;
  virtual bool cellular_available() const = 0;

  virtual bool ethernet_enabled() const = 0;
  virtual bool wifi_enabled() const = 0;
  virtual bool cellular_enabled() const = 0;

  virtual bool wifi_scanning() const = 0;

  virtual const Network* active_network() const = 0;

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
