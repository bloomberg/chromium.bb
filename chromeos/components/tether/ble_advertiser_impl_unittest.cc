// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_advertiser_impl.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/stl_util.h"
#include "chromeos/components/tether/ble_constants.h"
#include "chromeos/components/tether/error_tolerant_ble_advertisement_impl.h"
#include "chromeos/components/tether/fake_ble_synchronizer.h"
#include "chromeos/components/tether/fake_error_tolerant_ble_advertisement.h"
#include "components/cryptauth/mock_foreground_eid_generator.h"
#include "components/cryptauth/mock_local_device_data_provider.h"
#include "components/cryptauth/mock_remote_beacon_seed_fetcher.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace chromeos {

namespace tether {

namespace {

const char kFakePublicKey[] = "fakePublicKey";

std::vector<cryptauth::DataWithTimestamp> GenerateFakeAdvertisements() {
  cryptauth::DataWithTimestamp advertisement1("advertisement1", 1000L, 2000L);
  cryptauth::DataWithTimestamp advertisement2("advertisement2", 2000L, 3000L);
  cryptauth::DataWithTimestamp advertisement3("advertisement3", 3000L, 4000L);

  std::vector<cryptauth::DataWithTimestamp> advertisements = {
      advertisement1, advertisement2, advertisement3};
  return advertisements;
}

std::vector<cryptauth::BeaconSeed> CreateFakeBeaconSeedsForDevice(
    const cryptauth::RemoteDevice& remote_device) {
  cryptauth::BeaconSeed seed1;
  seed1.set_data("seed1Data" + remote_device.GetTruncatedDeviceIdForLogs());
  seed1.set_start_time_millis(1000L);
  seed1.set_start_time_millis(2000L);

  cryptauth::BeaconSeed seed2;
  seed2.set_data("seed2Data" + remote_device.GetTruncatedDeviceIdForLogs());
  seed2.set_start_time_millis(2000L);
  seed2.set_start_time_millis(3000L);

  std::vector<cryptauth::BeaconSeed> seeds = {seed1, seed2};
  return seeds;
}

class FakeErrorTolerantBleAdvertisementFactory
    : public ErrorTolerantBleAdvertisementImpl::Factory {
 public:
  FakeErrorTolerantBleAdvertisementFactory() {}
  ~FakeErrorTolerantBleAdvertisementFactory() override {}

  const std::vector<FakeErrorTolerantBleAdvertisement*>&
  active_advertisements() {
    return active_advertisements_;
  }

  size_t num_created() { return num_created_; }

  // ErrorTolerantBleAdvertisementImpl::Factory:
  std::unique_ptr<ErrorTolerantBleAdvertisement> BuildInstance(
      const std::string& device_id,
      std::unique_ptr<cryptauth::DataWithTimestamp> advertisement_data,
      BleSynchronizerBase* ble_synchronizer) override {
    FakeErrorTolerantBleAdvertisement* fake_advertisement =
        new FakeErrorTolerantBleAdvertisement(
            device_id, base::Bind(&FakeErrorTolerantBleAdvertisementFactory::
                                      OnFakeAdvertisementDeleted,
                                  base::Unretained(this)));
    active_advertisements_.push_back(fake_advertisement);
    ++num_created_;
    return base::WrapUnique(fake_advertisement);
  }

 protected:
  void OnFakeAdvertisementDeleted(
      FakeErrorTolerantBleAdvertisement* fake_advertisement) {
    EXPECT_TRUE(std::find(active_advertisements_.begin(),
                          active_advertisements_.end(),
                          fake_advertisement) != active_advertisements_.end());
    base::Erase(active_advertisements_, fake_advertisement);
  }

 private:
  std::vector<FakeErrorTolerantBleAdvertisement*> active_advertisements_;
  size_t num_created_ = 0;
};

class TestObserver : public BleAdvertiser::Observer {
 public:
  TestObserver() {}
  ~TestObserver() override {}

  size_t num_times_all_advertisements_unregistered() {
    return num_times_all_advertisements_unregistered_;
  }

  // BleAdvertiser::Observer:
  void OnAllAdvertisementsUnregistered() override {
    ++num_times_all_advertisements_unregistered_;
  }

