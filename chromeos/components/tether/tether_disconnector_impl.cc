// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_disconnector_impl.h"

#include "base/values.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/disconnect_tethering_request_sender.h"
#include "chromeos/components/tether/network_configuration_remover.h"
#include "chromeos/components/tether/pref_names.h"
#include "chromeos/components/tether/tether_connector.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

namespace {

void OnDisconnectError(const std::string& error_name) {
  PA_LOG(WARNING) << "Error disconnecting from Tether network during shutdown; "
                  << "Error name: " << error_name;
}

}  // namespace

// static
void TetherDisconnectorImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDisconnectingWifiNetworkGuid, "");
}

TetherDisconnectorImpl::TetherDisconnectorImpl(
    NetworkConnectionHandler* network_connection_handler,
    NetworkStateHandler* network_state_handler,
    ActiveHost* active_host,
    DisconnectTetheringRequestSender* disconnect_tethering_request_sender,
    NetworkConfigurationRemover* network_configuration_remover,
    TetherConnector* tether_connector,
    DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map,
    PrefService* pref_service)
    : network_connection_handler_(network_connection_handler),
      network_state_handler_(network_state_handler),
      active_host_(active_host),
      disconnect_tethering_request_sender_(disconnect_tethering_request_sender),
      network_configuration_remover_(network_configuration_remover),
      tether_connector_(tether_connector),
      device_id_tether_network_guid_map_(device_id_tether_network_guid_map),
      pref_service_(pref_service),
      weak_ptr_factory_(this) {
  std::string disconnecting_wifi_guid_from_previous_session =
      pref_service_->GetString(prefs::kDisconnectingWifiNetworkGuid);
  if (!disconnecting_wifi_guid_from_previous_session.empty()) {
    // If a previous disconnection attempt was aborted before it could be fully
    // completed, clean up the leftover network configuration.
    network_configuration_remover_->RemoveNetworkConfiguration(
        disconnecting_wifi_guid_from_previous_session);
    pref_service_->ClearPref(prefs::kDisconnectingWifiNetworkGuid);
  }
}

TetherDisconnectorImpl::~TetherDisconnectorImpl() {
  std::string active_tether_guid = active_host_->GetTetherNetworkGuid();
  if (!active_tether_guid.empty()) {
    PA_LOG(INFO) << "There was an active Tether connection during Tether "
                 << "shutdown. Initiating disconnection from network with GUID "
                 << "\"" << active_tether_guid << "\"";
    DisconnectFromNetwork(active_tether_guid, base::Bind(&base::DoNothing),
                          base::Bind(&OnDisconnectError));
  }
}

void TetherDisconnectorImpl::DisconnectFromNetwork(
    const std::string& tether_network_guid,
    const base::Closure& success_callback,
    const network_handler::StringResultCallback& error_callback) {
  DCHECK(!tether_network_guid.empty());

  ActiveHost::ActiveHostStatus status = active_host_->GetActiveHostStatus();
  std::string active_tether_network_guid = active_host_->GetTetherNetworkGuid();
  std::string active_wifi_network_guid = active_host_->GetWifiNetworkGuid();

  if (status == ActiveHost::ActiveHostStatus::DISCONNECTED) {
    PA_LOG(ERROR) << "Disconnect requested for Tether network with GUID "
                  << tether_network_guid << ", but no device is connected.";
    error_callback.Run(NetworkConnectionHandler::kErrorNotConnected);
    return;
  }

  if (tether_network_guid != active_tether_network_guid) {
    PA_LOG(ERROR) << "Disconnect requested for Tether network with GUID "
                  << tether_network_guid << ", but that device is not the "
                  << "active host.";
    error_callback.Run(NetworkConnectionHandler::kErrorNotConnected);
    return;
  }

  if (status == ActiveHost::ActiveHostStatus::CONNECTING) {
    // Note: CancelConnectionAttempt() internally sets the active host to be
    // disconnected.
    if (tether_connector_->CancelConnectionAttempt(tether_network_guid)) {
      PA_LOG(INFO) << "Disconnect requested for Tether network with GUID "
                   << tether_network_guid << ", which had not yet connected. "
                   << "Canceled in-progress connection attempt.";
      success_callback.Run();
      return;
    }

    PA_LOG(ERROR) << "Disconnect requested for Tether network with GUID "
                  << tether_network_guid << " (not yet connected), but "
                  << "canceling connection attempt failed.";
    error_callback.Run(NetworkConnectionHandler::kErrorDisconnectFailed);
    return;
  }

  DCHECK(!active_wifi_network_guid.empty());
  DisconnectActiveWifiConnection(tether_network_guid, active_wifi_network_guid,
                                 success_callback, error_callback);
}

