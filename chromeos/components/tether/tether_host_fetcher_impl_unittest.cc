// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_host_fetcher_impl.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/cryptauth/fake_remote_device_provider.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/remote_device_ref.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

const size_t kNumTestDevices = 5;

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
  TetherHostFetcherImplTest()
      : test_remote_device_list_(CreateTestRemoteDeviceList()),
        test_remote_device_ref_list_(
            CreateTestRemoteDeviceRefList(test_remote_device_list_)) {}

  void SetUp() override {
    fake_remote_device_provider_ =
        std::make_unique<cryptauth::FakeRemoteDeviceProvider>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_multidevice_setup_client_ = std::make_unique<
        chromeos::multidevice_setup::FakeMultiDeviceSetupClient>();
  }

  void TearDown() override {
    // |tether_host_fetcher_| needs to be deleted before |scoped_feature_list_|
    // is deleted. Without this, |tether_host_fetcher_|'s destructor will run
    // without the changes that were made to |scoped_feature_list_|.
    tether_host_fetcher_.reset();
  }

  void SetMultiDeviceApiEnabled() {
    scoped_feature_list_.InitAndEnableFeature(features::kMultiDeviceApi);
  }

  void InitializeTest() {
    SetSyncedDevices(test_remote_device_list_);

    tether_host_fetcher_ = TetherHostFetcherImpl::Factory::NewInstance(
        base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)
            ? nullptr
            : fake_remote_device_provider_.get(),
        base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)
            ? fake_device_sync_client_.get()
            : nullptr,
        base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)
            ? fake_multidevice_setup_client_.get()
            : nullptr);
    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi))
      InvokeGetHostStatusCallback(test_remote_device_list_[0]);
    test_observer_ = std::make_unique<TestObserver>();
    tether_host_fetcher_->AddObserver(test_observer_.get());
  }

  void InvokeGetHostStatusCallback(
      const base::Optional<cryptauth::RemoteDevice>& host_device) {
    EXPECT_TRUE(
        base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));
    if (!host_device) {
      fake_multidevice_setup_client_->InvokePendingGetHostStatusCallback(
          chromeos::multidevice_setup::mojom::HostStatus::kNoEligibleHosts,
          base::nullopt);
    } else {
      fake_multidevice_setup_client_->InvokePendingGetHostStatusCallback(
          chromeos::multidevice_setup::mojom::HostStatus::kHostVerified,
          cryptauth::RemoteDeviceRef(
              std::make_shared<cryptauth::RemoteDevice>(*host_device)));
    }
  }

  void OnTetherHostListFetched(
      const cryptauth::RemoteDeviceRefList& device_list) {
    last_fetched_list_ = device_list;
  }

  void OnSingleTetherHostFetched(
      base::Optional<cryptauth::RemoteDeviceRef> device) {
    last_fetched_single_host_ = device;
  }

  void VerifyAllTetherHosts(
      const cryptauth::RemoteDeviceRefList expected_list) {
    tether_host_fetcher_->FetchAllTetherHosts(
        base::Bind(&TetherHostFetcherImplTest::OnTetherHostListFetched,
                   base::Unretained(this)));
    EXPECT_EQ(expected_list, last_fetched_list_);
  }

  void VerifySingleTetherHost(
      const std::string& device_id,
      base::Optional<cryptauth::RemoteDeviceRef> expected_device) {
    tether_host_fetcher_->FetchTetherHost(
        device_id,
        base::Bind(&TetherHostFetcherImplTest::OnSingleTetherHostFetched,
                   base::Unretained(this)));
    if (expected_device)
      EXPECT_EQ(expected_device, last_fetched_single_host_);
    else
      EXPECT_EQ(base::nullopt, last_fetched_single_host_);
  }

  cryptauth::RemoteDeviceList CreateTestRemoteDeviceList() {
    cryptauth::RemoteDeviceList list =
        cryptauth::CreateRemoteDeviceListForTest(kNumTestDevices);
    for (auto& device : list) {
      device.software_features[cryptauth::SoftwareFeature::MAGIC_TETHER_HOST] =
          cryptauth::SoftwareFeatureState::kSupported;
    }

    // Mark the first device enabled instead of supported.
    list[0].software_features[cryptauth::SoftwareFeature::MAGIC_TETHER_HOST] =
        cryptauth::SoftwareFeatureState::kEnabled;

    return list;
  }

  cryptauth::RemoteDeviceRefList CreateTestRemoteDeviceRefList(
      cryptauth::RemoteDeviceList remote_device_list) {
    cryptauth::RemoteDeviceRefList list;
    for (const auto& device : remote_device_list) {
      list.push_back(cryptauth::RemoteDeviceRef(
          std::make_shared<cryptauth::RemoteDevice>(device)));
    }
    return list;
  }

  void SetSyncedDevices(cryptauth::RemoteDeviceList devices) {
    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
      fake_device_sync_client_->set_synced_devices(
          CreateTestRemoteDeviceRefList(devices));
    } else {
      fake_remote_device_provider_->set_synced_remote_devices(devices);
    }
  }

  void NotifyNewDevicesSynced() {
    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi))
      fake_device_sync_client_->NotifyNewDevicesSynced();
    else
      fake_remote_device_provider_->NotifyObserversDeviceListChanged();
  }

  void TestHasSyncedTetherHosts() {
    InitializeTest();

    EXPECT_TRUE(tether_host_fetcher_->HasSyncedTetherHosts());
    EXPECT_EQ(0u, test_observer_->num_updates());

    // Update the list of devices to be empty.
    SetSyncedDevices(cryptauth::RemoteDeviceList());
    NotifyNewDevicesSynced();
    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi))
      InvokeGetHostStatusCallback(base::nullopt);
    EXPECT_FALSE(tether_host_fetcher_->HasSyncedTetherHosts());
    EXPECT_EQ(1u, test_observer_->num_updates());

    // Notify that the list has changed, even though it hasn't. There should be
    // no update.
    NotifyNewDevicesSynced();
    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi))
      InvokeGetHostStatusCallback(base::nullopt);
    EXPECT_FALSE(tether_host_fetcher_->HasSyncedTetherHosts());
    EXPECT_EQ(1u, test_observer_->num_updates());

    // Update the list to include device 0 only.
    SetSyncedDevices({test_remote_device_list_[0]});
    NotifyNewDevicesSynced();
    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi))
      InvokeGetHostStatusCallback(test_remote_device_list_[0]);
    EXPECT_TRUE(tether_host_fetcher_->HasSyncedTetherHosts());
    EXPECT_EQ(2u, test_observer_->num_updates());

    // Notify that the list has changed, even though it hasn't. There should be
    // no update.
    NotifyNewDevicesSynced();
    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi))
      InvokeGetHostStatusCallback(test_remote_device_list_[0]);
    EXPECT_TRUE(tether_host_fetcher_->HasSyncedTetherHosts());
    EXPECT_EQ(2u, test_observer_->num_updates());
  }

  void TestSingleTetherHost() {
    InitializeTest();

    VerifySingleTetherHost(test_remote_device_ref_list_[0].GetDeviceId(),
                           test_remote_device_ref_list_[0]);

    // Now, set device 0 as the only device. It should still be returned when
    // requested.
    SetSyncedDevices(cryptauth::RemoteDeviceList{test_remote_device_list_[0]});
    NotifyNewDevicesSynced();
    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi))
      InvokeGetHostStatusCallback(test_remote_device_list_[0]);
    VerifySingleTetherHost(test_remote_device_ref_list_[0].GetDeviceId(),
                           test_remote_device_ref_list_[0]);

    // Now, set another device as the only device, but remove its mobile data
    // support. It should not be returned.
    cryptauth::RemoteDevice remote_device = cryptauth::RemoteDevice();
    remote_device
        .software_features[cryptauth::SoftwareFeature::MAGIC_TETHER_HOST] =
        cryptauth::SoftwareFeatureState::kNotSupported;

    SetSyncedDevices(cryptauth::RemoteDeviceList{remote_device});
    NotifyNewDevicesSynced();
    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi))
      InvokeGetHostStatusCallback(remote_device);
    VerifySingleTetherHost(test_remote_device_ref_list_[0].GetDeviceId(),
                           base::nullopt);

    // Update the list; now, there are no more devices.
    SetSyncedDevices(cryptauth::RemoteDeviceList());
    NotifyNewDevicesSynced();
    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi))
      InvokeGetHostStatusCallback(base::nullopt);
    VerifySingleTetherHost(test_remote_device_ref_list_[0].GetDeviceId(),
                           base::nullopt);
  }

  void TestFetchAllTetherHosts() {
    InitializeTest();

    // Create a list of test devices, only some of which are valid tether hosts.
    // Ensure that only that subset is fetched.
    test_remote_device_list_[3]
        .software_features[cryptauth::SoftwareFeature::MAGIC_TETHER_HOST] =
        cryptauth::SoftwareFeatureState::kNotSupported;
    test_remote_device_list_[4]
        .software_features[cryptauth::SoftwareFeature::MAGIC_TETHER_HOST] =
        cryptauth::SoftwareFeatureState::kNotSupported;

    cryptauth::RemoteDeviceRefList host_device_list(
        CreateTestRemoteDeviceRefList({test_remote_device_list_[0],
                                       test_remote_device_list_[1],
                                       test_remote_device_list_[2]}));

    SetSyncedDevices(test_remote_device_list_);
    NotifyNewDevicesSynced();
    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
      InvokeGetHostStatusCallback(test_remote_device_list_[0]);
      VerifyAllTetherHosts(
          CreateTestRemoteDeviceRefList({test_remote_device_list_[0]}));
    } else {
      VerifyAllTetherHosts(host_device_list);
    }
  }

  cryptauth::RemoteDeviceList test_remote_device_list_;
  cryptauth::RemoteDeviceRefList test_remote_device_ref_list_;

  cryptauth::RemoteDeviceRefList last_fetched_list_;
  base::Optional<cryptauth::RemoteDeviceRef> last_fetched_single_host_;
  std::unique_ptr<TestObserver> test_observer_;

  std::unique_ptr<cryptauth::FakeRemoteDeviceProvider>
      fake_remote_device_provider_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<chromeos::multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;

  std::unique_ptr<TetherHostFetcher> tether_host_fetcher_;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherHostFetcherImplTest);
};

