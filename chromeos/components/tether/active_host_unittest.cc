// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/active_host.h"

#include <memory>

#include "base/bind.h"
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
  std::string wifi_network_id;

  bool operator==(const GetActiveHostResult& other) const {
    bool devices_equal;
    if (remote_device) {
      devices_equal =
          other.remote_device && *remote_device == *other.remote_device;
    } else {
      devices_equal = !other.remote_device;
    }

    return active_host_status == other.active_host_status && devices_equal &&
           wifi_network_id == other.wifi_network_id;
  }
};

class TestObserver : public ActiveHost::Observer {
 public:
  void OnActiveHostChanged(
      ActiveHost::ActiveHostStatus active_host_status,
      std::unique_ptr<cryptauth::RemoteDevice> active_host_device,
      const std::string& wifi_network_id) override {
    host_changed_updates_.push_back(GetActiveHostResult{
        active_host_status, std::move(active_host_device), wifi_network_id});
  }

  std::vector<GetActiveHostResult>& host_changed_updates() {
    return host_changed_updates_;
  }

 private:
  std::vector<GetActiveHostResult> host_changed_updates_;
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
                           std::unique_ptr<cryptauth::RemoteDevice> active_host,
                           const std::string& wifi_network_id) {
    get_active_host_results_.push_back(GetActiveHostResult{
        active_host_status, std::move(active_host), wifi_network_id});
  }

  void VerifyActiveHostDisconnected() {
    EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
              active_host_->GetActiveHostStatus());
    EXPECT_TRUE(active_host_->GetActiveHostDeviceId().empty());
    EXPECT_TRUE(active_host_->GetWifiNetworkId().empty());

    active_host_->GetActiveHost(base::Bind(&ActiveHostTest::OnActiveHostFetched,
                                           base::Unretained(this)));
    fake_tether_host_fetcher_->InvokePendingCallbacks();
    ASSERT_EQ(1u, get_active_host_results_.size());
    EXPECT_EQ((GetActiveHostResult{ActiveHost::ActiveHostStatus::DISCONNECTED,
                                   nullptr, std::string()}),
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
  active_host_->SetActiveHostConnecting(test_devices_[0].GetDeviceId());

  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            active_host_->GetActiveHostDeviceId());
  EXPECT_TRUE(active_host_->GetWifiNetworkId().empty());

  active_host_->GetActiveHost(
      base::Bind(&ActiveHostTest::OnActiveHostFetched, base::Unretained(this)));
  fake_tether_host_fetcher_->InvokePendingCallbacks();
  ASSERT_EQ(1u, get_active_host_results_.size());
  EXPECT_EQ(
      (GetActiveHostResult{ActiveHost::ActiveHostStatus::CONNECTING,
                           std::shared_ptr<cryptauth::RemoteDevice>(
                               new cryptauth::RemoteDevice(test_devices_[0])),
                           std::string()}),
      get_active_host_results_[0]);
}

TEST_F(ActiveHostTest, TestConnected) {
  active_host_->SetActiveHostConnected(test_devices_[0].GetDeviceId(),
                                       "wifiNetworkId");

  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTED,
            active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            active_host_->GetActiveHostDeviceId());
  EXPECT_EQ("wifiNetworkId", active_host_->GetWifiNetworkId());

  active_host_->GetActiveHost(
      base::Bind(&ActiveHostTest::OnActiveHostFetched, base::Unretained(this)));
  fake_tether_host_fetcher_->InvokePendingCallbacks();
  ASSERT_EQ(1u, get_active_host_results_.size());
  EXPECT_EQ(
      (GetActiveHostResult{ActiveHost::ActiveHostStatus::CONNECTED,
                           std::shared_ptr<cryptauth::RemoteDevice>(
                               new cryptauth::RemoteDevice(test_devices_[0])),
                           "wifiNetworkId"}),
      get_active_host_results_[0]);
}

TEST_F(ActiveHostTest, TestActiveHostChangesDuringFetch) {
  active_host_->SetActiveHostConnected(test_devices_[0].GetDeviceId(),
                                       "wifiNetworkId");

  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTED,
            active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            active_host_->GetActiveHostDeviceId());
  EXPECT_EQ("wifiNetworkId", active_host_->GetWifiNetworkId());

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
                                 nullptr, ""}),
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
  active_host_->SetActiveHostConnecting(test_devices_[0].GetDeviceId());
  fake_tether_host_fetcher_->InvokePendingCallbacks();
  EXPECT_EQ(1u, test_observer_->host_changed_updates().size());
  EXPECT_EQ(
      (GetActiveHostResult{ActiveHost::ActiveHostStatus::CONNECTING,
                           std::shared_ptr<cryptauth::RemoteDevice>(
                               new cryptauth::RemoteDevice(test_devices_[0])),
                           ""}),
      test_observer_->host_changed_updates()[0]);

  // Transition to CONNECTED.
  active_host_->SetActiveHostConnected(test_devices_[0].GetDeviceId(),
                                       "wifiNetworkId");
  fake_tether_host_fetcher_->InvokePendingCallbacks();
  EXPECT_EQ(2u, test_observer_->host_changed_updates().size());
  EXPECT_EQ(
      (GetActiveHostResult{ActiveHost::ActiveHostStatus::CONNECTED,
                           std::shared_ptr<cryptauth::RemoteDevice>(
                               new cryptauth::RemoteDevice(test_devices_[0])),
                           "wifiNetworkId"}),
      test_observer_->host_changed_updates()[1]);

  // Transition to DISCONNECTED.
  active_host_->SetActiveHostDisconnected();
  fake_tether_host_fetcher_->InvokePendingCallbacks();
  EXPECT_EQ(3u, test_observer_->host_changed_updates().size());
  EXPECT_EQ((GetActiveHostResult{ActiveHost::ActiveHostStatus::DISCONNECTED,
                                 nullptr, ""}),
            test_observer_->host_changed_updates()[2]);
}

}  // namespace tether

}  // namespace cryptauth
