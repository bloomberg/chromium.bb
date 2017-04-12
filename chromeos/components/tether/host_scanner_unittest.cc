// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scanner.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/fake_notification_presenter.h"
#include "chromeos/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/components/tether/host_scan_device_prioritizer.h"
#include "chromeos/components/tether/host_scanner.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

class FakeHostScanDevicePrioritizer : public HostScanDevicePrioritizer {
 public:
  FakeHostScanDevicePrioritizer() : HostScanDevicePrioritizer(nullptr) {}
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
      HostScanDevicePrioritizer* host_scan_device_prioritizer)
      : HostScannerOperation(devices_to_connect,
                             connection_manager,
                             host_scan_device_prioritizer) {}

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
      HostScanDevicePrioritizer* host_scan_device_prioritizer) override {
    EXPECT_EQ(expected_devices_, devices_to_connect);
    FakeHostScannerOperation* operation = new FakeHostScannerOperation(
        devices_to_connect, connection_manager, host_scan_device_prioritizer);
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

DeviceStatus CreateFakeDeviceStatus(const std::string& cell_provider_name) {
  WifiStatus wifi_status;
  wifi_status.set_status_code(
      WifiStatus_StatusCode::WifiStatus_StatusCode_CONNECTED);
  wifi_status.set_ssid("Google A");

  DeviceStatus device_status;
  device_status.set_battery_percentage(75);
  device_status.set_cell_provider(cell_provider_name);
  device_status.set_connection_strength(4);
  device_status.mutable_wifi_status()->CopyFrom(wifi_status);

  return device_status;
}

std::vector<HostScannerOperation::ScannedDeviceInfo>
CreateFakeScannedDeviceInfos(
    const std::vector<cryptauth::RemoteDevice>& remote_devices) {
  std::vector<HostScannerOperation::ScannedDeviceInfo> scanned_device_infos;
  for (size_t i = 0; i < remote_devices.size(); ++i) {
    DeviceStatus device_status = CreateFakeDeviceStatus(
        GenerateCellProviderForDevice(remote_devices[i]));
    // Require set-up for odd-numbered device indices.
    bool set_up_required = i % 2 == 0;
    scanned_device_infos.push_back(HostScannerOperation::ScannedDeviceInfo(
        remote_devices[i], device_status, set_up_required));
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
    NetworkStateTest::SetUp();

    scanned_device_infos_so_far_.clear();

    fake_tether_host_fetcher_ = base::MakeUnique<FakeTetherHostFetcher>(
        test_devices_, false /* synchronously_reply_with_results */);
    fake_ble_connection_manager_ = base::MakeUnique<FakeBleConnectionManager>();
    fake_host_scan_device_prioritizer_ =
        base::MakeUnique<FakeHostScanDevicePrioritizer>();

    fake_host_scanner_operation_factory_ =
        base::WrapUnique(new FakeHostScannerOperationFactory(test_devices_));
    HostScannerOperation::Factory::SetInstanceForTesting(
        fake_host_scanner_operation_factory_.get());

    fake_notification_presenter_ =
        base::MakeUnique<FakeNotificationPresenter>();

    host_scanner_ = base::WrapUnique(new HostScanner(
        fake_tether_host_fetcher_.get(), fake_ble_connection_manager_.get(),
        fake_host_scan_device_prioritizer_.get(), network_state_handler(),
        fake_notification_presenter_.get()));
  }

  void TearDown() override {
    ShutdownNetworkState();
    NetworkStateTest::TearDown();
    DBusThreadManager::Shutdown();

    HostScannerOperation::Factory::SetInstanceForTesting(nullptr);
  }

  // Causes |fake_operation| to receive the scan result in
  // |test_scanned_device_infos| vector at the index |test_device_index| with
  // the "final result" value of |is_final_scan_result|.
  void ReceiveScanResultAndVerifySuccess(
      FakeHostScannerOperation* fake_operation,
      size_t test_device_index,
      bool is_final_scan_result) {
    scanned_device_infos_so_far_.push_back(
        test_scanned_device_infos[test_device_index]);
    fake_operation->SendScannedDeviceListUpdate(scanned_device_infos_so_far_,
                                                is_final_scan_result);
    EXPECT_EQ(scanned_device_infos_so_far_,
              host_scanner_->most_recent_scan_results());

    NetworkStateHandler::NetworkStateList tether_networks;
    network_state_handler()->GetTetherNetworkList(0 /* no limit */,
                                                  &tether_networks);
    EXPECT_EQ(scanned_device_infos_so_far_.size(), tether_networks.size());
    for (auto& scanned_device_info : scanned_device_infos_so_far_) {
      cryptauth::RemoteDevice remote_device = scanned_device_info.remote_device;
      const NetworkState* tether_network =
          network_state_handler()->GetNetworkStateFromGuid(
              remote_device.GetDeviceId());
      ASSERT_TRUE(tether_network);
      EXPECT_EQ(remote_device.name, tether_network->name());
    }

    if (scanned_device_infos_so_far_.size() == 1) {
      EXPECT_EQ(FakeNotificationPresenter::PotentialHotspotNotificationState::
                    SINGLE_HOTSPOT_NEARBY_SHOWN,
                fake_notification_presenter_->potential_hotspot_state());
    } else {
      EXPECT_EQ(FakeNotificationPresenter::PotentialHotspotNotificationState::
                    MULTIPLE_HOTSPOTS_NEARBY_SHOWN,
                fake_notification_presenter_->potential_hotspot_state());
    }
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  const std::vector<cryptauth::RemoteDevice> test_devices_;
  const std::vector<HostScannerOperation::ScannedDeviceInfo>
      test_scanned_device_infos;

  std::unique_ptr<FakeTetherHostFetcher> fake_tether_host_fetcher_;
  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<HostScanDevicePrioritizer> fake_host_scan_device_prioritizer_;

  std::unique_ptr<FakeHostScannerOperationFactory>
      fake_host_scanner_operation_factory_;

  std::vector<HostScannerOperation::ScannedDeviceInfo>
      scanned_device_infos_so_far_;

  std::unique_ptr<HostScanner> host_scanner_;

  std::unique_ptr<FakeNotificationPresenter> fake_notification_presenter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostScannerTest);
};

TEST_F(HostScannerTest, TestScan_ResultsFromAllDevices) {
  EXPECT_FALSE(host_scanner_->IsScanActive());
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  fake_tether_host_fetcher_->InvokePendingCallbacks();
  ASSERT_EQ(1u,
            fake_host_scanner_operation_factory_->created_operations().size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  ReceiveScanResultAndVerifySuccess(
      fake_host_scanner_operation_factory_->created_operations()[0],
      0u /* test_device_index */, false /* is_final_scan_result */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_host_scanner_operation_factory_->created_operations()[0],
      1u /* test_device_index */, false /* is_final_scan_result */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_host_scanner_operation_factory_->created_operations()[0],
      2u /* test_device_index */, false /* is_final_scan_result */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_host_scanner_operation_factory_->created_operations()[0],
      3u /* test_device_index */, true /* is_final_scan_result */);
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
  EXPECT_TRUE(host_scanner_->most_recent_scan_results().empty());
  EXPECT_FALSE(host_scanner_->IsScanActive());
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
      fake_host_scanner_operation_factory_->created_operations()[0],
      0u /* test_device_index */, false /* is_final_scan_result */);
  EXPECT_TRUE(host_scanner_->IsScanActive());
  ReceiveScanResultAndVerifySuccess(
      fake_host_scanner_operation_factory_->created_operations()[0],
      1u /* test_device_index */, false /* is_final_scan_result */);
  EXPECT_TRUE(host_scanner_->IsScanActive());

  fake_host_scanner_operation_factory_->created_operations()[0]
      ->SendScannedDeviceListUpdate(scanned_device_infos_so_far_,
                                    true /* is_final_scan_result */);
  EXPECT_EQ(scanned_device_infos_so_far_,
            host_scanner_->most_recent_scan_results());
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
  EXPECT_EQ(std::vector<HostScannerOperation::ScannedDeviceInfo>(),
            host_scanner_->most_recent_scan_results());

  fake_tether_host_fetcher_->InvokePendingCallbacks();
  ASSERT_EQ(1u,
            fake_host_scanner_operation_factory_->created_operations().size());
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // Call StartScan again after the tether host fetcher has finished but before
  // the final scan result has been received. This should be a no-op.
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // No devices should have been received yet.
  EXPECT_EQ(std::vector<HostScannerOperation::ScannedDeviceInfo>(),
            host_scanner_->most_recent_scan_results());

  // Receive updates from the 0th device.
  ReceiveScanResultAndVerifySuccess(
      fake_host_scanner_operation_factory_->created_operations()[0],
      0u /* test_device_index */, false /* is_final_scan_result */);
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // Call StartScan again after a scan result has been received but before
  // the final scan result ha been received. This should be a no-op.
  host_scanner_->StartScan();
  EXPECT_TRUE(host_scanner_->IsScanActive());

  // No devices should have been received yet.
  EXPECT_EQ(scanned_device_infos_so_far_,
            host_scanner_->most_recent_scan_results());

  // Finally, finish the scan.
  fake_host_scanner_operation_factory_->created_operations()[0]
      ->SendScannedDeviceListUpdate(scanned_device_infos_so_far_,
                                    true /* is_final_scan_result */);
  EXPECT_EQ(scanned_device_infos_so_far_,
            host_scanner_->most_recent_scan_results());
  EXPECT_FALSE(host_scanner_->IsScanActive());
}

}  // namespace tether

}  // namespace chromeos
