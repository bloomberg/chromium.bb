// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_connector.h"

#include "base/bind.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/components/tether/wifi_hotspot_connector.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

TetherConnector::TetherConnector(
    NetworkStateHandler* network_state_handler,
    WifiHotspotConnector* wifi_hotspot_connector,
    ActiveHost* active_host,
    TetherHostFetcher* tether_host_fetcher,
    BleConnectionManager* connection_manager,
    TetherHostResponseRecorder* tether_host_response_recorder,
    DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map)
    : network_state_handler_(network_state_handler),
      wifi_hotspot_connector_(wifi_hotspot_connector),
      active_host_(active_host),
      tether_host_fetcher_(tether_host_fetcher),
      connection_manager_(connection_manager),
      tether_host_response_recorder_(tether_host_response_recorder),
      device_id_tether_network_guid_map_(device_id_tether_network_guid_map),
      weak_ptr_factory_(this) {}

TetherConnector::~TetherConnector() {
  if (connect_tethering_operation_) {
    connect_tethering_operation_->RemoveObserver(this);
  }
}

void TetherConnector::ConnectToNetwork(
    const std::string& tether_network_guid,
    const base::Closure& success_callback,
    const network_handler::StringResultCallback& error_callback) {
  DCHECK(!tether_network_guid.empty());
  DCHECK(!success_callback.is_null());
  DCHECK(!error_callback.is_null());

  PA_LOG(INFO) << "Attempting to connect to network with GUID "
               << tether_network_guid << ".";

  const std::string device_id =
      device_id_tether_network_guid_map_->GetDeviceIdForTetherNetworkGuid(
          tether_network_guid);

  // If NetworkConnectionHandler receives a connection request for a network
  // to which it is already attempting a connection, it should stop the
  // duplicate connection request itself before invoking its TetherDelegate.
  // Thus, ConnectToNetwork() should never be called for a device which is
  // already pending connection.
  DCHECK(device_id_pending_connection_ != device_id);

  if (!device_id_pending_connection_.empty()) {
    PA_LOG(INFO) << "A connection attempt was already in progress to device "
                 << "with ID " << device_id_pending_connection_ << ". "
                 << "Canceling that connection attempt before continuing.";
    CancelConnectionAttempt(
        device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
            device_id_pending_connection_));
  }

  device_id_pending_connection_ = device_id;
  success_callback_ = success_callback;
  error_callback_ = error_callback;
  active_host_->SetActiveHostConnecting(device_id, tether_network_guid);

  tether_host_fetcher_->FetchTetherHost(
      device_id_pending_connection_,
      base::Bind(&TetherConnector::OnTetherHostToConnectFetched,
                 weak_ptr_factory_.GetWeakPtr(),
                 device_id_pending_connection_));
}

bool TetherConnector::CancelConnectionAttempt(
    const std::string& tether_network_guid) {
  const std::string device_id =
      device_id_tether_network_guid_map_->GetDeviceIdForTetherNetworkGuid(
          tether_network_guid);

  if (device_id != device_id_pending_connection_) {
    PA_LOG(ERROR) << "CancelConnectionAttempt(): Cancel requested for Tether "
                  << "network with GUID " << tether_network_guid << ", but "
                  << "there was no active connection to that network.";
    return false;
  }

  PA_LOG(INFO) << "Canceling connection attempt to Tether network with GUID "
               << tether_network_guid;

  if (connect_tethering_operation_) {
    // If a ConnectTetheringOperation is in progress, stop it.
    connect_tethering_operation_->RemoveObserver(this);
    connect_tethering_operation_.reset();
  }

  SetConnectionFailed(NetworkConnectionHandler::kErrorConnectCanceled);
  return true;
}

void TetherConnector::OnSuccessfulConnectTetheringResponse(
    const cryptauth::RemoteDevice& remote_device,
    const std::string& ssid,
    const std::string& password) {
  if (device_id_pending_connection_ != remote_device.GetDeviceId()) {
    // If the success was part of a previous attempt for a different device,
    // ignore it.
    PA_LOG(INFO) << "Received successful ConnectTetheringResponse from "
                 << "device with ID "
                 << remote_device.GetTruncatedDeviceIdForLogs()
                 << ", but a connection to another device was started while "
                 << "the response was being received.";
    return;
  }

  PA_LOG(INFO) << "Received successful ConnectTetheringResponse from device "
               << "with ID " << remote_device.GetTruncatedDeviceIdForLogs()
               << ". SSID: \"" << ssid << "\", Password: \"" << password
               << "\"";

  // Make a copy of the device ID, SSID, and password to pass below before
  // destroying |connect_tethering_operation_|.
  std::string remote_device_id = remote_device.GetDeviceId();
  std::string ssid_copy = ssid;
  std::string password_copy = password;

  connect_tethering_operation_->RemoveObserver(this);
  connect_tethering_operation_.reset();

  wifi_hotspot_connector_->ConnectToWifiHotspot(
      ssid_copy, password_copy, active_host_->GetTetherNetworkGuid(),
      base::Bind(&TetherConnector::OnWifiConnection,
                 weak_ptr_factory_.GetWeakPtr(), remote_device_id));
}

