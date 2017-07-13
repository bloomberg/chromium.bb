// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_advertiser.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "chromeos/components/tether/ble_constants.h"
#include "components/cryptauth/mock_foreground_eid_generator.h"
#include "components/cryptauth/mock_local_device_data_provider.h"
#include "components/cryptauth/mock_remote_beacon_seed_fetcher.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::Return;
using testing::StrictMock;
using testing::_;

namespace chromeos {

namespace tether {

namespace {
uint8_t kInvertedConnectionFlag = 0x01;

const char kFakePublicKey[] = "fakePublicKey";

struct RegisterAdvertisementArgs
    : public base::RefCounted<RegisterAdvertisementArgs> {
  RegisterAdvertisementArgs(
      const device::BluetoothAdvertisement::ServiceData& service_data,
      const device::BluetoothAdvertisement::UUIDList& service_uuids,
      const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
      const device::BluetoothAdapter::AdvertisementErrorCallback& errback)
      : service_data(service_data),
        service_uuids(service_uuids),
        callback(callback),
        errback(errback) {}

  RegisterAdvertisementArgs(const RegisterAdvertisementArgs& other)
      : service_data(other.service_data),
        service_uuids(other.service_uuids),
        callback(other.callback),
        errback(other.errback) {}

  device::BluetoothAdvertisement::ServiceData service_data;
  device::BluetoothAdvertisement::UUIDList service_uuids;
  const device::BluetoothAdapter::CreateAdvertisementCallback callback;
  const device::BluetoothAdapter::AdvertisementErrorCallback errback;

 protected:
  friend class base::RefCounted<RegisterAdvertisementArgs>;
  virtual ~RegisterAdvertisementArgs() {}
};

class MockBluetoothAdapterWithAdvertisements
    : public device::MockBluetoothAdapter {
 public:
  MockBluetoothAdapterWithAdvertisements() : MockBluetoothAdapter() {}

  MOCK_METHOD1(RegisterAdvertisementWithArgsStruct,
               void(scoped_refptr<RegisterAdvertisementArgs>));

  void RegisterAdvertisement(
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
      const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
      const device::BluetoothAdapter::AdvertisementErrorCallback& errback)
      override {
    RegisterAdvertisementWithArgsStruct(
        make_scoped_refptr(new RegisterAdvertisementArgs(
            *advertisement_data->service_data(),
            *advertisement_data->service_uuids(), callback, errback)));
  }

 protected:
  ~MockBluetoothAdapterWithAdvertisements() override {}
};

class FakeBluetoothAdvertisement : public device::BluetoothAdvertisement {
 public:
  // |unregister_callback| should be called with the success callback passed to
  // Unregister() whenever an Unregister() call occurs.
  FakeBluetoothAdvertisement(
      const base::Callback<
          void(const device::BluetoothAdvertisement::SuccessCallback&)>&
          unregister_callback)
      : unregister_callback_(unregister_callback) {}

  // BluetoothAdvertisement:
  void Unregister(
      const device::BluetoothAdvertisement::SuccessCallback& success_callback,
      const device::BluetoothAdvertisement::ErrorCallback& error_callback)
      override {
    unregister_callback_.Run(success_callback);
  }

 private:
  ~FakeBluetoothAdvertisement() override {}

  base::Callback<void(const device::BluetoothAdvertisement::SuccessCallback&)>
      unregister_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothAdvertisement);
};

std::vector<cryptauth::DataWithTimestamp> GenerateFakeAdvertisements() {
  cryptauth::DataWithTimestamp advertisement1("advertisement1", 1000L, 2000L);
  cryptauth::DataWithTimestamp advertisement2("advertisement2", 2000L, 3000L);

  std::vector<cryptauth::DataWithTimestamp> advertisements = {advertisement1,
                                                              advertisement2};
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

}  // namespace

class BleAdvertiserTest : public testing::Test {
 protected:
  BleAdvertiserTest()
      : fake_devices_(cryptauth::GenerateTestRemoteDevices(3)),
        fake_advertisements_(GenerateFakeAdvertisements()) {}

