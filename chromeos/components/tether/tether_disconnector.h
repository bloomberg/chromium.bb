// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_DISCONNECTOR_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_DISCONNECTOR_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/tether/disconnect_tethering_operation.h"
#include "chromeos/network/network_handler_callbacks.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class NetworkConnectionHandler;
class NetworkStateHandler;

namespace tether {

class ActiveHost;
class BleConnectionManager;
class DeviceIdTetherNetworkGuidMap;
class NetworkConfigurationRemover;
class TetherConnector;
class TetherHostFetcher;

class TetherDisconnector : public DisconnectTetheringOperation::Observer {
 public:
  TetherDisconnector(
      NetworkConnectionHandler* network_connection_handler,
      NetworkStateHandler* network_state_handler,
      ActiveHost* active_host,
      BleConnectionManager* ble_connection_manager,
      NetworkConfigurationRemover* network_configuration_remover,
      TetherConnector* tether_connector,
      DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map,
      TetherHostFetcher* tether_host_fetcher);
  virtual ~TetherDisconnector();

  virtual void DisconnectFromNetwork(
      const std::string& tether_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback);

  // DisconnectTetheringOperation::Observer:
  void OnOperationFinished(const std::string& device_id, bool success) override;

 private:
  friend class TetherDisconnectorTest;

  void DisconnectActiveWifiConnection(
      const std::string& tether_network_guid,
      const std::string& wifi_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback);
  void OnSuccessfulWifiDisconnect(
      const std::string& wifi_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback);
  void OnFailedWifiDisconnect(
      const std::string& wifi_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);
  void CleanUpAfterWifiDisconnection(
      bool success,
      const std::string& wifi_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback);
  void OnTetherHostFetched(
      const std::string& device_id,
      std::unique_ptr<cryptauth::RemoteDevice> tether_host);

  NetworkConnectionHandler* network_connection_handler_;
  NetworkStateHandler* network_state_handler_;
  ActiveHost* active_host_;
  BleConnectionManager* ble_connection_manager_;
  NetworkConfigurationRemover* network_configuration_remover_;
  TetherConnector* tether_connector_;
  DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map_;
  TetherHostFetcher* tether_host_fetcher_;

  std::unique_ptr<DisconnectTetheringOperation> disconnect_tethering_operation_;
  base::WeakPtrFactory<TetherDisconnector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TetherDisconnector);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_DISCONNECTOR_H_
