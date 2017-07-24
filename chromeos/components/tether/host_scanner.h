// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_H_
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_H_

#include <string>
#include <unordered_set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/components/tether/host_scanner_operation.h"
#include "chromeos/components/tether/notification_presenter.h"
#include "chromeos/network/network_state_handler.h"
#include "components/cryptauth/remote_device.h"

namespace chromeos {

namespace tether {

class BleConnectionManager;
class DeviceIdTetherNetworkGuidMap;
class HostScanCache;
class HostScanDevicePrioritizer;
class TetherHostFetcher;
class TetherHostResponseRecorder;

// Scans for nearby tether hosts. When StartScan() is called, this class creates
// a new HostScannerOperation and uses it to contact nearby devices to query
// whether they can provide tether capabilities. Once the scan results are
// received, they are stored in the HostScanCache passed to the constructor,
// and observers are notified via HostScanner::Observer::ScanFinished().
class HostScanner : public HostScannerOperation::Observer {
 public:
  class Observer {
   public:
    void virtual ScanFinished() = 0;
  };

  HostScanner(TetherHostFetcher* tether_host_fetcher,
              BleConnectionManager* connection_manager,
              HostScanDevicePrioritizer* host_scan_device_prioritizer,
              TetherHostResponseRecorder* tether_host_response_recorder,
              NotificationPresenter* notification_presenter,
              DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map,
              HostScanCache* host_scan_cache,
              base::Clock* clock);
  virtual ~HostScanner();

  // Returns true if a scan is currently in progress.
  virtual bool IsScanActive();

  // Starts a host scan if there is no current scan. If a scan is active, this
  // function is a no-op.
  virtual void StartScan();

  // HostScannerOperation::Observer:
  void OnTetherAvailabilityResponse(
      std::vector<HostScannerOperation::ScannedDeviceInfo>&
          scanned_device_list_so_far,
      bool is_final_scan_result) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class HostScannerTest;
  friend class HostScanSchedulerTest;
  FRIEND_TEST_ALL_PREFIXES(HostScannerTest, TestScan_ResultsFromNoDevices);

  enum HostScanResultEventType {
    NOTIFICATION_NOT_SHOWN = 0,
    NOTIFICATION_SHOWN_SINGLE_HOST = 1,
    NOTIFICATION_SHOWN_MULTIPLE_HOSTS = 2,
    HOST_SCAN_RESULT_MAX
  };

  void NotifyScanFinished();

  void OnTetherHostsFetched(const cryptauth::RemoteDeviceList& tether_hosts);
  void SetCacheEntry(
      const HostScannerOperation::ScannedDeviceInfo& scanned_device_info);
  void OnFinalScanResultReceived(
      std::vector<HostScannerOperation::ScannedDeviceInfo>& final_scan_results);
  void RecordHostScanResult(HostScanResultEventType event_type);

  TetherHostFetcher* tether_host_fetcher_;
  BleConnectionManager* connection_manager_;
  HostScanDevicePrioritizer* host_scan_device_prioritizer_;
  TetherHostResponseRecorder* tether_host_response_recorder_;
  NotificationPresenter* notification_presenter_;
  DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map_;
  HostScanCache* host_scan_cache_;
  base::Clock* clock_;

  bool is_fetching_hosts_;
  std::unique_ptr<HostScannerOperation> host_scanner_operation_;
  std::unordered_set<std::string> tether_guids_in_cache_before_scan_;

  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<HostScanner> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostScanner);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCANNER_H_