  void SetUp() override {
    individual_advertisements_.clear();
    register_advertisement_args_.clear();

    mock_adapter_ = make_scoped_refptr(
        new StrictMock<MockBluetoothAdapterWithAdvertisements>());
    ON_CALL(*mock_adapter_, AddObserver(_))
        .WillByDefault(Invoke(this, &BleAdvertiserTest::OnAdapterAddObserver));
    ON_CALL(*mock_adapter_, RemoveObserver(_))
        .WillByDefault(
            Invoke(this, &BleAdvertiserTest::OnAdapterRemoveObserver));
    ON_CALL(*mock_adapter_, IsPowered()).WillByDefault(Return(true));
    ON_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_))
        .WillByDefault(
            Invoke(this, &BleAdvertiserTest::OnAdapterRegisterAdvertisement));

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

    ble_advertiser_ = base::WrapUnique(new BleAdvertiser(
        mock_adapter_, std::move(eid_generator), mock_seed_fetcher_.get(),
        mock_local_data_provider_.get()));
  }

  void TearDown() override {
    // Delete the advertiser to ensure that cleanup happens properly.
    ble_advertiser_.reset();

    // All observers should have been removed.
    EXPECT_TRUE(individual_advertisements_.empty());
  }

  void OnAdapterAddObserver(device::BluetoothAdapter::Observer* observer) {
    individual_advertisements_.push_back(
        reinterpret_cast<BleAdvertiser::IndividualAdvertisement*>(observer));
  }

  void OnAdapterRemoveObserver(device::BluetoothAdapter::Observer* observer) {
    individual_advertisements_.erase(std::find(
        individual_advertisements_.begin(), individual_advertisements_.end(),
        reinterpret_cast<BleAdvertiser::IndividualAdvertisement*>(observer)));
  }

  void OnAdapterRegisterAdvertisement(
      scoped_refptr<RegisterAdvertisementArgs> args) {
    register_advertisement_args_.push_back(args);
  }

  void VerifyServiceDataMatches(
      scoped_refptr<RegisterAdvertisementArgs> args,
      const BleAdvertiser::IndividualAdvertisement* advertisement) {
    // First, verify that the service UUID list is correct.
    EXPECT_EQ(1u, args->service_uuids.size());
    EXPECT_EQ(std::string(kAdvertisingServiceUuid), args->service_uuids[0]);

    // Then, verify that the service data is correct.
    EXPECT_EQ(1u, args->service_data.size());
    std::vector<uint8_t> service_data_from_args =
        args->service_data[std::string(kAdvertisingServiceUuid)];
    EXPECT_EQ(service_data_from_args.size(),
              advertisement->advertisement_data_->data.size() + 1);
    EXPECT_FALSE(memcmp(advertisement->advertisement_data_->data.data(),
                        service_data_from_args.data(),
                        args->service_data.size()));
    EXPECT_EQ(kInvertedConnectionFlag,
              service_data_from_args[service_data_from_args.size() - 1]);
  }

  void VerifyAdvertisementHasNotStarted(
      BleAdvertiser::IndividualAdvertisement* individual_advertisement) {
    EXPECT_FALSE(individual_advertisement->advertisement_.get());
  }

  void VerifyAdvertisementHasStarted(
      BleAdvertiser::IndividualAdvertisement* individual_advertisement) {
    EXPECT_TRUE(individual_advertisement->advertisement_.get());
  }

  void InvokeErrback(scoped_refptr<RegisterAdvertisementArgs> args) {
    args->errback.Run(device::BluetoothAdvertisement::ErrorCode::
                          INVALID_ADVERTISEMENT_ERROR_CODE);
  }

  void InvokeCallback(scoped_refptr<RegisterAdvertisementArgs> args) {
    args->callback.Run(make_scoped_refptr(new FakeBluetoothAdvertisement(
        base::Bind(&BleAdvertiserTest::OnAdvertisementUnregistered,
                   base::Unretained(this)))));
  }

  void ReleaseAdvertisement(
      BleAdvertiser::IndividualAdvertisement* individual_advertisement) {
    individual_advertisement->AdvertisementReleased(
        individual_advertisement->advertisement_.get());
  }

  void OnAdvertisementUnregistered(
      const device::BluetoothAdvertisement::SuccessCallback& success_callback) {
    unregister_callbacks_.push_back(success_callback);
  }

  void InvokeLastUnregisterCallbackAndRemove() {
    DCHECK(!unregister_callbacks_.empty());
    unregister_callbacks_[unregister_callbacks_.size() - 1].Run();
    unregister_callbacks_.erase(unregister_callbacks_.begin() +
                                unregister_callbacks_.size() - 1);
  }

