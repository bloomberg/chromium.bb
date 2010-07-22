// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/platform_thread.h"
#include "base/singleton.h"
#include "base/string16.h"
#include "base/timer.h"
#include "net/url_request/url_request_job_tracker.h"
#include "cros/chromeos_network.h"

namespace chromeos {

class Network {
 public:
  const std::string& service_path() const { return service_path_; }
  const std::string& device_path() const { return device_path_; }
  const std::string& ip_address() const { return ip_address_; }
  ConnectionType type() const { return type_; }
  bool connecting() const { return state_ == STATE_ASSOCIATION ||
      state_ == STATE_CONFIGURATION || state_ == STATE_CARRIER; }
  bool connected() const { return state_ == STATE_READY; }
  bool connecting_or_connected() const { return connecting() || connected(); }
  bool failed() const { return state_ == STATE_FAILURE; }
  ConnectionError error() const { return error_; }

  void set_connecting(bool connecting) { state_ = (connecting ?
      STATE_ASSOCIATION : STATE_IDLE); }
  void set_connected(bool connected) { state_ = (connected ?
      STATE_READY : STATE_IDLE); }

  // Clear the fields.
  virtual void Clear();

  // Configure the Network from a ServiceInfo object.
  virtual void ConfigureFromService(const ServiceInfo& service);

  // Return a string representation of the state code.
  // This not translated and should be only used for debugging purposes.
  std::string GetStateString();

  // Return a string representation of the error code.
  // This not translated and should be only used for debugging purposes.
  std::string GetErrorString();

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
  int state_;
  ConnectionError error_;
};

class EthernetNetwork : public Network {
 public:
  EthernetNetwork() : Network() {}
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

  void set_name(const std::string& name) { name_ = name; }
  void set_auto_connect(bool auto_connect) { auto_connect_ = auto_connect; }

  // Network overrides.
  virtual void Clear();
  virtual void ConfigureFromService(const ServiceInfo& service);

 protected:
  WirelessNetwork()
      : Network(),
        strength_(0),
        auto_connect_(false) {}

  std::string name_;
  int strength_;
  bool auto_connect_;
};

class CellularNetwork : public WirelessNetwork {
 public:
  CellularNetwork() : WirelessNetwork() {}
  explicit CellularNetwork(const ServiceInfo& service)
      : WirelessNetwork() {
    ConfigureFromService(service);
  }

  // WirelessNetwork overrides.
  virtual void Clear();
  virtual void ConfigureFromService(const ServiceInfo& service);
};

class WifiNetwork : public WirelessNetwork {
 public:
  WifiNetwork()
      : WirelessNetwork(),
        encryption_(SECURITY_NONE) {}
  explicit WifiNetwork(const ServiceInfo& service)
      : WirelessNetwork() {
    ConfigureFromService(service);
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
  virtual void Clear();
  virtual void ConfigureFromService(const ServiceInfo& service);

  // Return a string representation of the encryption code.
  // This not translated and should be only used for debugging purposes.
  std::string GetEncryptionString();

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
  base::Time timestamp; // Timestamp when this cell was primary
  int signal_strength;  // Radio signal strength measured in dBm.
  int timing_advance;   // Represents the distance from the cell tower. Each
                        // unit is roughly 550 meters.
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
  std::string address;
  std::string netmask;
  std::string gateway;
  std::string name_servers;
};
typedef std::vector<NetworkIPConfig> NetworkIPConfigVector;

class NetworkLibrary {
 public:
  class Observer {
   public:
    // A bitfield mask for traffic types.
    enum TrafficTypes {
      TRAFFIC_DOWNLOAD = 0x1,
      TRAFFIC_UPLOAD = 0x2,
    } TrafficTypeMasks;

    // Called when the network has changed. (wifi networks, and ethernet)
    virtual void NetworkChanged(NetworkLibrary* obj) = 0;

    // Called when network traffic has been detected.
    // Takes a bitfield of TrafficTypeMasks.
    virtual void NetworkTraffic(NetworkLibrary* obj, int traffic_type) = 0;
  };

  virtual ~NetworkLibrary() {}
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual const EthernetNetwork& ethernet_network() const = 0;
  virtual bool ethernet_connecting() const = 0;
  virtual bool ethernet_connected() const = 0;

  virtual const std::string& wifi_name() const = 0;
  virtual bool wifi_connecting() const = 0;
  virtual bool wifi_connected() const = 0;
  virtual int wifi_strength() const = 0;

  virtual const std::string& cellular_name() const = 0;
  virtual bool cellular_connecting() const = 0;
  virtual bool cellular_connected() const = 0;
  virtual int cellular_strength() const = 0;

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