 private:
  size_t num_times_all_advertisements_unregistered_ = 0;
};

}  // namespace

class BleAdvertiserImplTest : public testing::Test {
 protected:
  BleAdvertiserImplTest()
      : fake_devices_(cryptauth::GenerateTestRemoteDevices(3)),
        fake_advertisements_(GenerateFakeAdvertisements()) {}

  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<StrictMock<device::MockBluetoothAdapter>>();

    std::unique_ptr<cryptauth::MockForegroundEidGenerator> eid_generator =
        base::MakeUnique<cryptauth::MockForegroundEidGenerator>();
    mock_eid_generator_ = eid_generator.get();

    mock_seed_fetcher_ =
        base::MakeUnique<cryptauth::MockRemoteBeaconSeedFetcher>();
    std::vector<cryptauth::BeaconSeed> device_0_beacon_seeds =
        CreateFakeBeaconSeedsForDevice(fake_devices_[0]);
    std::vector<cryptauth::BeaconSeed> device_1_beacon_seeds =
        CreateFakeBeaconSeedsForDevice(fake_devices_[1]);
    std::vector<cryptauth::BeaconSeed> device_2_beacon_seeds =
        CreateFakeBeaconSeedsForDevice(fake_devices_[2]);
    mock_seed_fetcher_->SetSeedsForDevice(fake_devices_[0],
                                          &device_0_beacon_seeds);
    mock_seed_fetcher_->SetSeedsForDevice(fake_devices_[1],
                                          &device_1_beacon_seeds);
    mock_seed_fetcher_->SetSeedsForDevice(fake_devices_[2],
                                          &device_2_beacon_seeds);

    mock_local_data_provider_ =
        base::MakeUnique<cryptauth::MockLocalDeviceDataProvider>();
    mock_local_data_provider_->SetPublicKey(
        base::MakeUnique<std::string>(kFakePublicKey));

    fake_ble_synchronizer_ = base::MakeUnique<FakeBleSynchronizer>();

    fake_advertisement_factory_ =
        base::WrapUnique(new FakeErrorTolerantBleAdvertisementFactory());
    ErrorTolerantBleAdvertisementImpl::Factory::SetInstanceForTesting(
        fake_advertisement_factory_.get());

    test_observer_ = base::WrapUnique(new TestObserver());

    ble_advertiser_ = base::MakeUnique<BleAdvertiserImpl>(
        mock_local_data_provider_.get(), mock_seed_fetcher_.get(),
        fake_ble_synchronizer_.get());
    ble_advertiser_->SetEidGeneratorForTest(std::move(eid_generator));
    ble_advertiser_->AddObserver(test_observer_.get());
  }

  void TearDown() override {
    ble_advertiser_->RemoveObserver(test_observer_.get());
    ErrorTolerantBleAdvertisementImpl::Factory::SetInstanceForTesting(nullptr);
  }

  void VerifyAdvertisementHasBeenStopped(
      size_t index,
      const std::string& expected_device_id) {
    FakeErrorTolerantBleAdvertisement* advertisement =
        fake_advertisement_factory_->active_advertisements()[index];
    EXPECT_EQ(expected_device_id, advertisement->device_id());
    EXPECT_TRUE(advertisement->HasBeenStopped());
  }

  void InvokeAdvertisementStoppedCallback(
      size_t index,
      const std::string& expected_device_id) {
    FakeErrorTolerantBleAdvertisement* advertisement =
        fake_advertisement_factory_->active_advertisements()[index];
    EXPECT_EQ(expected_device_id, advertisement->device_id());
    advertisement->InvokeStopCallback();
  }

  const std::vector<cryptauth::RemoteDevice> fake_devices_;
  const std::vector<cryptauth::DataWithTimestamp> fake_advertisements_;

  scoped_refptr<StrictMock<device::MockBluetoothAdapter>> mock_adapter_;
  cryptauth::MockForegroundEidGenerator* mock_eid_generator_;
  std::unique_ptr<cryptauth::MockRemoteBeaconSeedFetcher> mock_seed_fetcher_;
  std::unique_ptr<cryptauth::MockLocalDeviceDataProvider>
      mock_local_data_provider_;
  std::unique_ptr<FakeBleSynchronizer> fake_ble_synchronizer_;

  std::unique_ptr<TestObserver> test_observer_;

  std::unique_ptr<FakeErrorTolerantBleAdvertisementFactory>
      fake_advertisement_factory_;

