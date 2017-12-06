// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_host_fetcher_impl.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/cryptauth/fake_remote_device_provider.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

const size_t kNumTestDevices = 5;

cryptauth::RemoteDeviceList CreateTestDevices() {
  cryptauth::RemoteDeviceList list =
      cryptauth::GenerateTestRemoteDevices(kNumTestDevices);
  for (auto& remote_device : list)
    remote_device.supports_mobile_hotspot = true;
  return list;
}

class TestObserver : public TetherHostFetcher::Observer {
 public:
  TestObserver() = default;
  virtual ~TestObserver() = default;

  size_t num_updates() { return num_updates_; }

  // TetherHostFetcher::Observer:
  void OnTetherHostsUpdated() override { ++num_updates_; }

 private:
  size_t num_updates_ = 0;
};

}  // namespace

class TetherHostFetcherImplTest : public testing::Test {
 public:
  TetherHostFetcherImplTest() : test_devices_(CreateTestDevices()) {}

  void SetUp() override {
    last_fetched_list_.clear();
    last_fetched_single_host_.reset();

    fake_remote_device_provider_ =
        base::MakeUnique<cryptauth::FakeRemoteDeviceProvider>();
    fake_remote_device_provider_->set_synced_remote_devices(test_devices_);

    tether_host_fetcher_ = base::MakeUnique<TetherHostFetcherImpl>(
        fake_remote_device_provider_.get());

    test_observer_ = base::MakeUnique<TestObserver>();
    tether_host_fetcher_->AddObserver(test_observer_.get());
  }

  void OnTetherHostListFetched(const cryptauth::RemoteDeviceList& device_list) {
    last_fetched_list_ = device_list;
  }

  void OnSingleTetherHostFetched(
      std::unique_ptr<cryptauth::RemoteDevice> device) {
    last_fetched_single_host_ = std::move(device);
  }

  void VerifyAllTetherHosts(const cryptauth::RemoteDeviceList expected_list) {
    tether_host_fetcher_->FetchAllTetherHosts(
        base::Bind(&TetherHostFetcherImplTest::OnTetherHostListFetched,
                   base::Unretained(this)));
    EXPECT_EQ(expected_list, last_fetched_list_);
  }

  void VerifySingleTetherHost(const std::string& device_id,
                              const cryptauth::RemoteDevice* expected_device) {
    tether_host_fetcher_->FetchTetherHost(
        device_id,
        base::Bind(&TetherHostFetcherImplTest::OnSingleTetherHostFetched,
                   base::Unretained(this)));
    if (expected_device)
      EXPECT_EQ(*expected_device, *last_fetched_single_host_);
    else
      EXPECT_EQ(nullptr, last_fetched_single_host_);
  }

  const cryptauth::RemoteDeviceList test_devices_;

  cryptauth::RemoteDeviceList last_fetched_list_;
  std::unique_ptr<cryptauth::RemoteDevice> last_fetched_single_host_;
  std::unique_ptr<TestObserver> test_observer_;

  std::unique_ptr<cryptauth::FakeRemoteDeviceProvider>
      fake_remote_device_provider_;

  std::unique_ptr<TetherHostFetcherImpl> tether_host_fetcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherHostFetcherImplTest);
};

TEST_F(TetherHostFetcherImplTest, TestHasSyncedTetherHosts) {
  EXPECT_TRUE(tether_host_fetcher_->HasSyncedTetherHosts());
  EXPECT_EQ(0u, test_observer_->num_updates());

  // Update the list of devices to be empty.
  fake_remote_device_provider_->set_synced_remote_devices(
      cryptauth::RemoteDeviceList());
  fake_remote_device_provider_->NotifyObserversDeviceListChanged();
  EXPECT_FALSE(tether_host_fetcher_->HasSyncedTetherHosts());
  EXPECT_EQ(1u, test_observer_->num_updates());

  // Notify that the list has changed, even though it hasn't. There should be no
  // update.
  fake_remote_device_provider_->NotifyObserversDeviceListChanged();
  EXPECT_FALSE(tether_host_fetcher_->HasSyncedTetherHosts());
  EXPECT_EQ(1u, test_observer_->num_updates());

  // Update the list to include device 0 only.
  cryptauth::RemoteDeviceList device_0_list = {test_devices_[0]};
  fake_remote_device_provider_->set_synced_remote_devices(device_0_list);
  fake_remote_device_provider_->NotifyObserversDeviceListChanged();
  EXPECT_TRUE(tether_host_fetcher_->HasSyncedTetherHosts());
  EXPECT_EQ(2u, test_observer_->num_updates());

  // Notify that the list has changed, even though it hasn't. There should be no
  // update.
  fake_remote_device_provider_->NotifyObserversDeviceListChanged();
  EXPECT_TRUE(tether_host_fetcher_->HasSyncedTetherHosts());
  EXPECT_EQ(2u, test_observer_->num_updates());
}

TEST_F(TetherHostFetcherImplTest, TestFetchAllTetherHosts) {
  VerifyAllTetherHosts(test_devices_);

  // Since none of these devices support mobile data, the list should be empty.
  cryptauth::RemoteDeviceList list_without_mobile_support = test_devices_;
  for (auto& remote_device : list_without_mobile_support)
    remote_device.supports_mobile_hotspot = false;
  fake_remote_device_provider_->set_synced_remote_devices(
      list_without_mobile_support);
  fake_remote_device_provider_->NotifyObserversDeviceListChanged();
  VerifyAllTetherHosts(cryptauth::RemoteDeviceList());

  // The list contains devices 0, 1, and 2, all of which support mobile data.
  cryptauth::RemoteDeviceList smaller_list = {
      test_devices_[0], test_devices_[1], test_devices_[2]};
  fake_remote_device_provider_->set_synced_remote_devices(smaller_list);
  fake_remote_device_provider_->NotifyObserversDeviceListChanged();
  VerifyAllTetherHosts(smaller_list);
}

TEST_F(TetherHostFetcherImplTest, TestSingleTetherHost) {
  VerifySingleTetherHost(test_devices_[0].GetDeviceId(), &test_devices_[0]);

  // Now, set device 0 as the only device. It should still be returned when
  // requested.
  cryptauth::RemoteDevice device_0 = test_devices_[0];
  fake_remote_device_provider_->set_synced_remote_devices(
      cryptauth::RemoteDeviceList{device_0});
  fake_remote_device_provider_->NotifyObserversDeviceListChanged();
  VerifySingleTetherHost(test_devices_[0].GetDeviceId(), &device_0);

  // Now, set device 0 as the only device, but remove its mobile data support.
  // It should not be returned.
  device_0.supports_mobile_hotspot = false;
  fake_remote_device_provider_->set_synced_remote_devices(
      cryptauth::RemoteDeviceList{device_0});
  fake_remote_device_provider_->NotifyObserversDeviceListChanged();
  VerifySingleTetherHost(test_devices_[0].GetDeviceId(), nullptr);

  // Update the list; now, there are no more devices.
  fake_remote_device_provider_->set_synced_remote_devices(
      cryptauth::RemoteDeviceList());
  fake_remote_device_provider_->NotifyObserversDeviceListChanged();
  VerifySingleTetherHost(test_devices_[0].GetDeviceId(), nullptr);
}

TEST_F(TetherHostFetcherImplTest,
       TestSingleTetherHost_IdDoesNotCorrespondToDevice) {
  VerifySingleTetherHost("nonexistentId", nullptr);
}

}  // namespace tether

}  // namespace chromeos
