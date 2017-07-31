// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scanner.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/fake_host_scan_cache.h"
#include "chromeos/components/tether/fake_notification_presenter.h"
#include "chromeos/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/components/tether/host_scan_device_prioritizer.h"
#include "chromeos/components/tether/host_scanner.h"
#include "chromeos/components/tether/master_host_scan_cache.h"
#include "chromeos/components/tether/mock_tether_host_response_recorder.h"
#include "chromeos/components/tether/proto_test_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_shill_service_client.h"
#include "chromeos/network/network_state_test.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace tether {

namespace {

class TestObserver : public HostScanner::Observer {
 public:
  void ScanFinished() override { scan_finished_count_++; }

  int scan_finished_count() { return scan_finished_count_; }

 private:
  int scan_finished_count_ = 0;
};

class FakeHostScanDevicePrioritizer : public HostScanDevicePrioritizer {
 public:
  FakeHostScanDevicePrioritizer() : HostScanDevicePrioritizer() {}
  ~FakeHostScanDevicePrioritizer() override {}

  // Simply leave |remote_devices| as-is.
  void SortByHostScanOrder(
      std::vector<cryptauth::RemoteDevice>* remote_devices) const override {}
};

class FakeHostScannerOperation : public HostScannerOperation {
 public:
  FakeHostScannerOperation(
      const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
      BleConnectionManager* connection_manager,
      HostScanDevicePrioritizer* host_scan_device_prioritizer,
      TetherHostResponseRecorder* tether_host_response_recorder)
      : HostScannerOperation(devices_to_connect,
                             connection_manager,
                             host_scan_device_prioritizer,
                             tether_host_response_recorder) {}

  ~FakeHostScannerOperation() override {}

  void SendScannedDeviceListUpdate(
      const std::vector<HostScannerOperation::ScannedDeviceInfo>&
          scanned_device_list_so_far,
      bool is_final_scan_result) {
    scanned_device_list_so_far_ = scanned_device_list_so_far;
    NotifyObserversOfScannedDeviceList(is_final_scan_result);
  }
};

class FakeHostScannerOperationFactory : public HostScannerOperation::Factory {
 public:
  FakeHostScannerOperationFactory(
      const std::vector<cryptauth::RemoteDevice>& test_devices)
      : expected_devices_(test_devices) {}
  virtual ~FakeHostScannerOperationFactory() {}

  std::vector<FakeHostScannerOperation*>& created_operations() {
    return created_operations_;
  }

 protected:
  // HostScannerOperation::Factory:
  std::unique_ptr<HostScannerOperation> BuildInstance(
      const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
      BleConnectionManager* connection_manager,
      HostScanDevicePrioritizer* host_scan_device_prioritizer,
      TetherHostResponseRecorder* tether_host_response_recorder) override {
    EXPECT_EQ(expected_devices_, devices_to_connect);
    FakeHostScannerOperation* operation = new FakeHostScannerOperation(
        devices_to_connect, connection_manager, host_scan_device_prioritizer,
        tether_host_response_recorder);
    created_operations_.push_back(operation);
    return base::WrapUnique(operation);
  }

 private:
  const std::vector<cryptauth::RemoteDevice>& expected_devices_;
  std::vector<FakeHostScannerOperation*> created_operations_;
};

std::string GenerateCellProviderForDevice(
    const cryptauth::RemoteDevice& remote_device) {
  // Return a string unique to |remote_device|.
  return "cellProvider" + remote_device.GetTruncatedDeviceIdForLogs();
}

std::vector<HostScannerOperation::ScannedDeviceInfo>
CreateFakeScannedDeviceInfos(
    const std::vector<cryptauth::RemoteDevice>& remote_devices) {
  // At least 4 ScannedDeviceInfos should be created to ensure that all 4 cases
  // described below are tested.
  EXPECT_GT(remote_devices.size(), 3u);

  std::vector<HostScannerOperation::ScannedDeviceInfo> scanned_device_infos;

  for (size_t i = 0; i < remote_devices.size(); ++i) {
    // Four field possibilities:
    // i % 4 == 0: Field is not supplied.
    // i % 4 == 1: Field is below the minimum value (int fields only).
    // i % 4 == 2: Field is within the valid range (int fields only).
    // i % 4 == 3: Field is above the maximium value (int fields only).
    std::string cell_provider_name;
    int battery_percentage;
    int connection_strength;
    switch (i % 4) {
      case 0:
        cell_provider_name = proto_test_util::kDoNotSetStringField;
        battery_percentage = proto_test_util::kDoNotSetIntField;
        connection_strength = proto_test_util::kDoNotSetIntField;
        break;
      case 1:
        cell_provider_name = GenerateCellProviderForDevice(remote_devices[i]);
        battery_percentage = -1 - i;
        connection_strength = -1 - i;
        break;
      case 2:
        cell_provider_name = GenerateCellProviderForDevice(remote_devices[i]);
        battery_percentage = (50 + i) % 100;  // Valid range is [0, 100].
        connection_strength = (1 + i) % 4;    // Valid range is [0, 4].
        break;
      case 3:
        cell_provider_name = GenerateCellProviderForDevice(remote_devices[i]);
        battery_percentage = 101 + i;
        connection_strength = 101 + i;
        break;
      default:
        NOTREACHED();
        // Set values for |battery_percentage| and |connection_strength| here to
        // prevent a compiler warning which says that they may be unset at this
        // point.
        battery_percentage = 0;
        connection_strength = 0;
        break;
    }

    DeviceStatus device_status = CreateTestDeviceStatus(
        cell_provider_name, battery_percentage, connection_strength);

    // Require set-up for odd-numbered device indices.
    bool setup_required = i % 2 == 0;

    scanned_device_infos.push_back(HostScannerOperation::ScannedDeviceInfo(
        remote_devices[i], device_status, setup_required));
  }

  return scanned_device_infos;
}

}  // namespace