TEST_F(TetherHostFetcherImplTest, TestHasSyncedTetherHosts) {
  TestHasSyncedTetherHosts();
}

TEST_F(TetherHostFetcherImplTest,
       TestHasSyncedTetherHosts_MultideviceApiEnabled) {
  SetMultiDeviceApiEnabled();
  TestHasSyncedTetherHosts();
}

TEST_F(TetherHostFetcherImplTest, TestFetchAllTetherHosts) {
  TestFetchAllTetherHosts();
}

TEST_F(TetherHostFetcherImplTest,
       TestFetchAllTetherHosts_MultideviceApiEnabled) {
  SetMultiDeviceApiEnabled();
  TestFetchAllTetherHosts();
}

TEST_F(TetherHostFetcherImplTest, TestSingleTetherHost) {
  TestSingleTetherHost();
}

TEST_F(TetherHostFetcherImplTest, TestSingleTetherHost_MultideviceApiEnabled) {
  SetMultiDeviceApiEnabled();
  TestSingleTetherHost();
}

TEST_F(TetherHostFetcherImplTest,
       TestSingleTetherHost_IdDoesNotCorrespondToDevice) {
  InitializeTest();
  VerifySingleTetherHost("nonexistentId", base::nullopt);
}

TEST_F(TetherHostFetcherImplTest,
       TestSingleTetherHost_IdDoesNotCorrespondToDevice_MultideviceApiEnabled) {
  SetMultiDeviceApiEnabled();
  InitializeTest();
  VerifySingleTetherHost("nonexistentId", base::nullopt);
}

}  // namespace tether

}  // namespace chromeos
