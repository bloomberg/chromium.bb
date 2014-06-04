// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_BOOTSTRAPPING_DEVICE_LISTER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_BOOTSTRAPPING_DEVICE_LISTER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/local_discovery/wifi/wifi_manager.h"

namespace local_discovery {

namespace wifi {

struct BootstrappingDeviceDescription {
  enum ConnectionStatus {
    ONLINE,
    OFFLINE,
    CONNECTING,
    NOT_CONFIGURED,
    LOCAL_ONLY
  };

  BootstrappingDeviceDescription();
  ~BootstrappingDeviceDescription();

  std::string device_network_id;
  std::string device_ssid;
  std::string device_name;
  std::string device_kind;
  ConnectionStatus connection_status;
};

class BootstrappingDeviceLister : public NetworkListObserver {
 public:
  typedef base::Callback<
      void(bool available, const BootstrappingDeviceDescription& description)>
      UpdateCallback;

  BootstrappingDeviceLister(WifiManager* wifi_manager,
                            const UpdateCallback& update_callback);
  virtual ~BootstrappingDeviceLister();

  void Start();

 private:
  typedef std::vector<
      std::pair<std::string /*ssid*/, std::string /*internal_name*/> >
      ActiveDeviceList;

  virtual void OnNetworkListChanged(
      const std::vector<NetworkProperties>& ssids) OVERRIDE;

  void UpdateChangedSSIDs(bool available,
                          const ActiveDeviceList& changed,
                          const ActiveDeviceList& original);

  WifiManager* wifi_manager_;
  UpdateCallback update_callback_;

  bool started_;
  ActiveDeviceList active_devices_;
  base::WeakPtrFactory<BootstrappingDeviceLister> weak_factory_;
};

}  // namespace wifi

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_BOOTSTRAPPING_DEVICE_LISTER_H_