  std::unique_ptr<BleAdvertiser> ble_advertiser_;

  scoped_refptr<StrictMock<MockBluetoothAdapterWithAdvertisements>>
      mock_adapter_;
  cryptauth::MockForegroundEidGenerator* mock_eid_generator_;
  std::unique_ptr<cryptauth::MockRemoteBeaconSeedFetcher> mock_seed_fetcher_;
  std::unique_ptr<cryptauth::MockLocalDeviceDataProvider>
      mock_local_data_provider_;

  std::vector<scoped_refptr<RegisterAdvertisementArgs>>
      register_advertisement_args_;
  std::vector<BleAdvertiser::IndividualAdvertisement*>
      individual_advertisements_;
  std::vector<device::BluetoothAdvertisement::SuccessCallback>
      unregister_callbacks_;

  const std::vector<cryptauth::RemoteDevice> fake_devices_;
  const std::vector<cryptauth::DataWithTimestamp> fake_advertisements_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BleAdvertiserTest);
};

TEST_F(BleAdvertiserTest, TestCannotFetchPublicKey) {
  EXPECT_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_)).Times(0);

  mock_local_data_provider_->SetPublicKey(nullptr);

  EXPECT_FALSE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
}

TEST_F(BleAdvertiserTest, EmptyPublicKey) {
  EXPECT_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_)).Times(0);

  mock_local_data_provider_->SetPublicKey(base::MakeUnique<std::string>(""));

  EXPECT_FALSE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
}

TEST_F(BleAdvertiserTest, NoBeaconSeeds) {
  EXPECT_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_)).Times(0);

  mock_seed_fetcher_->SetSeedsForDevice(fake_devices_[0], nullptr);

  EXPECT_FALSE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
}

TEST_F(BleAdvertiserTest, EmptyBeaconSeeds) {
  EXPECT_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_)).Times(0);

  std::vector<cryptauth::BeaconSeed> empty_seeds;
  mock_seed_fetcher_->SetSeedsForDevice(fake_devices_[0], &empty_seeds);

  EXPECT_FALSE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
}

TEST_F(BleAdvertiserTest, CannotGenerateAdvertisement) {
  EXPECT_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_)).Times(0);

  mock_eid_generator_->set_advertisement(nullptr);

  EXPECT_FALSE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
}

TEST_F(BleAdvertiserTest, AdapterPoweredOffWhenAdvertisementRegistered) {
  EXPECT_CALL(*mock_adapter_, AddObserver(_)).Times(1);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(_)).Times(1);
  EXPECT_CALL(*mock_adapter_, IsPowered()).Times(1).WillOnce(Return(false));

  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[0]));

  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, individual_advertisements_.size());

  // RegisterAdvertisement() should not have been called.
  EXPECT_TRUE(register_advertisement_args_.empty());
}

TEST_F(BleAdvertiserTest, RegisteringAdvertisementFails) {
  EXPECT_CALL(*mock_adapter_, AddObserver(_)).Times(1);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(_)).Times(1);
  EXPECT_CALL(*mock_adapter_, IsPowered()).Times(1);
  EXPECT_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_)).Times(1);

  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[0]));

  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, individual_advertisements_.size());
  EXPECT_EQ(1u, register_advertisement_args_.size());
  VerifyServiceDataMatches(register_advertisement_args_[0],
                           individual_advertisements_[0]);

  InvokeErrback(register_advertisement_args_[0]);
  VerifyAdvertisementHasNotStarted(individual_advertisements_[0]);
}

TEST_F(BleAdvertiserTest, AdvertisementRegisteredSuccessfully) {
  EXPECT_CALL(*mock_adapter_, AddObserver(_)).Times(1);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(_)).Times(1);
  EXPECT_CALL(*mock_adapter_, IsPowered()).Times(1);
  EXPECT_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_)).Times(1);

  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[0]));

  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, individual_advertisements_.size());
  EXPECT_EQ(1u, register_advertisement_args_.size());
  VerifyServiceDataMatches(register_advertisement_args_[0],
                           individual_advertisements_[0]);

  InvokeCallback(register_advertisement_args_[0]);
  VerifyAdvertisementHasStarted(individual_advertisements_[0]);

  // Now, unregister.
  EXPECT_TRUE(ble_advertiser_->StopAdvertisingToDevice(fake_devices_[0]));
  InvokeLastUnregisterCallbackAndRemove();
  EXPECT_TRUE(individual_advertisements_.empty());
}

