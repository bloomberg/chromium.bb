// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scanner.h"

#include <algorithm>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/device_status_util.h"
#include "chromeos/components/tether/host_scan_cache.h"
#include "chromeos/components/tether/master_host_scan_cache.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/network/network_state.h"
#include "components/cryptauth/remote_device_loader.h"

namespace chromeos {

namespace tether {

HostScanner::HostScanner(
    TetherHostFetcher* tether_host_fetcher,
    BleConnectionManager* connection_manager,
    HostScanDevicePrioritizer* host_scan_device_prioritizer,
    TetherHostResponseRecorder* tether_host_response_recorder,
    NotificationPresenter* notification_presenter,
    DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map,
    HostScanCache* host_scan_cache,
    base::Clock* clock)
    : tether_host_fetcher_(tether_host_fetcher),
      connection_manager_(connection_manager),
      host_scan_device_prioritizer_(host_scan_device_prioritizer),
      tether_host_response_recorder_(tether_host_response_recorder),
      notification_presenter_(notification_presenter),
      device_id_tether_network_guid_map_(device_id_tether_network_guid_map),
      host_scan_cache_(host_scan_cache),
      clock_(clock),
      is_fetching_hosts_(false),
      weak_ptr_factory_(this) {}

HostScanner::~HostScanner() {}

bool HostScanner::IsScanActive() {
  return is_fetching_hosts_ || host_scanner_operation_;
}

void HostScanner::StartScan() {
  if (IsScanActive()) {
    return;
  }

  is_fetching_hosts_ = true;
  tether_host_fetcher_->FetchAllTetherHosts(base::Bind(
      &HostScanner::OnTetherHostsFetched, weak_ptr_factory_.GetWeakPtr()));
}

void HostScanner::OnTetherHostsFetched(
    const cryptauth::RemoteDeviceList& tether_hosts) {
  is_fetching_hosts_ = false;

  tether_guids_in_cache_before_scan_ =
      host_scan_cache_->GetTetherGuidsInCache();

  host_scanner_operation_ = HostScannerOperation::Factory::NewInstance(
      tether_hosts, connection_manager_, host_scan_device_prioritizer_,
      tether_host_response_recorder_);
  host_scanner_operation_->AddObserver(this);
  host_scanner_operation_->Initialize();
}

void HostScanner::OnTetherAvailabilityResponse(
    std::vector<HostScannerOperation::ScannedDeviceInfo>&
        scanned_device_list_so_far,
    bool is_final_scan_result) {
  // Ensure all results received so far are in the cache (setting entries which
  // already exist is a no-op).
  for (const auto& scanned_device_info : scanned_device_list_so_far) {
    SetCacheEntry(scanned_device_info);
  }

  if (scanned_device_list_so_far.size() == 1u) {
    const cryptauth::RemoteDevice& remote_device =
        scanned_device_list_so_far.at(0).remote_device;
    int32_t signal_strength;
    NormalizeDeviceStatus(scanned_device_list_so_far.at(0).device_status,
                          nullptr /* carrier */,
                          nullptr /* battery_percentage */, &signal_strength);
    notification_presenter_->NotifyPotentialHotspotNearby(remote_device,
                                                          signal_strength);
  } else if (scanned_device_list_so_far.size() > 1u) {
    // Note: If a single-device notification was previously displayed, calling
    // NotifyMultiplePotentialHotspotsNearby() will reuse the existing
    // notification.
    notification_presenter_->NotifyMultiplePotentialHotspotsNearby();
  }

  if (is_final_scan_result) {
    OnFinalScanResultReceived(scanned_device_list_so_far);
  }
}

void HostScanner::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HostScanner::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void HostScanner::NotifyScanFinished() {
  for (auto& observer : observer_list_) {
    observer.ScanFinished();
  }
}

void HostScanner::SetCacheEntry(
    const HostScannerOperation::ScannedDeviceInfo& scanned_device_info) {
  const DeviceStatus& status = scanned_device_info.device_status;
  const cryptauth::RemoteDevice& remote_device =
      scanned_device_info.remote_device;

  std::string carrier;
  int32_t battery_percentage;
  int32_t signal_strength;
  NormalizeDeviceStatus(status, &carrier, &battery_percentage,
                        &signal_strength);

  host_scan_cache_->SetHostScanResult(
      *HostScanCacheEntry::Builder()
           .SetTetherNetworkGuid(device_id_tether_network_guid_map_
                                     ->GetTetherNetworkGuidForDeviceId(
                                         remote_device.GetDeviceId()))
           .SetDeviceName(remote_device.name)
           .SetCarrier(carrier)
           .SetBatteryPercentage(battery_percentage)
           .SetSignalStrength(signal_strength)
           .SetSetupRequired(scanned_device_info.setup_required)
           .Build());
}

void HostScanner::OnFinalScanResultReceived(
    std::vector<HostScannerOperation::ScannedDeviceInfo>& final_scan_results) {
  // Search through all GUIDs that were in the cache before the scan began. If
  // any of those GUIDs are not present in the final scan results, remove them
  // from the cache.
  for (const auto& tether_guid_in_cache : tether_guids_in_cache_before_scan_) {
    bool is_guid_in_final_scan_results = false;

    for (const auto& scan_result : final_scan_results) {
      if (device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
              scan_result.remote_device.GetDeviceId()) ==
          tether_guid_in_cache) {
        is_guid_in_final_scan_results = true;
        break;
      }
    }

    if (!is_guid_in_final_scan_results)
      host_scan_cache_->RemoveHostScanResult(tether_guid_in_cache);
  }

  if (final_scan_results.empty()) {
    RecordHostScanResult(HostScanResultEventType::NOTIFICATION_NOT_SHOWN);
  } else if (final_scan_results.size() == 1u) {
    RecordHostScanResult(
        HostScanResultEventType::NOTIFICATION_SHOWN_SINGLE_HOST);
  } else {
    RecordHostScanResult(
        HostScanResultEventType::NOTIFICATION_SHOWN_MULTIPLE_HOSTS);
  }

  // If the final scan result has been received, the operation is finished.
  // Delete it.
  host_scanner_operation_->RemoveObserver(this);
  host_scanner_operation_.reset();

  NotifyScanFinished();
}

void HostScanner::RecordHostScanResult(HostScanResultEventType event_type) {
  DCHECK(event_type != HostScanResultEventType::HOST_SCAN_RESULT_MAX);
  UMA_HISTOGRAM_ENUMERATION("InstantTethering.HostScanResult", event_type,
                            HostScanResultEventType::HOST_SCAN_RESULT_MAX);
}

}  // namespace tether

}  // namespace chromeos