class HostScannerTest : public NetworkStateTest {
 protected:
  HostScannerTest()
      : test_devices_(cryptauth::GenerateTestRemoteDevices(4)),
        test_scanned_device_infos(CreateFakeScannedDeviceInfos(test_devices_)) {
  }

  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkHandler::Initialize();
    NetworkStateTest::SetUp();

    scanned_device_infos_from_current_scan_.clear();

    fake_tether_host_fetcher_ = base::MakeUnique<FakeTetherHostFetcher>(
        test_devices_, false /* synchronously_reply_with_results */);
    fake_ble_connection_manager_ = base::MakeUnique<FakeBleConnectionManager>();
    fake_host_scan_device_prioritizer_ =
        base::MakeUnique<FakeHostScanDevicePrioritizer>();
    mock_tether_host_response_recorder_ =
        base::MakeUnique<MockTetherHostResponseRecorder>();
    fake_notification_presenter_ =
        base::MakeUnique<FakeNotificationPresenter>();
    device_id_tether_network_guid_map_ =
        base::MakeUnique<DeviceIdTetherNetworkGuidMap>();
    fake_host_scan_cache_ = base::MakeUnique<FakeHostScanCache>();

    fake_host_scanner_operation_factory_ =
        base::WrapUnique(new FakeHostScannerOperationFactory(test_devices_));
    HostScannerOperation::Factory::SetInstanceForTesting(
        fake_host_scanner_operation_factory_.get());

    test_clock_ = base::MakeUnique<base::SimpleTestClock>();

    host_scanner_ = base::WrapUnique(new HostScanner(
        network_state_handler(), fake_tether_host_fetcher_.get(),
        fake_ble_connection_manager_.get(),
        fake_host_scan_device_prioritizer_.get(),
        mock_tether_host_response_recorder_.get(),
        fake_notification_presenter_.get(),
        device_id_tether_network_guid_map_.get(), fake_host_scan_cache_.get(),
        test_clock_.get()));

