// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/active_host.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/tether/fake_tether_host_fetcher.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

struct GetActiveHostResult {
  ActiveHost::ActiveHostStatus active_host_status;
  std::shared_ptr<cryptauth::RemoteDevice> remote_device;
  std::string tether_network_guid;
  std::string wifi_network_guid;

  bool operator==(const GetActiveHostResult& other) const {
    bool devices_equal;
    if (remote_device) {
      devices_equal =
          other.remote_device && *remote_device == *other.remote_device;
    } else {
      devices_equal = !other.remote_device;
    }

    return active_host_status == other.active_host_status && devices_equal &&
           tether_network_guid == other.tether_network_guid &&
           wifi_network_guid == other.wifi_network_guid;
  }
};

class TestObserver : public ActiveHost::Observer {
 public:
  void OnActiveHostChanged(
      const ActiveHost::ActiveHostChangeInfo& change_info) override {
    host_changed_updates_.push_back(change_info);
  }

  std::vector<ActiveHost::ActiveHostChangeInfo>& host_changed_updates() {
    return host_changed_updates_;
  }

 private:
  std::vector<ActiveHost::ActiveHostChangeInfo> host_changed_updates_;
};

}  // namespace

class ActiveHostTest : public testing::Test {
 public:
  ActiveHostTest() : test_devices_(cryptauth::GenerateTestRemoteDevices(4)) {}

  void SetUp() override {
    get_active_host_results_.clear();

    test_pref_service_ = base::MakeUnique<TestingPrefServiceSimple>();
    fake_tether_host_fetcher_ = base::MakeUnique<FakeTetherHostFetcher>(
        test_devices_, false /* synchronously_reply_with_results */);

    ActiveHost::RegisterPrefs(test_pref_service_->registry());
    active_host_ = base::MakeUnique<ActiveHost>(fake_tether_host_fetcher_.get(),
                                                test_pref_service_.get());

    test_observer_ = base::WrapUnique(new TestObserver);
    active_host_->AddObserver(test_observer_.get());
  }

  void OnActiveHostFetched(ActiveHost::ActiveHostStatus active_host_status,
                           std::shared_ptr<cryptauth::RemoteDevice> active_host,
                           const std::string& tether_network_guid,
                           const std::string& wifi_network_guid) {
    get_active_host_results_.push_back(
        GetActiveHostResult{active_host_status, active_host,
                            tether_network_guid, wifi_network_guid});
  }

  void VerifyActiveHostDisconnected() {
    EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
              active_host_->GetActiveHostStatus());
    EXPECT_TRUE(active_host_->GetActiveHostDeviceId().empty());
    EXPECT_TRUE(active_host_->GetWifiNetworkGuid().empty());
    EXPECT_TRUE(active_host_->GetTetherNetworkGuid().empty());

    active_host_->GetActiveHost(base::Bind(&ActiveHostTest::OnActiveHostFetched,
                                           base::Unretained(this)));
    fake_tether_host_fetcher_->InvokePendingCallbacks();
    ASSERT_EQ(1u, get_active_host_results_.size());
    EXPECT_EQ((GetActiveHostResult{ActiveHost::ActiveHostStatus::DISCONNECTED,
                                   nullptr, std::string(), std::string()}),
              get_active_host_results_[0]);
  }

  const std::vector<cryptauth::RemoteDevice> test_devices_;

  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
  std::unique_ptr<FakeTetherHostFetcher> fake_tether_host_fetcher_;
  std::unique_ptr<TestObserver> test_observer_;

  std::vector<GetActiveHostResult> get_active_host_results_;

  std::unique_ptr<ActiveHost> active_host_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ActiveHostTest);
};

TEST_F(ActiveHostTest, TestDefaultValues) {
  VerifyActiveHostDisconnected();
}

TEST_F(ActiveHostTest, TestDisconnected) {
  active_host_->SetActiveHostDisconnected();
  VerifyActiveHostDisconnected();
}

TEST_F(ActiveHostTest, TestConnecting) {
  active_host_->SetActiveHostConnecting(test_devices_[0].GetDeviceId(),
                                        "tetherNetworkGuid");

  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            active_host_->GetActiveHostDeviceId());
  EXPECT_EQ("tetherNetworkGuid", active_host_->GetTetherNetworkGuid());
  EXPECT_TRUE(active_host_->GetWifiNetworkGuid().empty());

  active_host_->GetActiveHost(
      base::Bind(&ActiveHostTest::OnActiveHostFetched, base::Unretained(this)));
  fake_tether_host_fetcher_->InvokePendingCallbacks();
  ASSERT_EQ(1u, get_active_host_results_.size());
  EXPECT_EQ(
      (GetActiveHostResult{ActiveHost::ActiveHostStatus::CONNECTING,
                           std::shared_ptr<cryptauth::RemoteDevice>(
                               new cryptauth::RemoteDevice(test_devices_[0])),
                           "tetherNetworkGuid", std::string()}),
      get_active_host_results_[0]);
}