TEST_F(BleAdvertiserTest, AdvertisementRegisteredSuccessfully_TwoDevices) {
  EXPECT_CALL(*mock_adapter_, AddObserver(_)).Times(2);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(_)).Times(2);
  EXPECT_CALL(*mock_adapter_, IsPowered()).Times(2);
  EXPECT_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_)).Times(2);

  // First device.
  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[0]));

  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, individual_advertisements_.size());
  EXPECT_EQ(1u, register_advertisement_args_.size());
  VerifyServiceDataMatches(register_advertisement_args_[0],
                           individual_advertisements_[0]);

  InvokeCallback(register_advertisement_args_[0]);
  VerifyAdvertisementHasStarted(individual_advertisements_[0]);

  // Second device.
  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[1]));

  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[1]));
  EXPECT_EQ(2u, individual_advertisements_.size());
  EXPECT_EQ(2u, register_advertisement_args_.size());
  VerifyServiceDataMatches(register_advertisement_args_[1],
                           individual_advertisements_[1]);

  InvokeCallback(register_advertisement_args_[1]);
  VerifyAdvertisementHasStarted(individual_advertisements_[1]);

  // Now, unregister.
  EXPECT_TRUE(ble_advertiser_->StopAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, individual_advertisements_.size());
  InvokeLastUnregisterCallbackAndRemove();

  EXPECT_TRUE(ble_advertiser_->StopAdvertisingToDevice(fake_devices_[1]));
  EXPECT_TRUE(individual_advertisements_.empty());
  InvokeLastUnregisterCallbackAndRemove();
}

TEST_F(BleAdvertiserTest, TooManyDevicesRegistered) {
  ASSERT_EQ(2, kMaxConcurrentAdvertisements);

  EXPECT_CALL(*mock_adapter_, AddObserver(_)).Times(3);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(_)).Times(3);
  EXPECT_CALL(*mock_adapter_, IsPowered()).Times(3);
  EXPECT_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_)).Times(3);

  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[0]));

  // Should succeed for the first two devices.
  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[1]));

  // Should fail on the third device.
  EXPECT_FALSE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[2]));

  // Now, stop advertising to one; registering the third device should succeed
  // at this point.
  EXPECT_TRUE(ble_advertiser_->StopAdvertisingToDevice(fake_devices_[0]));
  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[2]));
}

TEST_F(BleAdvertiserTest, AdapterPowerChange_StartsOffThenTurnsOn) {
  EXPECT_CALL(*mock_adapter_, AddObserver(_)).Times(1);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(_)).Times(1);
  EXPECT_CALL(*mock_adapter_, IsPowered())
      .Times(2)
      .WillOnce(Return(false))  // First, the adapter is powered off.
      .WillOnce(Return(true));  // Then, the adapter is powered back on.
  EXPECT_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_)).Times(1);

  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[0]));

  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, individual_advertisements_.size());

  // RegisterAdvertisement() should not have been called.
  EXPECT_TRUE(register_advertisement_args_.empty());

  // Now, simulate power being changed. Since the power is now on,
  // RegisterAdvertisement() should have been called.
  individual_advertisements_[0]->AdapterPoweredChanged(mock_adapter_.get(),
                                                       true);
  EXPECT_EQ(1u, individual_advertisements_.size());
  EXPECT_EQ(1u, register_advertisement_args_.size());
  VerifyServiceDataMatches(register_advertisement_args_[0],
                           individual_advertisements_[0]);
}

TEST_F(BleAdvertiserTest, AdvertisementReleased) {
  EXPECT_CALL(*mock_adapter_, AddObserver(_)).Times(1);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(_)).Times(1);
  EXPECT_CALL(*mock_adapter_, IsPowered()).Times(2);
  EXPECT_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_)).Times(2);

  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[0]));

  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, individual_advertisements_.size());
  EXPECT_EQ(1u, register_advertisement_args_.size());
  VerifyServiceDataMatches(register_advertisement_args_[0],
                           individual_advertisements_[0]);

  InvokeCallback(register_advertisement_args_[0]);
  VerifyAdvertisementHasStarted(individual_advertisements_[0]);

  // Now, simulate the advertisement being released. A new advertisement should
  // have been created.
  ReleaseAdvertisement(individual_advertisements_[0]);
  EXPECT_EQ(2u, register_advertisement_args_.size());
  VerifyServiceDataMatches(register_advertisement_args_[1],
                           individual_advertisements_[0]);

  InvokeCallback(register_advertisement_args_[1]);
  VerifyAdvertisementHasStarted(individual_advertisements_[0]);

  // Now, unregister.
  EXPECT_TRUE(ble_advertiser_->StopAdvertisingToDevice(fake_devices_[0]));
  InvokeLastUnregisterCallbackAndRemove();
  EXPECT_TRUE(individual_advertisements_.empty());
}

