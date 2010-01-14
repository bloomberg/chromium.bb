// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NETWORK_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_NETWORK_LIBRARY_H_

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
  EthernetNetwork(bool connecting, bool connected)
      : connecting(connecting),
        connected(connected) {}

  bool connecting;
  bool connected;
};

struct WifiNetwork {
  WifiNetwork()
      : encrypted(false),
        encryption(chromeos::NONE),
        strength(0),
        connecting(false),
        connected(false) {}
  WifiNetwork(const std::string& ssid, bool encrypted,
              chromeos::EncryptionType encryption, int strength,
              bool connecting, bool connected)
      : ssid(ssid),
        encrypted(encrypted),
        encryption(encryption),
        strength(strength),
        connecting(connecting),
        connected(connected) {}

  // WifiNetworks are sorted by ssids.
  bool operator< (const WifiNetwork& other) const {
    return ssid < other.ssid;
  }

  std::string ssid;
  bool encrypted;
  chromeos::EncryptionType encryption;
  int strength;
  bool connecting;
  bool connected;
};
typedef std::vector<WifiNetwork> WifiNetworkVector;

struct CellularNetwork {
  CellularNetwork()
      : strength(strength),
        connecting(false),
        connected(false) {}
  CellularNetwork(const std::string& name, int strength, bool connecting,
                  bool connected)
      : name(name),
        strength(strength),
        connecting(connecting),
        connected(connected) {}

  // CellularNetworks are sorted by name.
  bool operator< (const CellularNetwork& other) const {
    return name < other.name;
  }

  std::string name;
  int strength;
  bool connecting;
  bool connected;
};
typedef std::vector<CellularNetwork> CellularNetworkVector;

// This class handles the interaction with the ChromeOS network library APIs.
// Classes can add themselves as observers. Users can get an instance of this
// library class like this: NetworkLibrary::Get()
class NetworkLibrary : public URLRequestJobTracker::JobObserver {
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

  // This gets the singleton NetworkLibrary
  static NetworkLibrary* Get();

  // Makes sure the library is loaded, loading it if necessary. Returns true if
  // the library has been successfully loaded.
  static bool EnsureLoaded();

  // URLRequestJobTracker::JobObserver methods (called on the IO thread):
  virtual void OnJobAdded(URLRequestJob* job);
  virtual void OnJobRemoved(URLRequestJob* job);
  virtual void OnJobDone(URLRequestJob* job, const URLRequestStatus& status);
  virtual void OnJobRedirect(URLRequestJob* job, const GURL& location,
                             int status_code);
  virtual void OnBytesRead(URLRequestJob* job, int byte_count);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool ethernet_connecting() const { return ethernet_.connecting; }
  bool ethernet_connected() const { return ethernet_.connected; }
  const std::string& wifi_ssid() const { return wifi_.ssid; }
  bool wifi_connecting() const { return wifi_.connecting; }
  bool wifi_connected() const { return wifi_.connected; }
  int wifi_strength() const { return wifi_.strength; }
  const std::string& cellular_name() const { return cellular_.name; }
  bool cellular_connecting() const { return cellular_.connecting; }
  bool cellular_connected() const { return cellular_.connected; }
  int cellular_strength() const { return cellular_.strength; }

  // Returns the current list of wifi networks.
  const WifiNetworkVector& wifi_networks() const { return wifi_networks_; }

  // Returns the current list of cellular networks.
  const CellularNetworkVector& cellular_networks() const {
    return cellular_networks_;
  }

  // Connect to the specified wireless network with password.
  void ConnectToWifiNetwork(WifiNetwork network, const string16& password);

  // Connect to the specified cellular network.
  void ConnectToCellularNetwork(CellularNetwork network);

  bool ethernet_enabled() const { return network_devices_ & TYPE_ETHERNET; }
  bool wifi_enabled() const { return network_devices_ & TYPE_WIFI; }
  bool cellular_enabled() const { return network_devices_ & TYPE_CELLULAR; }
  bool offline_mode() const { return offline_mode_; }

  // Enables/disables the ethernet network device.
  void EnableEthernetNetworkDevice(bool enable);

  // Enables/disables the wifi network device.
  void EnableWifiNetworkDevice(bool enable);

  // Enables/disables the cellular network device.
  void EnableCellularNetworkDevice(bool enable);

  // Enables/disables offline mode.
  void EnableOfflineMode(bool enable);

 private:
  friend struct DefaultSingletonTraits<NetworkLibrary>;

  NetworkLibrary();
  ~NetworkLibrary();

  // This method is called when there's a change in network status.
  // This method is called on a background thread.
  static void NetworkStatusChangedHandler(void* object,
      const chromeos::ServiceStatus& service_status);

  // This parses ServiceStatus and creates a WifiNetworkVector of wifi networks
  // and a CellularNetworkVector of cellular networks.
  // It also sets the ethernet connecting/connected status.
  static void ParseNetworks(const chromeos::ServiceStatus& service_status,
                            EthernetNetwork* ethernet,
                            WifiNetworkVector* wifi_networks,
                            CellularNetworkVector* ceullular_networks);

  // This methods loads the initial list of networks on startup and starts the
  // monitoring of network changes.
  void Init();

  // Enables/disables the specified network device.
  void EnableNetworkDevice(chromeos::ConnectionType device, bool enable);

  // Update the network with the ethernet status and a list of wifi and cellular
  // networks.
  // This will notify all the Observers.
  void UpdateNetworkStatus(const EthernetNetwork& ethernet,
                           const WifiNetworkVector& wifi_networks,
                           const CellularNetworkVector& cellular_networks);

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
  base::OneShotTimer<NetworkLibrary> timer_;

  // The current traffic type that will be sent out for the next NetworkTraffic
  // notification. This is a bitfield of TrafficTypeMasks.
  int traffic_type_;

  // The network status connection for monitoring network status changes.
  chromeos::NetworkStatusConnection network_status_connection_;

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

  // The current enabled network devices. This is a bitwise flag of
  // ConnectionTypes.
  int network_devices_;

  bool offline_mode_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibrary);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NETWORK_LIBRARY_H_