TEST_F(ActiveHostTest, TestConnected) {
  active_host_->SetActiveHostConnected(test_devices_[0].GetDeviceId(),
                                       "tetherNetworkGuid", "wifiNetworkGuid");

  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTED,
            active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            active_host_->GetActiveHostDeviceId());
  EXPECT_EQ("tetherNetworkGuid", active_host_->GetTetherNetworkGuid());
  EXPECT_EQ("wifiNetworkGuid", active_host_->GetWifiNetworkGuid());

  active_host_->GetActiveHost(
      base::Bind(&ActiveHostTest::OnActiveHostFetched, base::Unretained(this)));
  fake_tether_host_fetcher_->InvokePendingCallbacks();
  ASSERT_EQ(1u, get_active_host_results_.size());
  EXPECT_EQ(
      (GetActiveHostResult{ActiveHost::ActiveHostStatus::CONNECTED,
                           std::shared_ptr<cryptauth::RemoteDevice>(
                               new cryptauth::RemoteDevice(test_devices_[0])),
                           "tetherNetworkGuid", "wifiNetworkGuid"}),
      get_active_host_results_[0]);
}

TEST_F(ActiveHostTest, TestActiveHostChangesDuringFetch) {
  active_host_->SetActiveHostConnected(test_devices_[0].GetDeviceId(),
                                       "tetherNetworkGuid", "wifiNetworkGuid");

  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTED,
            active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            active_host_->GetActiveHostDeviceId());
  EXPECT_EQ("tetherNetworkGuid", active_host_->GetTetherNetworkGuid());
  EXPECT_EQ("wifiNetworkGuid", active_host_->GetWifiNetworkGuid());

  // Start a fetch for the active host.
  active_host_->GetActiveHost(
      base::Bind(&ActiveHostTest::OnActiveHostFetched, base::Unretained(this)));

  // While the fetch is in progress, simulate a disconnection occurring.
  active_host_->SetActiveHostDisconnected();

  // Now, finish the fech.
  fake_tether_host_fetcher_->InvokePendingCallbacks();

  // The resulting callback should indicate that there is no active host.
  ASSERT_EQ(1u, get_active_host_results_.size());
  EXPECT_EQ((GetActiveHostResult{ActiveHost::ActiveHostStatus::DISCONNECTED,
                                 nullptr, "" /* tether_network_guid */,
                                 "" /* wifi_network_guid */}),
            get_active_host_results_[0]);
}

TEST_F(ActiveHostTest, TestObserverCalls) {
  // Start as DISCONNECTED.
  EXPECT_FALSE(test_observer_->host_changed_updates().size());

  // Go to DISCONNECTED again. This should not cause an observer callback to be
  // invoked.
  active_host_->SetActiveHostDisconnected();
  fake_tether_host_fetcher_->InvokePendingCallbacks();
  EXPECT_FALSE(test_observer_->host_changed_updates().size());

  // Transition to CONNECTING.
  active_host_->SetActiveHostConnecting(test_devices_[0].GetDeviceId(),
                                        "tetherNetworkGuid");
  fake_tether_host_fetcher_->InvokePendingCallbacks();
  EXPECT_EQ(1u, test_observer_->host_changed_updates().size());
  EXPECT_EQ(ActiveHost::ActiveHostChangeInfo(
                ActiveHost::ActiveHostStatus::CONNECTING,
                ActiveHost::ActiveHostStatus::DISCONNECTED,
                std::shared_ptr<cryptauth::RemoteDevice>(
                    new cryptauth::RemoteDevice(test_devices_[0])),
                "" /* old_active_host_id */,
                "tetherNetworkGuid" /* new_tether_network_guid */,
                "" /* old_tether_network_id */, "" /* new_wifi_network_guid */,
                "" /* old_wifi_network_guid */),
            test_observer_->host_changed_updates()[0]);

  // Transition to CONNECTED.
  active_host_->SetActiveHostConnected(test_devices_[0].GetDeviceId(),
                                       "tetherNetworkGuid", "wifiNetworkGuid");
  fake_tether_host_fetcher_->InvokePendingCallbacks();
  EXPECT_EQ(2u, test_observer_->host_changed_updates().size());
  EXPECT_EQ(ActiveHost::ActiveHostChangeInfo(
                ActiveHost::ActiveHostStatus::CONNECTED,
                ActiveHost::ActiveHostStatus::CONNECTING,
                std::shared_ptr<cryptauth::RemoteDevice>(
                    new cryptauth::RemoteDevice(test_devices_[0])),
                test_devices_[0].GetDeviceId(), "tetherNetworkGuid",
                "tetherNetworkGuid", "wifiNetworkGuid",
                "" /* old_wifi_network_guid */),
            test_observer_->host_changed_updates()[1]);

  // Transition to DISCONNECTED.
  active_host_->SetActiveHostDisconnected();
  fake_tether_host_fetcher_->InvokePendingCallbacks();
  EXPECT_EQ(3u, test_observer_->host_changed_updates().size());
  EXPECT_EQ(ActiveHost::ActiveHostChangeInfo(
                ActiveHost::ActiveHostStatus::DISCONNECTED,
                ActiveHost::ActiveHostStatus::CONNECTED, nullptr,
                test_devices_[0].GetDeviceId(),
                "" /* new_tether_network_guid */, "tetherNetworkGuid",
                "" /* new_wifi_network_guid */, "wifiNetworkGuid"),
            test_observer_->host_changed_updates()[2]);
}

}  // namespace tether

}  // namespace cryptauth
