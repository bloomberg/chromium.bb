// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scanner.h"

#include <algorithm>

#include "base/bind.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/host_scan_cache.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/network/network_state.h"
#include "components/cryptauth/remote_device_loader.h"

namespace chromeos {

namespace tether {

namespace {

const char kDefaultCellCarrierName[] = "unknown-carrier";

// Android signal strength is measured between 0 and 4 (inclusive), but Chrome
// OS signal strength is measured between 0 and 100 (inclusive). In order to
// convert between Android signal strength to Chrome OS signal strength, the
// value must be multiplied by the below value.
const int32_t kAndroidTetherHostToChromeOSSignalStrengthMultiplier = 25;

int32_t ForceBetweenZeroAndOneHundred(int32_t value) {
  return std::min(std::max(value, 0), 100);
}

}  // namespace

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

  base::TimeDelta difference = clock_->Now() - previous_scan_time_;
  return difference.InMinutes() <
         HostScanCache::kNumMinutesBeforeCacheEntryExpires;
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

  // Use a sentinel value if carrier information is not available. This value is
  // special-cased and replaced with a localized string in the settings UI.
  const std::string carrier =
      (!status.has_cell_provider() || status.cell_provider().empty())
          ? kDefaultCellCarrierName
          : status.cell_provider();

  // If battery or signal strength are missing, assume they are 100. For
  // battery percentage, force the value to be between 0 and 100. For signal
  // strength, convert from Android signal strength to Chrome OS signal
  // strength and force the value to be between 0 and 100.
  const int32_t battery_percentage =
      status.has_battery_percentage()
          ? ForceBetweenZeroAndOneHundred(status.battery_percentage())
          : 100;
  const int32_t signal_strength =
      status.has_connection_strength()
          ? ForceBetweenZeroAndOneHundred(
                kAndroidTetherHostToChromeOSSignalStrengthMultiplier *
                status.connection_strength())
          : 100;

  host_scan_cache_->SetHostScanResult(
      device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
          remote_device.GetDeviceId()),
      remote_device.name, carrier, battery_percentage, signal_strength);
}

}  // namespace tether

}  // namespace chromeos
