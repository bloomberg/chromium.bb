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

bool HostScanner::HasRecentlyScanned() {
  if (previous_scan_time_.is_null())
    return false;

  // TODO(khorimoto): Don't depend on MasterHostScanCache here.
  base::TimeDelta difference = clock_->Now() - previous_scan_time_;
  return difference.InMinutes() <
         MasterHostScanCache::kNumMinutesBeforeCacheEntryExpires;
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
  if (scanned_device_list_so_far.empty()) {
    // If a new scan is just starting up, remove existing cache entries; if they
    // are still within range to communicate with the current device, they will
    // show up in a subsequent OnTetherAvailabilityResponse() invocation.
    host_scan_cache_->ClearCacheExceptForActiveHost();
  } else {
    // Add all results received so far to the cache.
    for (auto& scanned_device_info : scanned_device_list_so_far) {
      SetCacheEntry(scanned_device_info);
    }

    if (scanned_device_list_so_far.size() == 1) {
      notification_presenter_->NotifyPotentialHotspotNearby(
          scanned_device_list_so_far.at(0).remote_device);
    } else {
      notification_presenter_->NotifyMultiplePotentialHotspotsNearby();
    }
  }

  if (is_final_scan_result) {
    if (scanned_device_list_so_far.empty()) {
      RecordHostScanResult(HostScanResultEventType::NOTIFICATION_NOT_SHOWN);
    } else if (scanned_device_list_so_far.size() == 1) {
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
    previous_scan_time_ = clock_->Now();
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

void HostScanner::RecordHostScanResult(HostScanResultEventType event_type) {
  DCHECK(event_type != HostScanResultEventType::HOST_SCAN_RESULT_MAX);
  UMA_HISTOGRAM_ENUMERATION("InstantTethering.HostScanResult", event_type,
                            HostScanResultEventType::HOST_SCAN_RESULT_MAX);
}

}  // namespace tether

}  // namespace chromeos
