// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scan_scheduler.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
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
                    nullptr),
        num_scans_started_(0) {}
  ~FakeHostScanner() override {}

  void StartScan() override {
    is_scan_active_ = true;
    num_scans_started_++;
  }

  void StopScan() { is_scan_active_ = false; }

  bool IsScanActive() override { return is_scan_active_; }

  int num_scans_started() { return num_scans_started_; }

 private:
  bool is_scan_active_ = false;
  int num_scans_started_ = 0;
};

const char kWifiServiceGuid[] = "wifiServiceGuid";

std::string CreateConfigurationJsonString() {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << kWifiServiceGuid << "\","
     << "  \"Type\": \"" << shill::kTypeWifi << "\","
     << "  \"State\": \"" << shill::kStateReady << "\""
     << "}";
  return ss.str();
}

}  // namespace

class HostScanSchedulerTest : public NetworkStateTest {
 protected:
  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();

    wifi_service_path_ = ConfigureService(CreateConfigurationJsonString());
    test_manager_client()->SetManagerProperty(shill::kDefaultServiceProperty,
                                              base::Value(wifi_service_path_));

    network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TECHNOLOGY_ENABLED);

    fake_host_scanner_ = base::MakeUnique<FakeHostScanner>();

    host_scan_scheduler_ = base::WrapUnique(new HostScanScheduler(
        network_state_handler(), fake_host_scanner_.get()));
  }

  void TearDown() override {
    host_scan_scheduler_.reset();

    ShutdownNetworkState();
    NetworkStateTest::TearDown();
    DBusThreadManager::Shutdown();
  }

  void SetDefaultNetworkDisconnected() {
    SetServiceProperty(wifi_service_path_, std::string(shill::kStateProperty),
                       base::Value(shill::kStateIdle));
  }

  void SetDefaultNetworkConnecting() {
    SetServiceProperty(wifi_service_path_, std::string(shill::kStateProperty),
                       base::Value(shill::kStateAssociation));
  }

  void SetDefaultNetworkConnected() {
    SetServiceProperty(wifi_service_path_, std::string(shill::kStateProperty),
                       base::Value(shill::kStateReady));
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::string wifi_service_path_;

  std::unique_ptr<FakeHostScanner> fake_host_scanner_;

  std::unique_ptr<HostScanScheduler> host_scan_scheduler_;
};

TEST_F(HostScanSchedulerTest, UserLoggedIn) {
  EXPECT_EQ(0, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  host_scan_scheduler_->UserLoggedIn();

  EXPECT_EQ(0, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetDefaultNetworkDisconnected();
  host_scan_scheduler_->UserLoggedIn();

  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));
}

TEST_F(HostScanSchedulerTest, ScanRequested) {
  EXPECT_EQ(0, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  // Begin scanning.
  host_scan_scheduler_->ScanRequested();
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  // Should not begin a new scan while a scan is active.
  host_scan_scheduler_->ScanRequested();
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  fake_host_scanner_->StopScan();
  host_scan_scheduler_->ScanFinished();
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  // A new scan should be allowed once a scan is not active.
  host_scan_scheduler_->ScanRequested();
  EXPECT_EQ(2, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));
}

TEST_F(HostScanSchedulerTest, DefaultNetworkChanged) {
  EXPECT_EQ(0, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetDefaultNetworkConnecting();

  EXPECT_EQ(0, fake_host_scanner_->num_scans_started());
  EXPECT_FALSE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetDefaultNetworkDisconnected();

  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetDefaultNetworkConnecting();

  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));

  SetDefaultNetworkDisconnected();

  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
  EXPECT_TRUE(
      network_state_handler()->GetScanningByType(NetworkTypePattern::Tether()));
}

}  // namespace tether

}  // namespace chromeos
