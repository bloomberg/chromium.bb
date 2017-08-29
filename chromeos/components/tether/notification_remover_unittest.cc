// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/notification_remover.h"

#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/components/tether/fake_active_host.h"
#include "chromeos/components/tether/fake_host_scan_cache.h"
#include "chromeos/components/tether/fake_notification_presenter.h"
#include "chromeos/components/tether/host_scan_test_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_state_test.h"
#include "components/cryptauth/remote_device.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace tether {

class NotificationRemoverTest : public NetworkStateTest {
 protected:
  NotificationRemoverTest()
      : NetworkStateTest(),
        test_entries_(host_scan_test_util::CreateTestEntries()) {}

  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();

    notification_presenter_ = base::MakeUnique<FakeNotificationPresenter>();
    host_scan_cache_ = base::MakeUnique<FakeHostScanCache>();
    active_host_ = base::MakeUnique<FakeActiveHost>();

    notification_remover_ = base::MakeUnique<NotificationRemover>(
        network_state_handler(), notification_presenter_.get(),
        host_scan_cache_.get(), active_host_.get());
  }

  void TearDown() override {
    notification_remover_.reset();

    NetworkStateTest::TearDown();
    DBusThreadManager::Shutdown();
  }

  void NotifyPotentialHotspotNearby() {
    cryptauth::RemoteDevice remote_device;
    notification_presenter_->NotifyPotentialHotspotNearby(
        remote_device, 100 /* signal_strength */);
  }

  void SetAndRemoveHostScanResult() {
    host_scan_cache_->SetHostScanResult(
        test_entries_.at(host_scan_test_util::kTetherGuid0));
    EXPECT_FALSE(host_scan_cache_->empty());

    host_scan_cache_->RemoveHostScanResult(host_scan_test_util::kTetherGuid0);
    EXPECT_TRUE(host_scan_cache_->empty());
  }

  void StartConnectingToWifiNetwork() {
    std::stringstream ss;
    ss << "{"
       << "  \"GUID\": \"wifiNetworkGuid\","
       << "  \"Type\": \"" << shill::kTypeWifi << "\","
       << "  \"State\": \"" << shill::kStateConfiguration << "\""
       << "}";

    ConfigureService(ss.str());
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  const std::unordered_map<std::string, HostScanCacheEntry> test_entries_;

  std::unique_ptr<FakeNotificationPresenter> notification_presenter_;
  std::unique_ptr<FakeHostScanCache> host_scan_cache_;
  std::unique_ptr<FakeActiveHost> active_host_;
  std::unique_ptr<NotificationRemover> notification_remover_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationRemoverTest);
};

TEST_F(NotificationRemoverTest, TestCacheBecameEmpty) {
  NotifyPotentialHotspotNearby();
  SetAndRemoveHostScanResult();
  EXPECT_EQ(NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN,
            notification_presenter_->GetPotentialHotspotNotificationState());

  notification_presenter_->NotifyMultiplePotentialHotspotsNearby();
  SetAndRemoveHostScanResult();
  EXPECT_EQ(NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN,
            notification_presenter_->GetPotentialHotspotNotificationState());
}

TEST_F(NotificationRemoverTest, TestStartConnectingToWifiNetwork) {
  NotifyPotentialHotspotNearby();

  StartConnectingToWifiNetwork();
  EXPECT_EQ(NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN,
            notification_presenter_->GetPotentialHotspotNotificationState());
}

TEST_F(NotificationRemoverTest, TestTetherDisabled) {
  NotifyPotentialHotspotNearby();
  notification_presenter_->NotifySetupRequired("testDevice");
  notification_presenter_->NotifyConnectionToHostFailed();

  notification_remover_.reset();
  EXPECT_EQ(NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN,
            notification_presenter_->GetPotentialHotspotNotificationState());
  EXPECT_FALSE(notification_presenter_->is_setup_required_notification_shown());
  EXPECT_FALSE(
      notification_presenter_->is_connection_failed_notification_shown());
}

TEST_F(NotificationRemoverTest, TestActiveHostConnecting) {
  NotifyPotentialHotspotNearby();

  active_host_->SetActiveHostDisconnected();
  EXPECT_EQ(NotificationPresenter::PotentialHotspotNotificationState::
                SINGLE_HOTSPOT_NEARBY_SHOWN,
            notification_presenter_->GetPotentialHotspotNotificationState());

  active_host_->SetActiveHostConnecting("testDeviceId",
                                        host_scan_test_util::kTetherGuid0);
  EXPECT_EQ(NotificationPresenter::PotentialHotspotNotificationState::
                NO_HOTSPOT_NOTIFICATION_SHOWN,
            notification_presenter_->GetPotentialHotspotNotificationState());
}

}  // namespace tether

}  // namespace chromeos
