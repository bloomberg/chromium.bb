// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/singleton.h"
#include "base/string16.h"
#include "third_party/cros/chromeos_network.h"

struct WifiNetwork {
  WifiNetwork() : encrypted(false), encryption(chromeos::NONE), strength(0) {}
  WifiNetwork(const std::string& ssid, bool encrypted,
              chromeos::EncryptionType encryption, int strength)
      : ssid(ssid),
        encrypted(encrypted),
        encryption(encryption),
        strength(strength) { }

  // WifiNetworks are sorted by ssids.
  bool operator< (const WifiNetwork& other) const {
    return ssid < other.ssid;
  }

  std::string ssid;
  bool encrypted;
  chromeos::EncryptionType encryption;
  int strength;
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
  const std::string& wifi_ssid() const { return wifi_ssid_; }
  bool wifi_connecting() const { return wifi_connecting_; }
  int wifi_strength() const { return wifi_strength_; }

  // Returns the current list of wifi networks.
  WifiNetworkVector GetWifiNetworks();

  // Connect to the specified wireless network with password.
  void ConnectToWifiNetwork(WifiNetwork network, const string16& password);

 private:
  friend struct DefaultSingletonTraits<CrosNetworkLibrary>;

  CrosNetworkLibrary();
  ~CrosNetworkLibrary() {}

  // This method is called when there's a change in network status.
  // This will notify all the Observers.
  static void NetworkStatusChangedHandler(void* object,
      const chromeos::ServiceStatus& service_status);

  // Update the network with the ServiceStatus.
  void UpdateNetworkServiceStatus(
      const chromeos::ServiceStatus& service_status);

  ObserverList<Observer> observers_;

  // Whether or not we are connected to the ethernet line.
  bool ethernet_connected_;

  // The current connected (or connecting) wireless ssid. Empty if none.
  std::string wifi_ssid_;

  // Whether or not we are connecting right now.
  bool wifi_connecting_;

  // The strength of the currently connected ssid.
  int wifi_strength_;

  DISALLOW_COPY_AND_ASSIGN(CrosNetworkLibrary);
};

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NETWORK_LIBRARY_H_
