// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_H_
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chromeos/components/tether/host_scanner_operation.h"
#include "chromeos/components/tether/notification_presenter.h"
#include "chromeos/network/network_state_handler.h"
#include "components/cryptauth/remote_device.h"

namespace chromeos {

namespace tether {

class BleConnectionManager;
class HostScanDevicePrioritizer;
class TetherHostFetcher;

// Scans for nearby tether hosts.
// TODO(khorimoto): Add some sort of "staleness" timeout which removes scan
//                  results which occurred long enough ago that they are no
//                  longer valid.
class HostScanner : public HostScannerOperation::Observer {
 public:
  HostScanner(TetherHostFetcher* tether_host_fetcher,
              BleConnectionManager* connection_manager,
              HostScanDevicePrioritizer* host_scan_device_prioritizer,
              NetworkStateHandler* network_state_handler,
              NotificationPresenter* notification_presenter);
  virtual ~HostScanner();

  // Starts a host scan if there is no current scan. If a scan is ongoing, this
  // function is a no-op.
  virtual void StartScan();

  bool IsScanActive();

  std::vector<HostScannerOperation::ScannedDeviceInfo>
  most_recent_scan_results() {
    return most_recent_scan_results_;
  }

  // HostScannerOperation::Observer:
  void OnTetherAvailabilityResponse(
      std::vector<HostScannerOperation::ScannedDeviceInfo>&
          scanned_device_list_so_far,
      bool is_final_scan_result) override;

 private:
  friend class HostScannerTest;
  friend class HostScanSchedulerTest;

  void OnTetherHostsFetched(const cryptauth::RemoteDeviceList& tether_hosts);

  TetherHostFetcher* tether_host_fetcher_;
  BleConnectionManager* connection_manager_;
  HostScanDevicePrioritizer* host_scan_device_prioritizer_;
  NetworkStateHandler* network_state_handler_;
  NotificationPresenter* notification_presenter_;

  bool is_fetching_hosts_;
  std::unique_ptr<HostScannerOperation> host_scanner_operation_;
  std::vector<HostScannerOperation::ScannedDeviceInfo>
      most_recent_scan_results_;

  base::WeakPtrFactory<HostScanner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostScanner);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_H_
