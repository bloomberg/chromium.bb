// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/eligible_host_devices_provider_impl.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/multidevice/software_feature.h"
#include "chromeos/components/multidevice/software_feature_state.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const size_t kNumTestDevices = 5;

}  // namespace

class MultiDeviceSetupEligibleHostDevicesProviderImplTest
    : public testing::Test {
 protected:
  MultiDeviceSetupEligibleHostDevicesProviderImplTest()
      : test_devices_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~MultiDeviceSetupEligibleHostDevicesProviderImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_synced_devices(test_devices_);

    provider_ = EligibleHostDevicesProviderImpl::Factory::Get()->BuildInstance(
        fake_device_sync_client_.get());
  }

  device_sync::FakeDeviceSyncClient* fake_device_sync_client() {
    return fake_device_sync_client_.get();
  }

  multidevice::RemoteDeviceRefList& test_devices() { return test_devices_; }

  EligibleHostDevicesProvider* provider() { return provider_.get(); }

 private:
  multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;

  std::unique_ptr<EligibleHostDevicesProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupEligibleHostDevicesProviderImplTest);
};

TEST_F(MultiDeviceSetupEligibleHostDevicesProviderImplTest, Empty) {
  EXPECT_TRUE(provider()->GetEligibleHostDevices().empty());
}

TEST_F(MultiDeviceSetupEligibleHostDevicesProviderImplTest, NoEligibleDevices) {
  GetMutableRemoteDevice(test_devices()[0])
      ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kNotSupported;
  GetMutableRemoteDevice(test_devices()[1])
      ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kNotSupported;

  multidevice::RemoteDeviceRefList devices{test_devices()[0],
                                           test_devices()[1]};
  fake_device_sync_client()->set_synced_devices(devices);
  fake_device_sync_client()->NotifyNewDevicesSynced();

  EXPECT_TRUE(provider()->GetEligibleHostDevices().empty());
}

TEST_F(MultiDeviceSetupEligibleHostDevicesProviderImplTest,
       SupportedAndEnabled) {
  // Devices 0, 1, and 2 are supported.
  GetMutableRemoteDevice(test_devices()[0])
      ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kSupported;
  GetMutableRemoteDevice(test_devices()[1])
      ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kSupported;
  GetMutableRemoteDevice(test_devices()[2])
      ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kSupported;

  // Device 3 is enabled.
  GetMutableRemoteDevice(test_devices()[3])
      ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kEnabled;

  // Device 4 is not supported.
  GetMutableRemoteDevice(test_devices()[4])
      ->software_features[multidevice::SoftwareFeature::kBetterTogetherHost] =
      multidevice::SoftwareFeatureState::kNotSupported;

  multidevice::RemoteDeviceRefList devices{test_devices()[0], test_devices()[1],
                                           test_devices()[2], test_devices()[3],
                                           test_devices()[4]};
  fake_device_sync_client()->set_synced_devices(devices);
  fake_device_sync_client()->NotifyNewDevicesSynced();

  base::flat_set<multidevice::RemoteDeviceRef> eligible_devices =
      provider()->GetEligibleHostDevices();
  EXPECT_TRUE(base::Contains(eligible_devices, test_devices()[0]));
  EXPECT_TRUE(base::Contains(eligible_devices, test_devices()[1]));
  EXPECT_TRUE(base::Contains(eligible_devices, test_devices()[2]));
  EXPECT_TRUE(base::Contains(eligible_devices, test_devices()[3]));
  EXPECT_FALSE(base::Contains(eligible_devices, test_devices()[4]));
}

}  // namespace multidevice_setup

}  // namespace chromeos