    test_observer_ = base::MakeUnique<TestObserver>();
    host_scanner_->AddObserver(test_observer_.get());
  }

  void TearDown() override {
    host_scanner_->RemoveObserver(test_observer_.get());
    HostScannerOperation::Factory::SetInstanceForTesting(nullptr);

    NetworkStateTest::TearDown();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

  // Causes |fake_operation| to receive the scan result in
  // |test_scanned_device_infos| vector at the index |test_device_index| with
  // the "final result" value of |is_final_scan_result|.
  void ReceiveScanResultAndVerifySuccess(
      FakeHostScannerOperation& fake_operation,
      size_t test_device_index,
      bool is_final_scan_result,
      bool is_connected_to_internet) {
    bool already_in_list = false;
    for (auto& scanned_device_info : scanned_device_infos_from_current_scan_) {
      if (scanned_device_info.remote_device.GetDeviceId() ==
          test_devices_[test_device_index].GetDeviceId()) {
        already_in_list = true;
        break;
      }
    }

    if (!already_in_list) {
      scanned_device_infos_from_current_scan_.push_back(
          test_scanned_device_infos[test_device_index]);
    }

    int previous_scan_finished_count = test_observer_->scan_finished_count();
    fake_operation.SendScannedDeviceListUpdate(
        scanned_device_infos_from_current_scan_, is_final_scan_result);
    VerifyScanResultsMatchCache();
    EXPECT_EQ(previous_scan_finished_count + (is_final_scan_result ? 1 : 0),
              test_observer_->scan_finished_count());

    if (is_connected_to_internet) {
      EXPECT_EQ(FakeNotificationPresenter::PotentialHotspotNotificationState::
                    NO_HOTSPOT_NOTIFICATION_SHOWN,
                fake_notification_presenter_->potential_hotspot_state());
    } else if (scanned_device_infos_from_current_scan_.size() == 1) {
      EXPECT_EQ(FakeNotificationPresenter::PotentialHotspotNotificationState::
                    SINGLE_HOTSPOT_NEARBY_SHOWN,
                fake_notification_presenter_->potential_hotspot_state());
    } else {
      EXPECT_EQ(FakeNotificationPresenter::PotentialHotspotNotificationState::
                    MULTIPLE_HOTSPOTS_NEARBY_SHOWN,
                fake_notification_presenter_->potential_hotspot_state());
    }

    if (is_final_scan_result) {
      HostScanner::HostScanResultEventType expected_event_type =
          HostScanner::HostScanResultEventType::NOTIFICATION_NOT_SHOWN;
      if (scanned_device_infos_from_current_scan_.size() == 1) {
        expected_event_type = HostScanner::HostScanResultEventType::
            NOTIFICATION_SHOWN_SINGLE_HOST;
      } else if (scanned_device_infos_from_current_scan_.size() > 1) {
        expected_event_type = HostScanner::HostScanResultEventType::
            NOTIFICATION_SHOWN_MULTIPLE_HOSTS;
      }
      histogram_tester_.ExpectUniqueSample("InstantTethering.HostScanResult",
                                           expected_event_type, 1);
    }
  }

  void VerifyScanResultsMatchCache() {
    std::vector<HostScannerOperation::ScannedDeviceInfo> combined_device_infos =
        scanned_device_infos_from_current_scan_;
    for (const auto& previous_scan_result :
         scanned_device_infos_from_previous_scans_) {
      bool already_in_combined = false;
      for (const auto& combined_device_info : combined_device_infos) {
        if (previous_scan_result.remote_device.GetDeviceId() ==
            combined_device_info.remote_device.GetDeviceId()) {
          already_in_combined = true;
          break;
        }
      }
      if (!already_in_combined)
        combined_device_infos.push_back(previous_scan_result);
    }

    ASSERT_EQ(combined_device_infos.size(), fake_host_scan_cache_->size());
    for (auto& scanned_device_info : combined_device_infos) {
      std::string tether_network_guid =
          device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
              scanned_device_info.remote_device.GetDeviceId());
      const HostScanCacheEntry* entry =
          fake_host_scan_cache_->GetCacheEntry(tether_network_guid);
      ASSERT_TRUE(entry);
      VerifyScannedDeviceInfoAndCacheEntryAreEquivalent(scanned_device_info,
                                                        *entry);
    }
  }

  void VerifyScannedDeviceInfoAndCacheEntryAreEquivalent(
      const HostScannerOperation::ScannedDeviceInfo& scanned_device_info,
      const HostScanCacheEntry& entry) {
    EXPECT_EQ(scanned_device_info.remote_device.name, entry.device_name);

    const DeviceStatus& status = scanned_device_info.device_status;
    if (!status.has_cell_provider() || status.cell_provider().empty())
      EXPECT_EQ("unknown-carrier", entry.carrier);
    else
      EXPECT_EQ(status.cell_provider(), entry.carrier);

    if (!status.has_battery_percentage() || status.battery_percentage() > 100)
      EXPECT_EQ(100, entry.battery_percentage);
    else if (status.battery_percentage() < 0)
      EXPECT_EQ(0, entry.battery_percentage);
    else
      EXPECT_EQ(status.battery_percentage(), entry.battery_percentage);

    if (!status.has_connection_strength() || status.connection_strength() > 4)
      EXPECT_EQ(100, entry.signal_strength);
    else if (status.connection_strength() < 0)
      EXPECT_EQ(0, entry.signal_strength);
    else
      EXPECT_EQ(status.connection_strength() * 25, entry.signal_strength);

    EXPECT_EQ(scanned_device_info.setup_required, entry.setup_required);
  }

  // Clears scan results from |scanned_device_infos_from_current_scan_| and
  // transfers them to |scanned_device_infos_from_previous_scans_|.
  void ClearCurrentScanResults() {
    scanned_device_infos_from_previous_scans_ =
        scanned_device_infos_from_current_scan_;
    scanned_device_infos_from_current_scan_.clear();
  }

  void ConnectToWifiNetwork() {
    std::stringstream ss;
    ss << "{"
       << "  \"GUID\": \"wifiNetworkGuid\","
       << "  \"Type\": \"" << shill::kTypeWifi << "\","
       << "  \"State\": \"" << shill::kStateOnline << "\""
       << "}";

    ConfigureService(ss.str());
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  const std::vector<cryptauth::RemoteDevice> test_devices_;
  const std::vector<HostScannerOperation::ScannedDeviceInfo>
      test_scanned_device_infos;

  std::unique_ptr<FakeTetherHostFetcher> fake_tether_host_fetcher_;
  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<HostScanDevicePrioritizer> fake_host_scan_device_prioritizer_;
  std::unique_ptr<MockTetherHostResponseRecorder>
      mock_tether_host_response_recorder_;
  std::unique_ptr<FakeNotificationPresenter> fake_notification_presenter_;
  // TODO(hansberry): Use a fake for this when a real mapping scheme is created.
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;
  std::unique_ptr<FakeHostScanCache> fake_host_scan_cache_;

  std::unique_ptr<base::SimpleTestClock> test_clock_;
  std::unique_ptr<TestObserver> test_observer_;

  std::unique_ptr<FakeHostScannerOperationFactory>
      fake_host_scanner_operation_factory_;

  std::vector<HostScannerOperation::ScannedDeviceInfo>
      scanned_device_infos_from_current_scan_;
  std::vector<HostScannerOperation::ScannedDeviceInfo>
      scanned_device_infos_from_previous_scans_;

  std::unique_ptr<HostScanner> host_scanner_;

  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostScannerTest);
};