  std::unique_ptr<BleAdvertiserImpl> ble_advertiser_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BleAdvertiserImplTest);
};

TEST_F(BleAdvertiserImplTest, TestCannotFetchPublicKey) {
  mock_local_data_provider_->SetPublicKey(nullptr);
  EXPECT_FALSE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(0u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(0u, test_observer_->num_times_all_advertisements_unregistered());
}

TEST_F(BleAdvertiserImplTest, EmptyPublicKey) {
  mock_local_data_provider_->SetPublicKey(base::MakeUnique<std::string>(""));
  EXPECT_FALSE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(0u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(0u, test_observer_->num_times_all_advertisements_unregistered());
}

TEST_F(BleAdvertiserImplTest, NoBeaconSeeds) {
  mock_seed_fetcher_->SetSeedsForDevice(fake_devices_[0], nullptr);
  EXPECT_FALSE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(0u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(0u, test_observer_->num_times_all_advertisements_unregistered());
}

TEST_F(BleAdvertiserImplTest, EmptyBeaconSeeds) {
  std::vector<cryptauth::BeaconSeed> empty_seeds;
  mock_seed_fetcher_->SetSeedsForDevice(fake_devices_[0], &empty_seeds);

  EXPECT_FALSE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(0u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(0u, test_observer_->num_times_all_advertisements_unregistered());
}

TEST_F(BleAdvertiserImplTest, CannotGenerateAdvertisement) {
  mock_eid_generator_->set_advertisement(nullptr);
  EXPECT_FALSE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(0u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(0u, test_observer_->num_times_all_advertisements_unregistered());
}

TEST_F(BleAdvertiserImplTest, AdvertisementRegisteredSuccessfully) {
  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[0]));

  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(1u, fake_advertisement_factory_->active_advertisements().size());
  EXPECT_TRUE(ble_advertiser_->AreAdvertisementsRegistered());
  EXPECT_EQ(0u, test_observer_->num_times_all_advertisements_unregistered());

  // Now, unregister.
  EXPECT_TRUE(ble_advertiser_->StopAdvertisingToDevice(fake_devices_[0]));
  EXPECT_TRUE(ble_advertiser_->AreAdvertisementsRegistered());

  // The advertisement should have been stopped, but it should not yet have
  // been removed.
  EXPECT_EQ(1u, fake_advertisement_factory_->active_advertisements().size());
  VerifyAdvertisementHasBeenStopped(0u /* index */,
                                    fake_devices_[0].GetDeviceId());

  // Invoke the stop callback and ensure the advertisement was deleted.
  InvokeAdvertisementStoppedCallback(0u /* index */,
                                     fake_devices_[0].GetDeviceId());
  EXPECT_EQ(0u, fake_advertisement_factory_->active_advertisements().size());
  EXPECT_FALSE(ble_advertiser_->AreAdvertisementsRegistered());
  EXPECT_EQ(1u, test_observer_->num_times_all_advertisements_unregistered());
}

TEST_F(BleAdvertiserImplTest, AdvertisementRegisteredSuccessfully_TwoDevices) {
  // Register device 0.
  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[0]));
  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(1u, fake_advertisement_factory_->active_advertisements().size());
  EXPECT_TRUE(ble_advertiser_->AreAdvertisementsRegistered());
  EXPECT_EQ(0u, test_observer_->num_times_all_advertisements_unregistered());

  // Register device 1.
  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[1]));
  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[1]));
  EXPECT_EQ(2u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(2u, fake_advertisement_factory_->active_advertisements().size());
  EXPECT_TRUE(ble_advertiser_->AreAdvertisementsRegistered());
  EXPECT_EQ(0u, test_observer_->num_times_all_advertisements_unregistered());

  // Unregister device 0.
  EXPECT_TRUE(ble_advertiser_->StopAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(2u, fake_advertisement_factory_->active_advertisements().size());
  InvokeAdvertisementStoppedCallback(0u /* index */,
                                     fake_devices_[0].GetDeviceId());
  EXPECT_EQ(1u, fake_advertisement_factory_->active_advertisements().size());
  EXPECT_TRUE(ble_advertiser_->AreAdvertisementsRegistered());
  EXPECT_EQ(0u, test_observer_->num_times_all_advertisements_unregistered());

  // Unregister device 1.
  EXPECT_TRUE(ble_advertiser_->StopAdvertisingToDevice(fake_devices_[1]));
  EXPECT_EQ(1u, fake_advertisement_factory_->active_advertisements().size());
  InvokeAdvertisementStoppedCallback(0u /* index */,
                                     fake_devices_[1].GetDeviceId());
  EXPECT_EQ(0u, fake_advertisement_factory_->active_advertisements().size());
  EXPECT_FALSE(ble_advertiser_->AreAdvertisementsRegistered());
  EXPECT_EQ(1u, test_observer_->num_times_all_advertisements_unregistered());
}

TEST_F(BleAdvertiserImplTest, TooManyDevicesRegistered) {
  ASSERT_EQ(2u, kMaxConcurrentAdvertisements);

  // Register device 0.
  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[0]));
  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(1u, fake_advertisement_factory_->active_advertisements().size());

  // Register device 1.
  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[1]));
  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[1]));
  EXPECT_EQ(2u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(2u, fake_advertisement_factory_->active_advertisements().size());

  // Register device 2. This should fail, since it is over the limit.
  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[2]));
  EXPECT_FALSE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[2]));
  EXPECT_EQ(2u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(2u, fake_advertisement_factory_->active_advertisements().size());

  // Now, stop advertising to device 1. It should now be possible to advertise
  // to device 2.
  EXPECT_TRUE(ble_advertiser_->StopAdvertisingToDevice(fake_devices_[1]));
  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[2]));

  // However, the advertisement for device 1 should still be present, and no new
  // advertisement for device 2 should have yet been created.
  EXPECT_EQ(2u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(2u, fake_advertisement_factory_->active_advertisements().size());
  VerifyAdvertisementHasBeenStopped(1u /* index */,
                                    fake_devices_[1].GetDeviceId());

  // Stop the advertisement for device 1. This should cause a new advertisement
  // for device 2 to be created.
  InvokeAdvertisementStoppedCallback(1u /* index */,
                                     fake_devices_[1].GetDeviceId());
  EXPECT_EQ(3u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(2u, fake_advertisement_factory_->active_advertisements().size());

  // Verify that the remaining active advertisements correspond to the correct
  // devices.
  EXPECT_EQ(
      fake_devices_[0].GetDeviceId(),
      fake_advertisement_factory_->active_advertisements()[0]->device_id());
  EXPECT_EQ(
      fake_devices_[2].GetDeviceId(),
      fake_advertisement_factory_->active_advertisements()[1]->device_id());
}

// Regression test for crbug.com/739883. This issue arises when the following
// occurs:
//   (1) BleAdvertiserImpl starts advertising to device A.
//   (2) BleAdvertiserImpl stops advertising to device A. The advertisement
//       starts its asynchyronous unregistration flow.
//   (3) BleAdvertiserImpl starts advertising to device A again, but the
//       previous advertisement has not yet been fully unregistered.
// Before the fix for crbug.com/739883, this would cause an error of type
// ERROR_ADVERTISEMENT_ALREADY_EXISTS. However, the fix for this issue ensures
// that the new advertisement in step (3) above does not start until the
// previous one has been finished.
TEST_F(BleAdvertiserImplTest, SameAdvertisementAdded_FirstHasNotBeenStopped) {
  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[0]));

  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(1u, fake_advertisement_factory_->active_advertisements().size());

  // Unregister, but do not invoke the stop callback.
  EXPECT_TRUE(ble_advertiser_->StopAdvertisingToDevice(fake_devices_[0]));
  VerifyAdvertisementHasBeenStopped(0u /* index */,
                                    fake_devices_[0].GetDeviceId());

  // Start advertising again, to the same device. Since the previous
  // advertisement has not successfully stopped, no new advertisement should
  // have been created yet.
  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(1u, fake_advertisement_factory_->active_advertisements().size());

  // Now, complete the previous stop. This should cause a new advertisement to
  // be generated, but only the new one should be active.
  InvokeAdvertisementStoppedCallback(0u /* index */,
                                     fake_devices_[0].GetDeviceId());
  EXPECT_EQ(2u, fake_advertisement_factory_->num_created());
  EXPECT_EQ(1u, fake_advertisement_factory_->active_advertisements().size());
}

}  // namespace tether

}  // namespace chromeos