void TetherConnector::OnConnectTetheringFailure(
    const cryptauth::RemoteDevice& remote_device,
    ConnectTetheringResponse_ResponseCode error_code) {
  if (device_id_pending_connection_ != remote_device.GetDeviceId()) {
    // If the failure was part of a previous attempt for a different device,
    // ignore it.
    PA_LOG(INFO) << "Received failed ConnectTetheringResponse from device with "
                 << "ID " << remote_device.GetTruncatedDeviceIdForLogs()
                 << ", but a connection to another device has already started.";
    return;
  }

  PA_LOG(WARNING) << "Connection to device with ID "
                  << remote_device.GetTruncatedDeviceIdForLogs()
                  << " could not complete. Error code: " << error_code;

  connect_tethering_operation_->RemoveObserver(this);
  connect_tethering_operation_.reset();
  SetConnectionFailed(NetworkConnectionHandler::kErrorConnectFailed);
}

void TetherConnector::OnTetherHostToConnectFetched(
    const std::string& device_id,
    std::unique_ptr<cryptauth::RemoteDevice> tether_host_to_connect) {
  if (device_id_pending_connection_ != device_id) {
    PA_LOG(INFO) << "Device to connect to has changed while device with ID "
                 << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(device_id)
                 << " was being fetched.";
    return;
  }

  if (!tether_host_to_connect) {
    PA_LOG(ERROR) << "Could not fetch tether host with device ID "
                  << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(device_id)
                  << ". Cannot connect.";
    SetConnectionFailed(NetworkConnectionHandler::kErrorConnectFailed);
    return;
  }

  DCHECK(device_id == tether_host_to_connect->GetDeviceId());

  connect_tethering_operation_ =
      ConnectTetheringOperation::Factory::NewInstance(
          *tether_host_to_connect, connection_manager_,
          tether_host_response_recorder_);
  connect_tethering_operation_->AddObserver(this);
  connect_tethering_operation_->Initialize();
}

void TetherConnector::SetConnectionFailed(const std::string& error_name) {
  DCHECK(!device_id_pending_connection_.empty());
  DCHECK(!error_callback_.is_null());

  // Save a copy of the callback before resetting it below.
  network_handler::StringResultCallback error_callback = error_callback_;

  device_id_pending_connection_.clear();
  success_callback_.Reset();
  error_callback_.Reset();

  error_callback.Run(error_name);
  active_host_->SetActiveHostDisconnected();
}

void TetherConnector::SetConnectionSucceeded(
    const std::string& device_id,
    const std::string& wifi_network_guid) {
  DCHECK(!device_id_pending_connection_.empty());
  DCHECK(device_id_pending_connection_ == device_id);
  DCHECK(!success_callback_.is_null());

  // Save a copy of the callback before resetting it below.
  base::Closure success_callback = success_callback_;

  device_id_pending_connection_.clear();
  success_callback_.Reset();
  error_callback_.Reset();

  success_callback.Run();
  active_host_->SetActiveHostConnected(
      device_id,
      device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
          device_id),
      wifi_network_guid);
}

void TetherConnector::OnWifiConnection(const std::string& device_id,
                                       const std::string& wifi_network_guid) {
  if (device_id != device_id_pending_connection_) {
    // If the device ID does not match the ID of the device pending connection,
    // this is a stale attempt.
    PA_LOG(ERROR) << "Cannot connect to device with ID "
                  << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(device_id)
                  << " because another connection attempt has been started to "
                  << "a different device.";

    // TODO(khorimoto): Disconnect from the network.
    return;
  }

  if (wifi_network_guid.empty()) {
    // If the Wi-Fi network ID is empty, then the connection did not succeed.
    PA_LOG(ERROR) << "Failed to connect to the hotspot belonging to the device "
                  << "with ID "
                  << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(device_id)
                  << ".";

    SetConnectionFailed(NetworkConnectionHandler::kErrorConnectFailed);
    return;
  }

  SetConnectionSucceeded(device_id, wifi_network_guid);
}

}  // namespace tether

}  // namespace chromeos