  // Attempt to connect to the preferred network if available and it is set up.
  // This call will return true if connection is started.
  // If the preferred network is not available or not setup, returns false.
  // Note: For dogfood purposes, we hardcode the preferred network to Google-A.
  virtual bool ConnectToPreferredNetworkIfAvailable() = 0;

  // Returns true if we are currently connected to the preferred network.
  virtual bool PreferredNetworkConnected() = 0;

  // Returns true if we failed to connect to the preferred network.
  virtual bool PreferredNetworkFailed() = 0;

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

  // Disconnect from the specified wireless (either cellular or wifi) network.
  virtual void DisconnectFromWirelessNetwork(
      const WirelessNetwork& network) = 0;

  // Save network information including passwords (wifi) and auto-connect.
  virtual void SaveCellularNetwork(const CellularNetwork& network) = 0;
  virtual void SaveWifiNetwork(const WifiNetwork& network) = 0;

  // Forget the passed in wireless (either cellular or wifi) network.
  virtual void ForgetWirelessNetwork(const WirelessNetwork& network) = 0;

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

  // Fetches IP configs for a given device_path
  virtual NetworkIPConfigVector GetIPConfigs(
      const std::string& device_path) = 0;

  // Fetches debug network info for display in about:network.
  // The page will have a meta refresh of |refresh| seconds if |refresh| > 0.
  virtual std::string GetHtmlInfo(int refresh) = 0;
};

// This class handles the interaction with the ChromeOS network library APIs.
// Classes can add themselves as observers. Users can get an instance of this
// library class like this: NetworkLibrary::Get()
class NetworkLibraryImpl : public NetworkLibrary,
                           public URLRequestJobTracker::JobObserver {
 public:
  NetworkLibraryImpl();
  virtual ~NetworkLibraryImpl();

  // URLRequestJobTracker::JobObserver methods (called on the IO thread):
  virtual void OnJobAdded(URLRequestJob* job);
  virtual void OnJobRemoved(URLRequestJob* job);
  virtual void OnJobDone(URLRequestJob* job, const URLRequestStatus& status);
  virtual void OnJobRedirect(URLRequestJob* job, const GURL& location,
                             int status_code);
  virtual void OnBytesRead(URLRequestJob* job, const char* buf, int byte_count);

  // NetworkLibrary overrides.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  virtual const EthernetNetwork& ethernet_network() const { return ethernet_; }
  virtual bool ethernet_connecting() const { return ethernet_.connecting(); }
  virtual bool ethernet_connected() const { return ethernet_.connected(); }

  virtual const std::string& wifi_name() const { return wifi_.name(); }
  virtual bool wifi_connecting() const { return wifi_.connecting(); }
  virtual bool wifi_connected() const { return wifi_.connected(); }
  virtual int wifi_strength() const { return wifi_.strength(); }

  virtual const std::string& cellular_name() const { return cellular_.name(); }
  virtual bool cellular_connecting() const { return cellular_.connecting(); }
  virtual bool cellular_connected() const { return cellular_.connected(); }
  virtual int cellular_strength() const { return cellular_.strength(); }

  virtual bool Connected() const;
  virtual bool Connecting() const;
  virtual const std::string& IPAddress() const;

  virtual const WifiNetworkVector& wifi_networks() const {
    return wifi_networks_;
  }

  virtual const WifiNetworkVector& remembered_wifi_networks() const {
    return remembered_wifi_networks_;
  }

  virtual const CellularNetworkVector& cellular_networks() const {
    return cellular_networks_;
  }

  virtual const CellularNetworkVector& remembered_cellular_networks() const {
    return remembered_cellular_networks_;
  }

  virtual bool FindWifiNetworkByPath(const std::string& path,
                                     WifiNetwork* network) const;
  virtual bool FindCellularNetworkByPath(const std::string& path,
                                         CellularNetwork* network) const;

  virtual void RequestWifiScan();
  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result);
  virtual bool ConnectToPreferredNetworkIfAvailable();
  virtual bool PreferredNetworkConnected();
  virtual bool PreferredNetworkFailed();
  virtual void ConnectToWifiNetwork(WifiNetwork network,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath);
  virtual void ConnectToWifiNetwork(const std::string& ssid,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath,
                                    bool auto_connect);
  virtual void ConnectToCellularNetwork(CellularNetwork network);
  virtual void DisconnectFromWirelessNetwork(const WirelessNetwork& network);
  virtual void SaveCellularNetwork(const CellularNetwork& network);
  virtual void SaveWifiNetwork(const WifiNetwork& network);
  virtual void ForgetWirelessNetwork(const WirelessNetwork& network);

  virtual bool ethernet_available() const {
      return available_devices_ & (1 << TYPE_ETHERNET);
  }
  virtual bool wifi_available() const {
      return available_devices_ & (1 << TYPE_WIFI);
  }
  virtual bool cellular_available() const {
      return available_devices_ & (1 << TYPE_CELLULAR);
  }

