// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_ACTIVE_HOST_H_
#define CHROMEOS_COMPONENTS_TETHER_ACTIVE_HOST_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

class PrefRegistrySimple;
class PrefService;

namespace cryptauth {
struct RemoteDevice;
}  // namespace cryptauth

namespace chromeos {

namespace tether {

class TetherHostFetcher;

// ActiveHost tracks metadata about the current connection to a tether host.
// This data is persisted to user preferences.
class ActiveHost {
 public:
  // Enumeration used to describe the state of the active connection to a tether
  // host.
  enum class ActiveHostStatus {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2
  };

  ActiveHost(TetherHostFetcher* tether_host_fetcher, PrefService* pref_service);
  virtual ~ActiveHost();

  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Sets the active host to be no host at all (i.e., the local device is not
  // connecting or connected to a tether host).
  void SetActiveHostDisconnected();

  // Sets the active host to be the device with ID |active_host_device_id| and
  // records that the there is an active attempt to connect to that host (i.e.,
  // the host is not yet connected but it is in the process of connecting).
  void SetActiveHostConnecting(const std::string& active_host_device_id);

  // Sets the active host to be the device with ID |active_host_device_id| and
  // that the local device is connected to that device on the mobile hotspot
  // with Wi-Fi network ID |wifi_network_id|.
  void SetActiveHostConnected(const std::string& active_host_device_id,
                              const std::string& wifi_network_id);

  // Gets the active host and associated metadata asynchronously. If
  // the active host status is...
  //     DISCONNECTED: The callback's |active_host| parameter will be nullptr
  //                   and |wifi_network_id| parameter will be "".
  //     CONNECTING: The callback's |wifi_network_id| will be "".
  //     CONNECTED: All three parameter  will be present.
  using ActiveHostCallback =
      base::Callback<void(ActiveHostStatus active_host_status,
                          std::unique_ptr<cryptauth::RemoteDevice> active_host,
                          const std::string& wifi_network_id)>;
  void GetActiveHost(const ActiveHostCallback& active_host_callback);

  // Synchronous getter methods which do not return a full RemoteDevice object.
  ActiveHostStatus GetActiveHostStatus() const;
  const std::string GetActiveHostDeviceId() const;
  const std::string GetWifiNetworkId() const;

 private:
  void SetActiveHost(ActiveHostStatus active_host_status,
                     const std::string& active_host_device_id,
                     const std::string& wifi_network_id);

  void OnTetherHostFetched(
      const ActiveHostCallback& active_host_callback,
      std::unique_ptr<cryptauth::RemoteDevice> remote_device);

  TetherHostFetcher* tether_host_fetcher_;
  PrefService* pref_service_;

  base::WeakPtrFactory<ActiveHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ActiveHost);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_ACTIVE_HOST_H_