TEST_F(HostScannerTest, TestScan_ConnectedToExistingNetwork) {
  ConnectToWifiNetwork();
  EXPECT_TRUE(network_state_handler()->DefaultNetwork());

  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  fake_tether_host_fetcher_->InvokePendingCallbacks();
  ASSERT_EQ(1u,
            fake_host_scanner_operation_factory_->created_operations().size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[0],
      0u /* test_device_index */, false /* is_final_scan_result */,
      true /* is_connected_to_internet */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[0],
      1u /* test_device_index */, false /* is_final_scan_result */,
      true /* is_connected_to_internet */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[0],
      2u /* test_device_index */, false /* is_final_scan_result */,
      true /* is_connected_to_internet */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[0],
      3u /* test_device_index */, true /* is_final_scan_result */,
      true /* is_connected_to_internet */);
  EXPECT_FALSE(host_scanner_->IsScanActive());
}

TEST_F(HostScannerTest, TestScan_ResultsFromAllDevices) {
  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  fake_tether_host_fetcher_->InvokePendingCallbacks();
  ASSERT_EQ(1u,
            fake_host_scanner_operation_factory_->created_operations().size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[0],
      0u /* test_device_index */, false /* is_final_scan_result */,
      false /* is_connected_to_internet */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[0],
      1u /* test_device_index */, false /* is_final_scan_result */,
      false /* is_connected_to_internet */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[0],
      2u /* test_device_index */, false /* is_final_scan_result */,
      false /* is_connected_to_internet */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[0],
      3u /* test_device_index */, true /* is_final_scan_result */,
      false /* is_connected_to_internet */);
  EXPECT_FALSE(host_scanner_->IsScanActive());
}

