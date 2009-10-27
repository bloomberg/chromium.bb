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
#include "third_party/cros/chromeos_network.h"

struct WifiNetwork {
  WifiNetwork()
      : encrypted(false),
        encryption(chromeos::NONE),
        strength(0),
        connecting(false),
        connected(false),
        destroyed(false) {}
  WifiNetwork(const std::string& ssid, bool encrypted,
              chromeos::EncryptionType encryption, int strength,
              bool connecting, bool connected)
      : ssid(ssid),
        encrypted(encrypted),
        encryption(encryption),
        strength(strength),
        connecting(connecting),
        connected(connected),
        destroyed(false) {}

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
  bool destroyed;
};
typedef std::vector<WifiNetwork> WifiNetworkVector;

// This class handles the interaction with the ChromeOS network library APIs.
// Classes can add themselves as observers. Users can get an instance of this
// library class like this: CrosNetworkLibrary::Get()
class CrosNetworkLibrary {
 public:
  class Observer {
   public:
    virtual void NetworkChanged(CrosNetworkLibrary* obj) = 0;
  };

  // This gets the singleton CrosNetworkLibrary
  static CrosNetworkLibrary* Get();

  // Returns true if the ChromeOS library was loaded.
  static bool loaded();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool ethernet_connected() const { return ethernet_connected_; }
  const std::string& wifi_ssid() const { return wifi_.ssid; }
  bool wifi_connecting() const { return wifi_.connecting; }
  int wifi_strength() const { return wifi_.strength; }

  // Returns the current list of wifi networks.
  const WifiNetworkVector& wifi_networks() const { return wifi_networks_; }

  // Connect to the specified wireless network with password.
  void ConnectToWifiNetwork(WifiNetwork network, const string16& password);

 private:
  friend struct DefaultSingletonTraits<CrosNetworkLibrary>;

  CrosNetworkLibrary();
  ~CrosNetworkLibrary();

  // This method is called when there's a change in network status.
  // This method is called on a background thread.
  static void NetworkStatusChangedHandler(void* object,
      const chromeos::ServiceStatus& service_status);

  // This parses ServiceStatus and creates a WifiNetworkVector of wifi networks.
  // It also sets ethernet_connected depending on if we have ethernet or not.
  static void ParseNetworks(const chromeos::ServiceStatus& service_status,
                            WifiNetworkVector* networks,
                            bool* ethernet_connected);

  // This methods loads the initial list of networks on startup and starts the
  // monitoring of network changes.
  void Init();

  // Update the network with the a list of wifi networks and ethernet status.
  // This will notify all the Observers.
  void UpdateNetworkStatus(const WifiNetworkVector& networks,
                           bool ethernet_connected);

  ObserverList<Observer> observers_;

  // The network status connection for monitoring network status changes.
  chromeos::NetworkStatusConnection network_status_connection_;

  // Whether or not we are connected to the ethernet line.
  bool ethernet_connected_;

  // The list of available wifi networks.
  WifiNetworkVector wifi_networks_;

  // The current connected (or connecting) wifi network.
  WifiNetwork wifi_;

  DISALLOW_COPY_AND_ASSIGN(CrosNetworkLibrary);
};

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