  virtual bool ethernet_enabled() const {
      return enabled_devices_ & (1 << TYPE_ETHERNET);
  }
  virtual bool wifi_enabled() const {
      return enabled_devices_ & (1 << TYPE_WIFI);
  }
  virtual bool cellular_enabled() const {
      return enabled_devices_ & (1 << TYPE_CELLULAR);
  }

  virtual bool offline_mode() const { return offline_mode_; }

  virtual void EnableEthernetNetworkDevice(bool enable);
  virtual void EnableWifiNetworkDevice(bool enable);
  virtual void EnableCellularNetworkDevice(bool enable);
  virtual void EnableOfflineMode(bool enable);
  virtual NetworkIPConfigVector GetIPConfigs(const std::string& device_path);
  virtual std::string GetHtmlInfo(int refresh);

  virtual void UpdateSystemInfo();

 private:

  // This method is called when there's a change in network status.
  // This method is called on a background thread.
  static void NetworkStatusChangedHandler(void* object);

  // This parses SystemInfo into:
  //  - an EthernetNetwork
  //  - a WifiNetworkVector of wifi networks
  //  - a CellularNetworkVector of cellular networks.
  //  - a WifiNetworkVector of remembered wifi networks
  //  - a CellularNetworkVector of remembered cellular networks.
  static void ParseSystem(SystemInfo* system,
                          EthernetNetwork* ethernet,
                          WifiNetworkVector* wifi_networks,
                          CellularNetworkVector* ceullular_networks,
                          WifiNetworkVector* remembered_wifi_networks,
                          CellularNetworkVector* remembered_ceullular_networks);

  // This methods loads the initial list of networks on startup and starts the
  // monitoring of network changes.
  void Init();

  // Returns the preferred wifi network.
  WifiNetwork* GetPreferredNetwork();

  // Gets the WifiNetwork with the given name. Returns NULL if not found.
  // Only used by GetPreferredNetwork() to lookup "Google" and "GoogleA" (hack)
  WifiNetwork* GetWifiNetworkByName(const std::string& name);

  // Gets the WirelessNetwork (WifiNetwork or CellularNetwork) by path
  template<typename T>
  T* GetWirelessNetworkByPath(std::vector<T>& networks,
                              const std::string& path);
  template<typename T>
  const T* GetWirelessNetworkByPath(const std::vector<T>& networks,
                                    const std::string& path) const;

  // Enables/disables the specified network device.
  void EnableNetworkDeviceType(ConnectionType device, bool enable);

  // Update the cached network status.
  // This will notify all the Observers.
  void UpdateNetworkStatus();

  // Checks network traffic to see if there is any uploading.
  // If there is download traffic, then true is passed in for download.
  // If there is network traffic then start timer that invokes
  // NetworkTrafficTimerFired.
  void CheckNetworkTraffic(bool download);

  // Called when the timer fires and we need to send out NetworkTraffic
  // notifications.
  void NetworkTrafficTimerFired();

  // This is a helper method to notify the observers on the UI thread.
  void NotifyNetworkTraffic(int traffic_type);

  // This will notify all obeservers on the UI thread.
  void NotifyObservers();

  ObserverList<Observer> observers_;

  // The amount of time to wait between each NetworkTraffic notifications.
  static const int kNetworkTrafficeTimerSecs;

  // Timer for sending NetworkTraffic notification every
  // kNetworkTrafficeTimerSecs seconds.
  base::OneShotTimer<NetworkLibraryImpl> timer_;

  // The current traffic type that will be sent out for the next NetworkTraffic
  // notification. This is a bitfield of TrafficTypeMasks.
  int traffic_type_;

  // The network status connection for monitoring network status changes.
  MonitorNetworkConnection network_status_connection_;

  // The ethernet network.
  EthernetNetwork ethernet_;

  // The list of available wifi networks.
  WifiNetworkVector wifi_networks_;

  // The current connected (or connecting) wifi network.
  WifiNetwork wifi_;

  // The remembered wifi networks.
  WifiNetworkVector remembered_wifi_networks_;

  // The list of available cellular networks.
  CellularNetworkVector cellular_networks_;

  // The current connected (or connecting) cellular network.
  CellularNetwork cellular_;

  // The remembered cellular networks.
  CellularNetworkVector remembered_cellular_networks_;

  // The current available network devices. Bitwise flag of ConnectionTypes.
  int available_devices_;

  // The current enabled network devices. Bitwise flag of ConnectionTypes.
  int enabled_devices_;

  // The current connected network devices. Bitwise flag of ConnectionTypes.
  int connected_devices_;

  bool offline_mode_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