TEST_F(HostScannerTest, TestScan_ResultsFromNoDevices) {
  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  fake_tether_host_fetcher_->InvokePendingCallbacks();
  ASSERT_EQ(1u,
            fake_host_scanner_operation_factory_->created_operations().size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  fake_host_scanner_operation_factory_->created_operations()[0]
      ->SendScannedDeviceListUpdate(
          std::vector<HostScannerOperation::ScannedDeviceInfo>(),
          true /* is_final_scan_result */);
  EXPECT_EQ(0u, fake_host_scan_cache_->size());
  EXPECT_FALSE(host_scanner_->IsScanActive());

  histogram_tester_.ExpectUniqueSample(
      "InstantTethering.HostScanResult",
      HostScanner::HostScanResultEventType::NOTIFICATION_NOT_SHOWN, 1);
}

TEST_F(HostScannerTest, TestScan_ResultsFromSomeDevices) {
  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  fake_tether_host_fetcher_->InvokePendingCallbacks();
  ASSERT_EQ(1u,
            fake_host_scanner_operation_factory_->created_operations().size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // Only receive updates from the 0th and 1st device.
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[0],
      0u /* test_device_index */, false /* is_final_scan_result */,
      false /* is_connected_to_internet */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[0],
      1u /* test_device_index */, false /* is_final_scan_result */,
      false /* is_connected_to_internet */);
  EXPECT_TRUE(host_scanner_->IsScanActive());

  fake_host_scanner_operation_factory_->created_operations()[0]
      ->SendScannedDeviceListUpdate(scanned_device_infos_from_current_scan_,
                                    true /* is_final_scan_result */);
  EXPECT_EQ(scanned_device_infos_from_current_scan_.size(),
            fake_host_scan_cache_->size());
  EXPECT_FALSE(host_scanner_->IsScanActive());
}

TEST_F(HostScannerTest, TestScan_MultipleScanCallsDuringOperation) {
  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // Call StartScan() again before the tether host fetcher has finished. This
  // should be a no-op.
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // No devices should have been received yet.
  EXPECT_EQ(0u, fake_host_scan_cache_->size());

  fake_tether_host_fetcher_->InvokePendingCallbacks();
  ASSERT_EQ(1u,
            fake_host_scanner_operation_factory_->created_operations().size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // Call StartScan again after the tether host fetcher has finished but before
  // the final scan result has been received. This should be a no-op.
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // No devices should have been received yet.
  EXPECT_EQ(0u, fake_host_scan_cache_->size());

  // Receive updates from the 0th device.
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[0],
      0u /* test_device_index */, false /* is_final_scan_result */,
      false /* is_connected_to_internet */);
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // Call StartScan again after a scan result has been received but before
  // the final scan result ha been received. This should be a no-op.
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // The scanned devices so far should be the same (i.e., they should not have
  // been affected by the extra call to StartScan()).
  EXPECT_EQ(scanned_device_infos_from_current_scan_.size(),
            fake_host_scan_cache_->size());

  // Finally, finish the scan.
  fake_host_scanner_operation_factory_->created_operations()[0]
      ->SendScannedDeviceListUpdate(scanned_device_infos_from_current_scan_,
                                    true /* is_final_scan_result */);
  EXPECT_EQ(scanned_device_infos_from_current_scan_.size(),
            fake_host_scan_cache_->size());
  EXPECT_FALSE(host_scanner_->IsScanActive());
}

TEST_F(HostScannerTest, TestScan_MultipleCompleteScanSessions) {
  // Start the first scan session.
  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  fake_tether_host_fetcher_->InvokePendingCallbacks();
  ASSERT_EQ(1u,
            fake_host_scanner_operation_factory_->created_operations().size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // Receive updates from devices 0-2.
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[0],
      0u /* test_device_index */, false /* is_final_scan_result */,
      false /* is_connected_to_internet */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[0],
      1u /* test_device_index */, false /* is_final_scan_result */,
      false /* is_connected_to_internet */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[0],
      2u /* test_device_index */, false /* is_final_scan_result */,
      false /* is_connected_to_internet */);
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // Finish the first scan.
  fake_host_scanner_operation_factory_->created_operations()[0]
      ->SendScannedDeviceListUpdate(scanned_device_infos_from_current_scan_,
                                    true /* is_final_scan_result */);
  EXPECT_EQ(scanned_device_infos_from_current_scan_.size(),
            fake_host_scan_cache_->size());
  EXPECT_FALSE(host_scanner_->IsScanActive());

  // Now, start the second scan session.
  ClearCurrentScanResults();
  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  fake_tether_host_fetcher_->InvokePendingCallbacks();
  ASSERT_EQ(2u,
            fake_host_scanner_operation_factory_->created_operations().size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // The cache should be unaffected by the start of a new scan.
  VerifyScanResultsMatchCache();

  // Receive results from devices 0, 2 and 3. Results from device 1 should still
  // be present in the cache even though no results have been received from that
  // device during this scan session.
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[1],
      0u /* test_device_index */, false /* is_final_scan_result */,
      false /* is_connected_to_internet */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[1],
      2u /* test_device_index */, false /* is_final_scan_result */,
      false /* is_connected_to_internet */);
  ReceiveScanResultAndVerifySuccess(
      *fake_host_scanner_operation_factory_->created_operations()[1],
      3u /* test_device_index */, false /* is_final_scan_result */,
      false /* is_connected_to_internet */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  VerifyScanResultsMatchCache();

  // Finish the second scan. Since results were not received from device 1,
  // previous results from evice 1 should now be removed from the cache.
  fake_host_scanner_operation_factory_->created_operations()[1]
      ->SendScannedDeviceListUpdate(scanned_device_infos_from_current_scan_,
                                    true /* is_final_scan_result */);
  EXPECT_FALSE(host_scanner_->IsScanActive());

  ClearCurrentScanResults();
  VerifyScanResultsMatchCache();
}

}  // namespace tether

}  // namespace chromeos
