// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_WIFI_MANAGER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_WIFI_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "components/wifi/network_properties.h"

namespace local_discovery {

namespace wifi {

// Convenience definition for users of this header, since ::wifi and
// local_discovery::wifi may conflict.
using ::wifi::NetworkProperties;

typedef std::vector<NetworkProperties> NetworkPropertiesList;

// Credentials for WiFi networks. Currently only supports PSK-based networks.
// TODO(noamsml): Support for 802.11X and other authentication methods.
struct WifiCredentials {
  static WifiCredentials FromPSK(const std::string& psk);

  std::string psk;
};

class WifiManagerFactory;

// Observer for the network list. Classes may implement this interface and call
// |AddNetworkListObserver| to be notified of changes to the visible network
// list.
class NetworkListObserver {
 public:
  virtual ~NetworkListObserver() {}

  virtual void OnNetworkListChanged(const NetworkPropertiesList& ssid) = 0;
};

// A class to manage listing, connecting to, and getting the credentials of WiFi
// networks.
class WifiManager {
 public:
  typedef base::Callback<void(const NetworkPropertiesList& ssids)>
      SSIDListCallback;
  typedef base::Callback<void(bool success)> SuccessCallback;
  typedef base::Callback<
      void(bool success, const std::string& ssid, const std::string& password)>
      CredentialsCallback;

  virtual ~WifiManager() {}

  static scoped_ptr<WifiManager> Create();

  static void SetFactory(WifiManagerFactory* factory);

  // Start the wifi manager. This must be called before any other method calls.
  virtual void Start() = 0;

  // Get the list of visible SSIDs in the vicinity. This does not initiate a
  // scan, but merely gets the list of networks from the system.
  virtual void GetSSIDList(const SSIDListCallback& callback) = 0;

  // Request a scan for networks nearby.
  virtual void RequestScan() = 0;

  // Configure and connect to a network with a given SSID and
  // credentials. |callback| will be called once the network is connected or
  // after it has failed to connect.
  virtual void ConfigureAndConnectNetwork(const std::string& ssid,
                                          const WifiCredentials& credentials,
                                          const SuccessCallback& callback) = 0;

  // Connect to a configured network with a given network ID. |callback| will be
  // called once the network is connected or after it has failed to connect.
  virtual void ConnectToNetworkByID(const std::string& ssid,
                                    const SuccessCallback& callback) = 0;

  // Reequest the credentials for a network with a given network ID from the
  // system. |callback| will be called with credentials if they can be
  // retrieved. Depending on platform, this may bring up a confirmation dialog
  // or password prompt.
  virtual void RequestNetworkCredentials(
      const std::string& internal_id,
      const CredentialsCallback& callback) = 0;

  // Add a network list observer. This observer will be notified every time the
  // network list changes.
  virtual void AddNetworkListObserver(NetworkListObserver* observer) = 0;

  // Remove a network list observer.
  virtual void RemoveNetworkListObserver(NetworkListObserver* observer) = 0;

 private:
  static scoped_ptr<WifiManager> CreateDefault();
};

class WifiManagerFactory {
 public:
  virtual ~WifiManagerFactory() {}

  virtual scoped_ptr<WifiManager> CreateWifiManager() = 0;
};

}  // namespace wifi

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_WIFI_MANAGER_H_