void TetherDisconnectorImpl::DisconnectActiveWifiConnection(
    const std::string& tether_network_guid,
    const std::string& wifi_network_guid,
    const base::Closure& success_callback,
    const network_handler::StringResultCallback& error_callback) {
  // First, disconnect the active host so that the user gets visual indication
  // that the disconnection is in progress as quickly as possible.
  // TODO(hansberry): This will result in the Tether network becoming
  // disconnected, but the Wi-Fi network will still be connected until the
  // DisconnectNetwork() call below completes. This will result in a jarring UI
  // transition which needs to be fixed.
  active_host_->SetActiveHostDisconnected();

  // Before starting disconnection, log the disconnecting Wi-Fi GUID to prefs.
  // Under normal circumstances, the GUID will be cleared as part of
  // CleanUpAfterWifiDisconnection(). However, when the user logs out,
  // this TetherDisconnectorImpl instance will be deleted before one of the
  // callbacks passed below to DisconnectNetwork() can be called, and the
  // GUID will remain in prefs until the next time the user logs in, at which
  // time the associated network configuration can be removed.
  pref_service_->Set(prefs::kDisconnectingWifiNetworkGuid,
                     base::Value(wifi_network_guid));

  const NetworkState* wifi_network_state =
      network_state_handler_->GetNetworkStateFromGuid(wifi_network_guid);
  if (wifi_network_state) {
    network_connection_handler_->DisconnectNetwork(
        wifi_network_state->path(),
        base::Bind(&TetherDisconnectorImpl::OnSuccessfulWifiDisconnect,
                   weak_ptr_factory_.GetWeakPtr(), wifi_network_guid,
                   success_callback, error_callback),
        base::Bind(&TetherDisconnectorImpl::OnFailedWifiDisconnect,
                   weak_ptr_factory_.GetWeakPtr(), wifi_network_guid,
                   success_callback, error_callback));
  } else {
    PA_LOG(ERROR) << "Wi-Fi NetworkState for GUID " << wifi_network_guid << " "
                  << "was not registered. Cannot disconnect.";
    error_callback.Run(NetworkConnectionHandler::kErrorDisconnectFailed);
  }

  // In addition to disconnecting from the Wi-Fi network, this device must also
  // send a DisconnectTetheringRequest to the tether host so that it can shut
  // down its Wi-Fi hotspot if it is no longer in use.
  const std::string device_id =
      device_id_tether_network_guid_map_->GetDeviceIdForTetherNetworkGuid(
          tether_network_guid);
  disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
      device_id);
}

void TetherDisconnectorImpl::OnSuccessfulWifiDisconnect(
    const std::string& wifi_network_guid,
    const base::Closure& success_callback,
    const network_handler::StringResultCallback& error_callback) {
  PA_LOG(INFO) << "Successfully disconnected from Wi-Fi network with GUID "
               << wifi_network_guid << ".";
  CleanUpAfterWifiDisconnection(true /* success */, wifi_network_guid,
                                success_callback, error_callback);
}

void TetherDisconnectorImpl::OnFailedWifiDisconnect(
    const std::string& wifi_network_guid,
    const base::Closure& success_callback,
    const network_handler::StringResultCallback& error_callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  PA_LOG(ERROR) << "Failed to disconnect from Wi-Fi network with GUID "
                << wifi_network_guid << ". Error name: " << error_name;
  CleanUpAfterWifiDisconnection(false /* success */, wifi_network_guid,
                                success_callback, error_callback);
}

void TetherDisconnectorImpl::CleanUpAfterWifiDisconnection(
    bool success,
    const std::string& wifi_network_guid,
    const base::Closure& success_callback,
    const network_handler::StringResultCallback& error_callback) {
  network_configuration_remover_->RemoveNetworkConfiguration(wifi_network_guid);
  pref_service_->ClearPref(prefs::kDisconnectingWifiNetworkGuid);

  if (success)
    success_callback.Run();
  else
    error_callback.Run(NetworkConnectionHandler::kErrorDisconnectFailed);
}

}  // namespace tether

}  // namespace chromeos
