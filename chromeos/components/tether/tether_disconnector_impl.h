// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_DISCONNECTOR_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_DISCONNECTOR_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/tether/tether_disconnector.h"
#include "chromeos/network/network_handler_callbacks.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class DictionaryValue;
}

namespace chromeos {

class NetworkConnectionHandler;
class NetworkStateHandler;

namespace tether {

class ActiveHost;
class DeviceIdTetherNetworkGuidMap;
class DisconnectTetheringRequestSender;
class NetworkConfigurationRemover;
class TetherConnector;

class TetherDisconnectorImpl : public TetherDisconnector {
 public:
  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  TetherDisconnectorImpl(
      NetworkConnectionHandler* network_connection_handler,
      NetworkStateHandler* network_state_handler,
      ActiveHost* active_host,
      DisconnectTetheringRequestSender* disconnect_tethering_request_sender,
      NetworkConfigurationRemover* network_configuration_remover,
      TetherConnector* tether_connector,
      DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map,
      PrefService* pref_service);
  ~TetherDisconnectorImpl() override;

  void DisconnectFromNetwork(
      const std::string& tether_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback) override;

 private:
  friend class TetherDisconnectorImplTest;

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

  NetworkConnectionHandler* network_connection_handler_;
  NetworkStateHandler* network_state_handler_;
  ActiveHost* active_host_;
  DisconnectTetheringRequestSender* disconnect_tethering_request_sender_;
  NetworkConfigurationRemover* network_configuration_remover_;
  TetherConnector* tether_connector_;
  DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map_;
  PrefService* pref_service_;

  base::WeakPtrFactory<TetherDisconnectorImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TetherDisconnectorImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_DISCONNECTOR_IMPL_H_