// Regression test for crbug.com/739883. This issue arises when the following
// occurs:
//   (1) BleAdvertiser starts advertising to device A.
//   (2) BleAdvertiser stops advertising to device A. The advertisement starts
//       its asynchyronous unregistration flow.
//   (3) BleAdvertiser starts advertising to device A again, but the previous
//       advertisement has not yet been fully unregistered.
// Before the fix for crbug.com/739883, this would cause an error of type
// ERROR_ADVERTISEMENT_ALREADY_EXISTS. However, the fix for this issue ensures
// that the new advertisement in step (3) above does not start until the
// previous one has been finished.
TEST_F(BleAdvertiserTest, SameAdvertisementAdded_FirstHasNotBeenUnregistered) {
  EXPECT_CALL(*mock_adapter_, AddObserver(_)).Times(2);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(_)).Times(2);
  EXPECT_CALL(*mock_adapter_, IsPowered()).Times(3);
  EXPECT_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_)).Times(2);

  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[0]));

  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, individual_advertisements_.size());
  EXPECT_EQ(1u, register_advertisement_args_.size());
  VerifyServiceDataMatches(register_advertisement_args_[0],
                           individual_advertisements_[0]);

  InvokeCallback(register_advertisement_args_[0]);
  VerifyAdvertisementHasStarted(individual_advertisements_[0]);

  // Unregister, but do not invoke the last unregister callback.
  EXPECT_TRUE(ble_advertiser_->StopAdvertisingToDevice(fake_devices_[0]));
  EXPECT_TRUE(individual_advertisements_.empty());

  // Start advertising again, to the same device. A new IndividualAdvertisement
  // should have been created, but a call to RegisterAdvertisement() should NOT
  // have occurred, since the previous advertisement has not yet been
  // unregistered.
  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, individual_advertisements_.size());
  // Should still be only one set of RegisterAdvertisement() args.
  EXPECT_EQ(1u, register_advertisement_args_.size());

  // Now, complete the previous unregistration.
  InvokeLastUnregisterCallbackAndRemove();

  // RegisterAdvertisement() should be called for the new advertisement.
  EXPECT_EQ(2u, register_advertisement_args_.size());

  VerifyServiceDataMatches(register_advertisement_args_[1],
                           individual_advertisements_[0]);
}

TEST_F(BleAdvertiserTest,
       AdvertisementRegisteredAfterIndividualAdvertisementWasDestroyed) {
  EXPECT_CALL(*mock_adapter_, AddObserver(_)).Times(1);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(_)).Times(1);
  EXPECT_CALL(*mock_adapter_, IsPowered()).Times(1);
  EXPECT_CALL(*mock_adapter_, RegisterAdvertisementWithArgsStruct(_)).Times(1);

  mock_eid_generator_->set_advertisement(
      base::MakeUnique<cryptauth::DataWithTimestamp>(fake_advertisements_[0]));

  EXPECT_TRUE(ble_advertiser_->StartAdvertisingToDevice(fake_devices_[0]));
  EXPECT_EQ(1u, individual_advertisements_.size());
  EXPECT_EQ(1u, register_advertisement_args_.size());
  VerifyServiceDataMatches(register_advertisement_args_[0],
                           individual_advertisements_[0]);

  EXPECT_TRUE(ble_advertiser_->StopAdvertisingToDevice(fake_devices_[0]));
  EXPECT_TRUE(individual_advertisements_.empty());

  // Finish advertisement registration, without the IndividualAdvertisement
  // which registered it existing.
  InvokeCallback(register_advertisement_args_[0]);

  // Verify that some mechanism recognized that the advertisement is not owned
  // by any IndividualAdvertisement, and unregistered it. This verification will
  // crash if Unregister() was not called on the advertisement.
  InvokeLastUnregisterCallbackAndRemove();
}

}  // namespace tether

}  // namespace chromeos
