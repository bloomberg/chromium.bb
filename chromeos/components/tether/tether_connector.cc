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
    NetworkConnect* network_connect,
    NetworkStateHandler* network_state_handler,
    WifiHotspotConnector* wifi_hotspot_connector,
    ActiveHost* active_host,
    TetherHostFetcher* tether_host_fetcher,
    BleConnectionManager* connection_manager,
    HostScanDevicePrioritizer* host_scan_device_prioritizer,
    DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map)
    : network_connect_(network_connect),
      network_state_handler_(network_state_handler),
      wifi_hotspot_connector_(wifi_hotspot_connector),
      active_host_(active_host),
      tether_host_fetcher_(tether_host_fetcher),
      connection_manager_(connection_manager),
      host_scan_device_prioritizer_(host_scan_device_prioritizer),
      device_id_tether_network_guid_map_(device_id_tether_network_guid_map),
      weak_ptr_factory_(this) {
  network_connect_->SetTetherDelegate(this);
}

TetherConnector::~TetherConnector() {
  network_connect_->SetTetherDelegate(nullptr);
  if (connect_tethering_operation_) {
    connect_tethering_operation_->RemoveObserver(this);
  }
}

void TetherConnector::ConnectToNetwork(const std::string& guid) {
  PA_LOG(INFO) << "Attempting to connect to network with GUID " << guid << ".";

  std::string device_id =
      device_id_tether_network_guid_map_->GetDeviceIdForTetherNetworkGuid(guid);

  if (device_id_pending_connection_ == device_id) {
    PA_LOG(INFO) << "Connection attempt requested for network with GUID "
                 << guid << ", but a connection attempt is already in "
                 << "progress. Continuing with the existing attempt.";
    return;
  }

  if (connect_tethering_operation_) {
    DCHECK(!device_id_pending_connection_.empty());

    PA_LOG(INFO) << "A connection attempt was already in progress to device "
                 << "with ID " << device_id_pending_connection_ << ". "
                 << "Canceling that connection attempt before continuing.";

    // If a connection to a *different* device is pending, stop the connection
    // attempt.
    connect_tethering_operation_->RemoveObserver(this);
    connect_tethering_operation_.reset();
  }

  device_id_pending_connection_ = device_id;

  tether_host_fetcher_->FetchTetherHost(
      device_id_pending_connection_,
      base::Bind(&TetherConnector::OnTetherHostToConnectFetched,
                 weak_ptr_factory_.GetWeakPtr(),
                 device_id_pending_connection_));
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
      ssid_copy, password_copy,
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
                  << " could not connect. Error code: " << error_code;

  connect_tethering_operation_->RemoveObserver(this);
  connect_tethering_operation_.reset();
  SetDisconnected();
}

void TetherConnector::OnTetherHostToConnectFetched(
    const std::string& device_id,
    std::unique_ptr<cryptauth::RemoteDevice> tether_host_to_connect) {
  if (!tether_host_to_connect) {
    PA_LOG(ERROR) << "Could not fetch tether host with device ID "
                  << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(device_id)
                  << ". Cannot connect.";
    return;
  }

  if (device_id_pending_connection_ != tether_host_to_connect->GetDeviceId()) {
    PA_LOG(INFO) << "Device to connect to has changed while device with ID "
                 << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(device_id)
                 << " was being fetched.";
    return;
  }

  active_host_->SetActiveHostConnecting(
      device_id_pending_connection_,
      device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
          device_id_pending_connection_));

  connect_tethering_operation_ =
      ConnectTetheringOperation::Factory::NewInstance(
          *tether_host_to_connect, connection_manager_,
          host_scan_device_prioritizer_);
  connect_tethering_operation_->AddObserver(this);
  connect_tethering_operation_->Initialize();
}

void TetherConnector::SetDisconnected() {
  device_id_pending_connection_ = "";
  active_host_->SetActiveHostDisconnected();
}

void TetherConnector::SetConnected(const std::string& device_id,
                                   const std::string& wifi_network_guid) {
  device_id_pending_connection_ = "";
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

    SetDisconnected();
    return;
  }

  bool successful_association =
      network_state_handler_->AssociateTetherNetworkStateWithWifiNetwork(
          device_id, wifi_network_guid);
  if (successful_association) {
    PA_LOG(INFO) << "Successfully connected to host device with ID "
                 << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(device_id)
                 << ". Tether network ID: \"" << device_id
                 << "\", Wi-Fi network ID: \"" << wifi_network_guid << "\"";
  } else {
    PA_LOG(WARNING) << "Successfully connected to host device with ID "
                    << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(
                           device_id)
                    << ", but failed to associate tether network with ID \""
                    << device_id << "\" to Wi-Fi network with ID \""
                    << wifi_network_guid << "\".";
  }

  SetConnected(device_id, wifi_network_guid);
}

}  // namespace tether

}  // namespace chromeos
