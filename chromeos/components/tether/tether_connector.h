// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_CONNECTOR_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_CONNECTOR_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/tether/connect_tethering_operation.h"
#include "chromeos/network/network_connect.h"

namespace chromeos {

class NetworkStateHandler;

namespace tether {

class ActiveHost;
class BleConnectionManager;
class DeviceIdTetherNetworkGuidMap;
class HostScanDevicePrioritizer;
class TetherHostFetcher;
class WifiHotspotConnector;

// Connects to a tether network. When the user initiates a connection via the
// UI, TetherConnector receives a callback from NetworkConnect and initiates a
// connection by starting a ConnectTetheringOperation. When a response has been
// received from the tether host, TetherConnector connects to the associated
// Wi-Fi network.
class TetherConnector : public NetworkConnect::TetherDelegate,
                        public ConnectTetheringOperation::Observer {
 public:
  TetherConnector(
      NetworkConnect* network_connect,
      NetworkStateHandler* network_state_handler,
      WifiHotspotConnector* wifi_hotspot_connector,
      ActiveHost* active_host,
      TetherHostFetcher* tether_host_fetcher,
      BleConnectionManager* connection_manager,
      HostScanDevicePrioritizer* host_scan_device_prioritizer,
      DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map);
  ~TetherConnector() override;

  // NetworkConnect::TetherDelegate:
  void ConnectToNetwork(const std::string& guid) override;

  // ConnectTetheringOperation::Observer:
  void OnSuccessfulConnectTetheringResponse(
      const cryptauth::RemoteDevice& remote_device,
      const std::string& ssid,
      const std::string& password) override;
  void OnConnectTetheringFailure(
      const cryptauth::RemoteDevice& remote_device,
      ConnectTetheringResponse_ResponseCode error_code) override;

 private:
  friend class TetherConnectorTest;

  void SetDisconnected();
  void SetConnected(const std::string& device_id,
                    const std::string& wifi_network_guid);

  void OnTetherHostToConnectFetched(
      const std::string& device_id,
      std::unique_ptr<cryptauth::RemoteDevice> tether_host_to_connect);
  void OnWifiConnection(const std::string& device_id,
                        const std::string& wifi_network_guid);

  NetworkConnect* network_connect_;
  NetworkStateHandler* network_state_handler_;
  WifiHotspotConnector* wifi_hotspot_connector_;
  ActiveHost* active_host_;
  TetherHostFetcher* tether_host_fetcher_;
  BleConnectionManager* connection_manager_;
  HostScanDevicePrioritizer* host_scan_device_prioritizer_;
  DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map_;

  std::string device_id_pending_connection_;
  std::unique_ptr<ConnectTetheringOperation> connect_tethering_operation_;
  base::WeakPtrFactory<TetherConnector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TetherConnector);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_CONNECTOR_H_
