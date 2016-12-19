// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scan_scheduler.h"

#include "base/memory/ptr_util.h"
#include "chromeos/components/tether/host_scanner.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

class HostScanSchedulerTest : public testing::Test {
 protected:
  class TestDelegate : public HostScanScheduler::Delegate {
   public:
    TestDelegate(HostScanSchedulerTest* test)
        : test_(test),
          observer_(nullptr),
          is_authenticated_user_logged_in(true),
          is_network_connected_or_connecting(false),
          are_tether_hosts_synced(true) {}

    void AddObserver(HostScanScheduler* host_scan_scheduler) override {
      observer_ = host_scan_scheduler;
    }

    void RemoveObserver(HostScanScheduler* host_scan_scheduler) override {
      if (observer_ == host_scan_scheduler) {
        observer_ = nullptr;
      }
      EXPECT_FALSE(IsObserverSet());
    }

    bool IsAuthenticatedUserLoggedIn() const override {
      return is_authenticated_user_logged_in;
    }

    bool IsNetworkConnectedOrConnecting() const override {
      return is_network_connected_or_connecting;
    }

    bool AreTetherHostsSynced() const override {
      return are_tether_hosts_synced;
    }

    bool IsObserverSet() const {
      return observer_ != nullptr &&
             observer_ == test_->host_scan_scheduler_.get();
    }

    void set_is_authenticated_user_logged_in(bool value) {
      is_authenticated_user_logged_in = value;
    }

    void set_is_network_connected_or_connecting(bool value) {
      is_network_connected_or_connecting = value;
    }

    void set_are_tether_hosts_synced(bool value) {
      are_tether_hosts_synced = value;
    }

   private:
    const HostScanSchedulerTest* test_;

    HostScanScheduler* observer_;
    bool is_authenticated_user_logged_in;
    bool is_network_connected_or_connecting;
    bool are_tether_hosts_synced;
  };

  class FakeHostScanner : public HostScanner {
   public:
    FakeHostScanner() : num_scans_started_(0) {}
    ~FakeHostScanner() override {}

    void StartScan() override { num_scans_started_++; }

    int num_scans_started() { return num_scans_started_; }

   private:
    int num_scans_started_;
  };

  HostScanSchedulerTest() {}

  void SetUp() override {
    test_delegate_ = new TestDelegate(this);
    fake_host_scanner_ = new FakeHostScanner();

    host_scan_scheduler_.reset(
        new HostScanScheduler(base::WrapUnique(test_delegate_),
                              base::WrapUnique(fake_host_scanner_)));

    EXPECT_FALSE(test_delegate_->IsObserverSet());
    host_scan_scheduler_->InitializeAutomaticScans();
    EXPECT_TRUE(test_delegate_->IsObserverSet());
  }

  void TearDown() override {
    EXPECT_TRUE(test_delegate_->IsObserverSet());
    host_scan_scheduler_.reset();
  }

  void TestScheduleScanNowIfPossible(bool is_authenticated_user_logged_in,
                                     bool is_network_connected_or_connecting,
                                     bool are_tether_hosts_synced,
                                     int num_expected_scans) {
    test_delegate_->set_is_authenticated_user_logged_in(
        is_authenticated_user_logged_in);
    test_delegate_->set_is_network_connected_or_connecting(
        is_network_connected_or_connecting);
    test_delegate_->set_are_tether_hosts_synced(are_tether_hosts_synced);
    host_scan_scheduler_->ScheduleScanNowIfPossible();
    EXPECT_EQ(num_expected_scans, fake_host_scanner_->num_scans_started());
  }

  std::unique_ptr<HostScanScheduler> host_scan_scheduler_;

  TestDelegate* test_delegate_;
  FakeHostScanner* fake_host_scanner_;
};

TEST_F(HostScanSchedulerTest, TestObserverAddedAndRemoved) {
  // Intentionally empty. This test just ensures that adding/removing observers
  // (in setUp() and tearDown()) works.
}

