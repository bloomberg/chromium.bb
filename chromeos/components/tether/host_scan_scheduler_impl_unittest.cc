// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scan_scheduler_impl.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/timer/mock_timer.h"
#include "chromeos/components/tether/host_scanner.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

class FakeHostScanner : public HostScanner {
 public:
  FakeHostScanner()
      : HostScanner(nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr),
        num_scans_started_(0) {}
  ~FakeHostScanner() override {}

  void StartScan() override {
    is_scan_active_ = true;
    num_scans_started_++;
  }

  void StopScan() {
    is_scan_active_ = false;
    NotifyScanFinished();
  }

  bool IsScanActive() override { return is_scan_active_; }

  int num_scans_started() { return num_scans_started_; }

 private:
  bool is_scan_active_ = false;
  int num_scans_started_ = 0;
};

const char kEthernetServiceGuid[] = "ethernetServiceGuid";
const char kWifiServiceGuid[] = "wifiServiceGuid";
const char kTetherGuid[] = "tetherGuid";

std::string CreateConfigurationJsonString(const std::string& guid,
                                          const std::string& type) {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << guid << "\","
     << "  \"Type\": \"" << type << "\","
     << "  \"State\": \"" << shill::kStateReady << "\""
     << "}";
  return ss.str();
}

}  // namespace

class HostScanSchedulerImplTest : public NetworkStateTest {
 protected:
  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();

    histogram_tester_ = base::MakeUnique<base::HistogramTester>();

    ethernet_service_path_ = ConfigureService(CreateConfigurationJsonString(
        kEthernetServiceGuid, shill::kTypeEthernet));
    test_manager_client()->SetManagerProperty(
        shill::kDefaultServiceProperty, base::Value(ethernet_service_path_));

    network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TECHNOLOGY_ENABLED);

    fake_host_scanner_ = base::MakeUnique<FakeHostScanner>();

    host_scan_scheduler_ = base::MakeUnique<HostScanSchedulerImpl>(
        network_state_handler(), fake_host_scanner_.get());

    mock_timer_ = new base::MockTimer(true /* retain_user_task */,
                                      false /* is_repeating */);
    test_clock_ = new base::SimpleTestClock();
    // Advance the clock by an arbitrary value to ensure that when Now() is
    // called, the Unix epoch will not be returned.
    test_clock_->Advance(base::TimeDelta::FromSeconds(10));
    host_scan_scheduler_->SetTestDoubles(base::WrapUnique(mock_timer_),
                                         base::WrapUnique(test_clock_));
  }

  void TearDown() override {
    host_scan_scheduler_.reset();

    ShutdownNetworkState();
    NetworkStateTest::TearDown();
    DBusThreadManager::Shutdown();
  }

  void RequestScan() { host_scan_scheduler_->ScanRequested(); }

  // Disconnects the Ethernet network and manually sets the default network to
  // |new_default_service_path|. If |new_default_service_path| is empty then no
  // default network is set.
  void SetEthernetNetworkDisconnected(
      const std::string& new_default_service_path) {
    SetServiceProperty(ethernet_service_path_,
                       std::string(shill::kStateProperty),
                       base::Value(shill::kStateIdle));
    if (new_default_service_path.empty())
      return;

    test_manager_client()->SetManagerProperty(
        shill::kDefaultServiceProperty, base::Value(new_default_service_path));
  }

  void SetEthernetNetworkConnecting() {
    SetServiceProperty(ethernet_service_path_,
                       std::string(shill::kStateProperty),
                       base::Value(shill::kStateAssociation));
    test_manager_client()->SetManagerProperty(
        shill::kDefaultServiceProperty, base::Value(ethernet_service_path_));
  }

  void SetEthernetNetworkConnected() {
    SetServiceProperty(ethernet_service_path_,
                       std::string(shill::kStateProperty),
                       base::Value(shill::kStateReady));
    test_manager_client()->SetManagerProperty(
        shill::kDefaultServiceProperty, base::Value(ethernet_service_path_));
  }

  // Adds a Tether network state, adds a Wifi network to be used as the Wifi
  // hotspot, and associates the two networks. Returns the service path of the
  // Wifi network.
  std::string AddTetherNetworkState() {
    network_state_handler()->AddTetherNetworkState(
        kTetherGuid, "name", "carrier", 100 /* battery_percentage */,
        100 /* signal strength */, false /* has_connected_to_host */);
    std::string wifi_service_path = ConfigureService(
        CreateConfigurationJsonString(kWifiServiceGuid, shill::kTypeWifi));
    network_state_handler()->AssociateTetherNetworkStateWithWifiNetwork(
        kTetherGuid, kWifiServiceGuid);
    return wifi_service_path;
  }

  void VerifyScanDuration(size_t expected_num_seconds) {
    histogram_tester_->ExpectTimeBucketCount(
        "InstantTethering.HostScanBatchDuration",
        base::TimeDelta::FromSeconds(expected_num_seconds), 1u);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::string ethernet_service_path_;

  std::unique_ptr<FakeHostScanner> fake_host_scanner_;

  base::MockTimer* mock_timer_;
  base::SimpleTestClock* test_clock_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;

  std::unique_ptr<HostScanSchedulerImpl> host_scan_scheduler_;
};

TEST_F(HostScanSchedulerImplTest, ScheduleScan) {
  host_scan_scheduler_->ScheduleScan();
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  test_clock_->Advance(base::TimeDelta::FromSeconds(5));
  fake_host_scanner_->StopScan();
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  // Fire the timer; the duration should be recorded.
  mock_timer_->Fire();
  VerifyScanDuration(5u /* expected_num_sections */);
}

