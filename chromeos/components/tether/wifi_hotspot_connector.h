// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_WIFI_HOTSPOT_CONNECTOR_H_
#define CHROMEOS_COMPONENTS_TETHER_WIFI_HOTSPOT_CONNECTOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkConnect;
class NetworkState;
class NetworkStateHandler;

namespace tether {

// Connects to a Wi-Fi hotspot, given an SSID and password.
class WifiHotspotConnector : public NetworkStateHandlerObserver {
 public:
  WifiHotspotConnector(NetworkStateHandler* network_state_handler,
                       NetworkConnect* network_connect);
  ~WifiHotspotConnector() override;

  // Function which receives the GUID of the connected Wi-Fi hotspot. If
  // the string passed is empty, an error occurred trying to connect.
  using WifiConnectionCallback = base::Callback<void(const std::string&)>;

  // Connects to the Wi-Fi network with SSID |ssid| and password |password|,
  // invoking |callback| when the connection succeeds, fails, or times out.
  // Note: If ConnectToWifiHotspot() is called while another connection attempt
  // is in progress, the previous attempt will be canceled and the new attempt
  // will begin.
  virtual void ConnectToWifiHotspot(const std::string& ssid,
                                    const std::string& password,
                                    const std::string& tether_network_guid,
                                    const WifiConnectionCallback& callback);

  // NetworkStateHandlerObserver:
  void NetworkPropertiesUpdated(const NetworkState* network) override;

 private:
  friend class WifiHotspotConnectorTest;

  static const int64_t kConnectionTimeoutSeconds = 20;

  // Passes an empty string as |wifi_guid| to signal that the connection did not
  // succeed.
  void InvokeWifiConnectionCallback(const std::string& wifi_guid);
  base::DictionaryValue CreateWifiPropertyDictionary(
      const std::string& ssid,
      const std::string& password);
  void OnConnectionTimeout();

  void SetTimerForTest(std::unique_ptr<base::Timer> timer);

  NetworkStateHandler* network_state_handler_;
  NetworkConnect* network_connect_;
  std::unique_ptr<base::Timer> timer_;

  std::string ssid_;
  std::string password_;
  std::string tether_network_guid_;
  std::string wifi_network_guid_;
  WifiConnectionCallback callback_;
  bool has_initiated_connection_to_current_network_ = false;

  base::WeakPtrFactory<WifiHotspotConnector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WifiHotspotConnector);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_WIFI_HOTSPOT_CONNECTOR_H_