TEST_F(HostScanSchedulerTest, TestScheduleScanNowIfPossible) {
  EXPECT_EQ(0, fake_host_scanner_->num_scans_started());

  // A scan should only be started when an authenticated user is logged in,
  // the network is not connected/connecting, and tether hosts are synced.
  TestScheduleScanNowIfPossible(false, false, false, 0);
  TestScheduleScanNowIfPossible(true, false, false, 0);
  TestScheduleScanNowIfPossible(false, true, false, 0);
  TestScheduleScanNowIfPossible(false, false, true, 0);
  TestScheduleScanNowIfPossible(true, true, false, 0);
  TestScheduleScanNowIfPossible(true, false, true, 1);
  TestScheduleScanNowIfPossible(false, true, true, 1);
  TestScheduleScanNowIfPossible(true, true, true, 1);
}

TEST_F(HostScanSchedulerTest, TestLoggedInStateChange) {
  EXPECT_EQ(0, fake_host_scanner_->num_scans_started());

  test_delegate_->set_is_authenticated_user_logged_in(true);
  test_delegate_->set_is_network_connected_or_connecting(false);
  test_delegate_->set_are_tether_hosts_synced(true);

  host_scan_scheduler_->LoggedInStateChanged();
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());

  // Change a condition so that it should not scan, and re-trigger; there should
  // still be only 1 started scan.
  test_delegate_->set_is_authenticated_user_logged_in(false);
  host_scan_scheduler_->LoggedInStateChanged();
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
}

TEST_F(HostScanSchedulerTest, TestSuspendDone) {
  EXPECT_EQ(0, fake_host_scanner_->num_scans_started());

  test_delegate_->set_is_authenticated_user_logged_in(true);
  test_delegate_->set_is_network_connected_or_connecting(false);
  test_delegate_->set_are_tether_hosts_synced(true);

  host_scan_scheduler_->SuspendDone(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());

  // Change a condition so that it should not scan, and re-trigger; there should
  // still be only 1 started scan.
  test_delegate_->set_is_authenticated_user_logged_in(false);
  host_scan_scheduler_->SuspendDone(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
}

TEST_F(HostScanSchedulerTest, TestNetworkConnectionStateChanged) {
  EXPECT_EQ(0, fake_host_scanner_->num_scans_started());

  test_delegate_->set_is_authenticated_user_logged_in(true);
  test_delegate_->set_is_network_connected_or_connecting(false);
  test_delegate_->set_are_tether_hosts_synced(true);

  host_scan_scheduler_->NetworkConnectionStateChanged(nullptr);
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());

  // Change a condition so that it should not scan, and re-trigger; there should
  // still be only 1 started scan.
  test_delegate_->set_is_network_connected_or_connecting(true);
  host_scan_scheduler_->NetworkConnectionStateChanged(nullptr);
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
}

TEST_F(HostScanSchedulerTest, TestOnSyncFinished) {
  EXPECT_EQ(0, fake_host_scanner_->num_scans_started());

  test_delegate_->set_is_authenticated_user_logged_in(true);
  test_delegate_->set_is_network_connected_or_connecting(false);
  test_delegate_->set_are_tether_hosts_synced(true);

  host_scan_scheduler_->OnSyncFinished(
      cryptauth::CryptAuthDeviceManager::SyncResult::SUCCESS,
      cryptauth::CryptAuthDeviceManager::DeviceChangeResult::CHANGED);
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());

  // Change a condition so that it should not scan, and re-trigger; there should
  // still be only 1 started scan.
  test_delegate_->set_are_tether_hosts_synced(false);
  host_scan_scheduler_->OnSyncFinished(
      cryptauth::CryptAuthDeviceManager::SyncResult::SUCCESS,
      cryptauth::CryptAuthDeviceManager::DeviceChangeResult::UNCHANGED);
  EXPECT_EQ(1, fake_host_scanner_->num_scans_started());
}

}  // namespace tether

}  // namespace chromeos