TEST_F(HostScanSchedulerImplTest, ScanRequested) {
  // Begin scanning.
  RequestScan();
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  // Should not begin a new scan while a scan is active.
  RequestScan();
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  test_clock_->Advance(base::TimeDelta::FromSeconds(5));
  fake_host_scanner_->StopScan();
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));
  mock_timer_->Fire();
  VerifyScanDuration(5u /* expected_num_sections */);

  // A new scan should be allowed once a scan is not active.
  RequestScan();
  EXPECT_EQ(2, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));
}

TEST_F(HostScanSchedulerImplTest, HostScanSchedulerDestroyed) {
  host_scan_scheduler_->ScheduleScan();
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  test_clock_->Advance(base::TimeDelta::FromSeconds(5));

  // Delete |host_scan_scheduler_|, which should cause the metric to be logged.
  host_scan_scheduler_.reset();
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));
  VerifyScanDuration(5u /* expected_num_sections */);
}

TEST_F(HostScanSchedulerImplTest, HostScanBatchMetric) {
  // The first scan takes 5 seconds. After stopping, the timer should be
  // running.
  host_scan_scheduler_->ScheduleScan();
  test_clock_->Advance(base::TimeDelta::FromSeconds(5));
  fake_host_scanner_->StopScan();
  EXPECT_TRUE(mock_timer_->IsRunning());

  // Advance the clock by 1 second and start another scan. The timer should have
  // been stopped.
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_LT(base::TimeDelta::FromSeconds(1), mock_timer_->GetCurrentDelay());
  host_scan_scheduler_->ScheduleScan();
  EXPECT_FALSE(mock_timer_->IsRunning());

  // Stop the scan; the duration should not have been recorded, and the timer
  // should be running again.
  test_clock_->Advance(base::TimeDelta::FromSeconds(5));
  fake_host_scanner_->StopScan();
  EXPECT_TRUE(mock_timer_->IsRunning());

  // Advance the clock by 59 seconds and start another scan. The timer should
  // have been stopped.
  test_clock_->Advance(base::TimeDelta::FromSeconds(59));
  EXPECT_LT(base::TimeDelta::FromSeconds(59), mock_timer_->GetCurrentDelay());
  host_scan_scheduler_->ScheduleScan();
  EXPECT_FALSE(mock_timer_->IsRunning());

  // Stop the scan; the duration should not have been recorded, and the timer
  // should be running again.
  test_clock_->Advance(base::TimeDelta::FromSeconds(5));
  fake_host_scanner_->StopScan();
  EXPECT_TRUE(mock_timer_->IsRunning());

  // Advance the clock by 60 seconds, which should be equal to the timer's
  // delay. Since this is a MockTimer, we need to manually fire the timer.
  test_clock_->Advance(base::TimeDelta::FromSeconds(60));
  EXPECT_EQ(base::TimeDelta::FromSeconds(60), mock_timer_->GetCurrentDelay());
  mock_timer_->Fire();

  // The scan duration should be equal to the three 5-second scans as well as
  // the 1-second and 59-second breaks between the three scans.
  VerifyScanDuration(5u + 1u + 5u + 59u + 5u /* expected_num_sections */);

  // Now, start a new 5-second scan, then wait for the timer to fire. A new
  // batch duration should have been logged to metrics.
  host_scan_scheduler_->ScheduleScan();
  test_clock_->Advance(base::TimeDelta::FromSeconds(5));
  fake_host_scanner_->StopScan();
  EXPECT_TRUE(mock_timer_->IsRunning());
  test_clock_->Advance(base::TimeDelta::FromSeconds(60));
  EXPECT_EQ(base::TimeDelta::FromSeconds(60), mock_timer_->GetCurrentDelay());
  mock_timer_->Fire();
  VerifyScanDuration(5u /* expected_num_sections */);
}

TEST_F(HostScanSchedulerImplTest, DefaultNetworkChanged) {
  // When no Tether network is present, a scan should start when the default
  // network is disconnected.
  SetEthernetNetworkConnecting();
  EXPECT_EQ(0, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkConnected();
  EXPECT_EQ(0, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkDisconnected(std::string() /* default_service_path */);
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  fake_host_scanner_->StopScan();

  // When Tether is present but disconnected, a scan should start when the
  // default network is disconnected.
  std::string tether_service_path = AddTetherNetworkState();
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkConnecting();
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkConnected();
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkDisconnected(std::string() /* default_service_path */);
  EXPECT_EQ(2, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  fake_host_scanner_->StopScan();

  // When Tether is present and connecting, no scan should start when an
  // Ethernet network becomes the default network and then disconnects.
  network_state_handler()->SetTetherNetworkStateConnecting(kTetherGuid);

  EXPECT_EQ(2, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkConnecting();
  EXPECT_EQ(2, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkConnected();
  EXPECT_EQ(2, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkDisconnected(tether_service_path);
  EXPECT_EQ(2, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  fake_host_scanner_->StopScan();

  // When Tether is present and connected, no scan should start when an Ethernet
  // network becomes the default network and then disconnects.
  base::RunLoop().RunUntilIdle();
  network_state_handler()->SetTetherNetworkStateConnected(kTetherGuid);

  EXPECT_EQ(2, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkConnecting();
  EXPECT_EQ(2, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkConnected();
  EXPECT_EQ(2, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetEthernetNetworkDisconnected(tether_service_path);
  EXPECT_EQ(2, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));
}

}  // namespace tether

}  // namespace chromeos
