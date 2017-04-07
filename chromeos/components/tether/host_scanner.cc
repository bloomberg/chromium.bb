// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scanner.h"

#include "base/bind.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/network/network_state.h"
#include "components/cryptauth/remote_device_loader.h"

namespace chromeos {

namespace tether {

HostScanner::HostScanner(
    TetherHostFetcher* tether_host_fetcher,
    BleConnectionManager* connection_manager,
    HostScanDevicePrioritizer* host_scan_device_prioritizer,
    NetworkStateHandler* network_state_handler,
    NotificationPresenter* notification_presenter)
    : tether_host_fetcher_(tether_host_fetcher),
      connection_manager_(connection_manager),
      host_scan_device_prioritizer_(host_scan_device_prioritizer),
      network_state_handler_(network_state_handler),
      notification_presenter_(notification_presenter),
      is_fetching_hosts_(false),
      weak_ptr_factory_(this) {}

HostScanner::~HostScanner() {}

void HostScanner::StartScan() {
  if (host_scanner_operation_) {
    // If a scan is already active, do not continue.
    return;
  }

  if (is_fetching_hosts_) {
    // If fetching tether hosts is active, stop and wait for them to load.
    return;
  }
  is_fetching_hosts_ = true;

  tether_host_fetcher_->FetchAllTetherHosts(base::Bind(
      &HostScanner::OnTetherHostsFetched, weak_ptr_factory_.GetWeakPtr()));
}

bool HostScanner::IsScanActive() {
  return is_fetching_hosts_ || host_scanner_operation_;
}

void HostScanner::OnTetherHostsFetched(
    const cryptauth::RemoteDeviceList& tether_hosts) {
  is_fetching_hosts_ = false;

  host_scanner_operation_ = HostScannerOperation::Factory::NewInstance(
      tether_hosts, connection_manager_, host_scan_device_prioritizer_);
  host_scanner_operation_->AddObserver(this);
  host_scanner_operation_->Initialize();
}

void HostScanner::OnTetherAvailabilityResponse(
    std::vector<HostScannerOperation::ScannedDeviceInfo>&
        scanned_device_list_so_far,
    bool is_final_scan_result) {
  most_recent_scan_results_ = scanned_device_list_so_far;

  if (!scanned_device_list_so_far.empty()) {
    // TODO (hansberry): Clear out old scanned hosts from NetworkStateHandler.
    // TODO (hansberry): Add battery and cell strength properties once
    // available.
    for (auto& scanned_device_info : scanned_device_list_so_far) {
      cryptauth::RemoteDevice remote_device = scanned_device_info.remote_device;
      network_state_handler_->AddTetherNetworkState(remote_device.GetDeviceId(),
                                                    remote_device.name);
    }

    if (scanned_device_list_so_far.size() == 1) {
      notification_presenter_->NotifyPotentialHotspotNearby(
          scanned_device_list_so_far.at(0).remote_device);
    } else {
      notification_presenter_->NotifyMultiplePotentialHotspotsNearby();
    }
  }

  if (is_final_scan_result) {
    // If the final scan result has been received, the operation is finished.
    // Delete it.
    host_scanner_operation_->RemoveObserver(this);
    host_scanner_operation_.reset();
  }
}

}  // namespace tether

}  // namespace chromeos
