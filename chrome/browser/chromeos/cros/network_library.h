// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
#include "third_party/cros/chromeos_network.h"

namespace chromeos {

struct EthernetNetwork {
  EthernetNetwork()
      : connecting(false),
        connected(false) {}

  std::string device_path;
  std::string ip_address;
  bool connecting;
  bool connected;
};

struct WifiNetwork {
  WifiNetwork()
      : encrypted(false),
        encryption(SECURITY_UNKNOWN),
        strength(0),
        connecting(false),
        connected(false),
        auto_connect(false) {}
  WifiNetwork(ServiceInfo service, bool connecting, bool connected,
              const std::string& ip_address)
      : service_path(service.service_path),
        device_path(service.device_path),
        ssid(service.name),
        encrypted(service.security != SECURITY_NONE),
        encryption(service.security),
        passphrase(service.passphrase),
        strength(service.strength),
        connecting(connecting),
        connected(connected),
        ip_address(ip_address) {}

  // WifiNetworks are sorted by ssids.
  bool operator< (const WifiNetwork& other) const {
    return ssid < other.ssid;
  }

  std::string service_path;
  std::string device_path;
  std::string ssid;
  bool encrypted;
  ConnectionSecurity encryption;
  std::string passphrase;
  int strength;
  bool connecting;
  bool connected;
  bool auto_connect;
  std::string ip_address;
};
typedef std::vector<WifiNetwork> WifiNetworkVector;

struct CellularNetwork {
  CellularNetwork()
      : strength(strength),
        connecting(false),
        connected(false),
        auto_connect(false) {}
  CellularNetwork(ServiceInfo service, bool connecting, bool connected,
                  const std::string& ip_address)
      : service_path(service.service_path),
        device_path(service.device_path),
        name(service.name),
        strength(service.strength),
        connecting(connecting),
        connected(connected),
        ip_address(ip_address) {}

  // CellularNetworks are sorted by name.
  bool operator< (const CellularNetwork& other) const {
    return name < other.name;
  }

  std::string service_path;
  std::string device_path;
  std::string name;
  int strength;
  bool connecting;
  bool connected;
  bool auto_connect;
  std::string ip_address;
};
typedef std::vector<CellularNetwork> CellularNetworkVector;

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

  virtual const std::string& wifi_ssid() const = 0;
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

  // Returns the current list of cellular networks.
  virtual const CellularNetworkVector& cellular_networks() const = 0;

  // Request a scan for new wifi networks.
  virtual void RequestWifiScan() = 0;

  // Connect to the specified wireless network with password.
  virtual void ConnectToWifiNetwork(WifiNetwork network,
                                    const string16& password) = 0;

  // Connect to the specified wifi ssid with password.
  virtual void ConnectToWifiNetwork(const string16& ssid,
                                    const string16& password) = 0;

  // Connect to the specified cellular network.
  virtual void ConnectToCellularNetwork(CellularNetwork network) = 0;

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
  virtual void OnBytesRead(URLRequestJob* job, int byte_count);

  // NetworkLibrary overrides.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  virtual const EthernetNetwork& ethernet_network() const { return ethernet_; }
  virtual bool ethernet_connecting() const { return ethernet_.connecting; }
  virtual bool ethernet_connected() const { return ethernet_.connected; }

  virtual const std::string& wifi_ssid() const { return wifi_.ssid; }
  virtual bool wifi_connecting() const { return wifi_.connecting; }
  virtual bool wifi_connected() const { return wifi_.connected; }
  virtual int wifi_strength() const { return wifi_.strength; }

  virtual const std::string& cellular_name() const { return cellular_.name; }
  virtual bool cellular_connecting() const { return cellular_.connecting; }
  virtual bool cellular_connected() const { return cellular_.connected; }
  virtual int cellular_strength() const { return cellular_.strength; }

  // Return true if any network is currently connected.
  virtual bool Connected() const;

  // Return true if any network is currently connecting.
  virtual bool Connecting() const;

  // Returns the current IP address if connected. If not, returns empty string.
  virtual const std::string& IPAddress() const;

  // Returns the current list of wifi networks.
  virtual const WifiNetworkVector& wifi_networks() const {
    return wifi_networks_;
  }

  // Returns the current list of cellular networks.
  virtual const CellularNetworkVector& cellular_networks() const {
    return cellular_networks_;
  }

  // Request a scan for new wifi networks.
  virtual void RequestWifiScan();

  // Connect to the specified wireless network with password.
  virtual void ConnectToWifiNetwork(WifiNetwork network,
                                    const string16& password);

  // Connect to the specified wifi ssid with password.
  virtual void ConnectToWifiNetwork(const string16& ssid,
                                    const string16& password);

  // Connect to the specified cellular network.
  virtual void ConnectToCellularNetwork(CellularNetwork network);

  virtual bool ethernet_available() const {
      return available_devices_ & (1 << TYPE_ETHERNET); }
  virtual bool wifi_available() const {
      return available_devices_ & (1 << TYPE_WIFI); }
  virtual bool cellular_available() const {
      return available_devices_ & (1 << TYPE_CELLULAR); }

  virtual bool ethernet_enabled() const {
      return enabled_devices_ & (1 << TYPE_ETHERNET); }
  virtual bool wifi_enabled() const {
      return enabled_devices_ & (1 << TYPE_WIFI); }
  virtual bool cellular_enabled() const {
      return enabled_devices_ & (1 << TYPE_CELLULAR); }

  virtual bool offline_mode() const { return offline_mode_; }

  // Enables/disables the ethernet network device.
  virtual void EnableEthernetNetworkDevice(bool enable);

  // Enables/disables the wifi network device.
  virtual void EnableWifiNetworkDevice(bool enable);

  // Enables/disables the cellular network device.
  virtual void EnableCellularNetworkDevice(bool enable);

  // Enables/disables offline mode.
  virtual void EnableOfflineMode(bool enable);

  // Fetches IP configs for a given device_path
  virtual NetworkIPConfigVector GetIPConfigs(const std::string& device_path);

 private:

  // This method is called when there's a change in network status.
  // This method is called on a background thread.
  static void NetworkStatusChangedHandler(void* object);

  // This parses SystemInfo and creates a WifiNetworkVector of wifi networks
  // and a CellularNetworkVector of cellular networks.
  // It also sets the ethernet connecting/connected status.
  static void ParseSystem(SystemInfo* system,
                          EthernetNetwork* ethernet,
                          WifiNetworkVector* wifi_networks,
                          CellularNetworkVector* ceullular_networks);

  // This methods loads the initial list of networks on startup and starts the
  // monitoring of network changes.
  void Init();

  // Enables/disables the specified network device.
  void EnableNetworkDevice(ConnectionType device, bool enable);

  // Update the network with the SystemInfo object.
  // This will notify all the Observers.
  void UpdateNetworkStatus(SystemInfo* system);

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

  // The list of available cellular networks.
  CellularNetworkVector cellular_networks_;

  // The current connected (or connecting) cellular network.
  CellularNetwork cellular_;

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
