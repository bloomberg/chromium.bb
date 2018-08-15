// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/feature_state_manager_impl.h"

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_feature_state_manager.h"
#include "chromeos/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

// TODO(jordynass): Use constants declared in
// chromeos/services/multidevice_setup/public/cpp/prefs.h once migration is
// complete, then delete these fields which are duplicates.
const char kSmartLockFeatureEnabledPrefName[] = "easy_unlock.enabled";
const char kSmartLockFeatureAllowedPrefName[] = "easy_unlock.allowed";

cryptauth::RemoteDeviceRef CreateTestHostDevice() {
  cryptauth::RemoteDeviceRef host_device =
      cryptauth::CreateRemoteDeviceRefForTest();

  // Set all host features to supported.
  cryptauth::RemoteDevice* raw_device =
      cryptauth::GetMutableRemoteDevice(host_device);
  raw_device
      ->software_features[cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST] =
      cryptauth::SoftwareFeatureState::kSupported;
  raw_device->software_features[cryptauth::SoftwareFeature::EASY_UNLOCK_HOST] =
      cryptauth::SoftwareFeatureState::kSupported;
  raw_device->software_features[cryptauth::SoftwareFeature::MAGIC_TETHER_HOST] =
      cryptauth::SoftwareFeatureState::kSupported;
  raw_device->software_features[cryptauth::SoftwareFeature::SMS_CONNECT_HOST] =
      cryptauth::SoftwareFeatureState::kSupported;

  return host_device;
}

}  // namespace

class MultiDeviceSetupFeatureStateManagerImplTest : public testing::Test {
 protected:
  MultiDeviceSetupFeatureStateManagerImplTest()
      : test_local_device_(cryptauth::CreateRemoteDeviceRefForTest()),
        test_host_device_(CreateTestHostDevice()) {}
  ~MultiDeviceSetupFeatureStateManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    user_prefs::PrefRegistrySyncable* registry = test_pref_service_->registry();
    RegisterFeaturePrefs(registry);
    // TODO(jordynass): Remove the registration of these preferences once they
    // are migrated.
    registry->RegisterBooleanPref(kSmartLockFeatureAllowedPrefName, true);
    registry->RegisterBooleanPref(kSmartLockFeatureEnabledPrefName, true);

    fake_host_status_provider_ = std::make_unique<FakeHostStatusProvider>();

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->set_synced_devices(
        cryptauth::RemoteDeviceRefList{test_local_device_, test_host_device_});
    fake_device_sync_client_->set_local_device_metadata(test_local_device_);

    manager_ = FeatureStateManagerImpl::Factory::Get()->BuildInstance(
        test_pref_service_.get(), fake_host_status_provider_.get(),
        fake_device_sync_client_.get());

    fake_observer_ = std::make_unique<FakeFeatureStateManagerObserver>();
    manager_->AddObserver(fake_observer_.get());
  }

  void TryAllUnverifiedHostStatesAndVerifyFeatureState(mojom::Feature feature) {
    bool was_previously_verified =
        fake_host_status_provider_->GetHostWithStatus().host_status() ==
        mojom::HostStatus::kHostVerified;
    size_t num_observer_events_before_call =
        fake_observer_->feature_state_updates().size();
    size_t expected_num_observer_events_after_call =
        num_observer_events_before_call + (was_previously_verified ? 1u : 0u);

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kNoEligibleHosts, base::nullopt /* host_device */);
    if (was_previously_verified) {
      VerifyFeatureStateChange(num_observer_events_before_call, feature,
                               mojom::FeatureState::kUnavailableNoVerifiedHost);
    }
    EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kEligibleHostExistsButNoHostSet,
        base::nullopt /* host_device */);
    EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
        test_host_device_);
    EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());

    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kHostSetButNotYetVerified, test_host_device_);
    EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
              manager_->GetFeatureStates()[feature]);
    EXPECT_EQ(expected_num_observer_events_after_call,
              fake_observer_->feature_state_updates().size());
  }

  void SetVerifiedHost() {
    // Should not already be verified if we are setting it to be verified.
    EXPECT_NE(mojom::HostStatus::kHostVerified,
              fake_host_status_provider_->GetHostWithStatus().host_status());

    size_t num_observer_events_before_call =
        fake_observer_->feature_state_updates().size();

    SetSoftwareFeatureState(false /* use_local_device */,
                            cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
                            cryptauth::SoftwareFeatureState::kEnabled);
    fake_host_status_provider_->SetHostWithStatus(
        mojom::HostStatus::kHostVerified, test_host_device_);

    // Since the host is now verified, there should be a feature state update
    // for all features.
    EXPECT_EQ(num_observer_events_before_call + 1u,
              fake_observer_->feature_state_updates().size());
  }

  void VerifyFeatureStateChange(size_t expected_index,
                                mojom::Feature expected_feature,
                                mojom::FeatureState expected_feature_state) {
    const FeatureStateManager::FeatureStatesMap& map =
        fake_observer_->feature_state_updates()[expected_index];
    const auto it = map.find(expected_feature);
    EXPECT_NE(map.end(), it);
    EXPECT_EQ(expected_feature_state, it->second);
  }

  void SetSoftwareFeatureState(
      bool use_local_device,
      cryptauth::SoftwareFeature software_feature,
      cryptauth::SoftwareFeatureState software_feature_state) {
    cryptauth::RemoteDeviceRef& device =
        use_local_device ? test_local_device_ : test_host_device_;
    cryptauth::GetMutableRemoteDevice(device)
        ->software_features[software_feature] = software_feature_state;
    fake_device_sync_client_->NotifyNewDevicesSynced();
  }

  sync_preferences::TestingPrefServiceSyncable* test_pref_service() {
    return test_pref_service_.get();
  }

  FeatureStateManager* manager() { return manager_.get(); }

 private:
  cryptauth::RemoteDeviceRef test_local_device_;
  cryptauth::RemoteDeviceRef test_host_device_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<FakeHostStatusProvider> fake_host_status_provider_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;

  std::unique_ptr<FakeFeatureStateManagerObserver> fake_observer_;

  std::unique_ptr<FeatureStateManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupFeatureStateManagerImplTest);
};

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, BetterTogetherSuite) {
  TryAllUnverifiedHostStatesAndVerifyFeatureState(
      mojom::Feature::kBetterTogetherSuite);

  SetVerifiedHost();
  EXPECT_EQ(
      mojom::FeatureState::kNotSupportedByChromebook,
      manager()->GetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);

  SetSoftwareFeatureState(true /* use_local_device */,
                          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
                          cryptauth::SoftwareFeatureState::kSupported);
  EXPECT_EQ(
      mojom::FeatureState::kEnabledByUser,
      manager()->GetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  VerifyFeatureStateChange(1u /* expected_index */,
                           mojom::Feature::kBetterTogetherSuite,
                           mojom::FeatureState::kEnabledByUser);

  test_pref_service()->SetBoolean(kSuiteEnabledPrefName, false);
  EXPECT_EQ(
      mojom::FeatureState::kDisabledByUser,
      manager()->GetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  VerifyFeatureStateChange(2u /* expected_index */,
                           mojom::Feature::kBetterTogetherSuite,
                           mojom::FeatureState::kDisabledByUser);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, InstantTethering) {
  TryAllUnverifiedHostStatesAndVerifyFeatureState(
      mojom::Feature::kInstantTethering);

  SetVerifiedHost();
  EXPECT_EQ(mojom::FeatureState::kNotSupportedByChromebook,
            manager()->GetFeatureStates()[mojom::Feature::kInstantTethering]);

  SetSoftwareFeatureState(true /* use_local_device */,
                          cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT,
                          cryptauth::SoftwareFeatureState::kSupported);
  EXPECT_EQ(mojom::FeatureState::kNotSupportedByPhone,
            manager()->GetFeatureStates()[mojom::Feature::kInstantTethering]);
  VerifyFeatureStateChange(1u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kNotSupportedByPhone);

  SetSoftwareFeatureState(false /* use_local_device */,
                          cryptauth::SoftwareFeature::MAGIC_TETHER_HOST,
                          cryptauth::SoftwareFeatureState::kEnabled);
  EXPECT_EQ(mojom::FeatureState::kEnabledByUser,
            manager()->GetFeatureStates()[mojom::Feature::kInstantTethering]);
  VerifyFeatureStateChange(2u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kEnabledByUser);

  test_pref_service()->SetBoolean(kInstantTetheringFeatureEnabledPrefName,
                                  false);
  EXPECT_EQ(mojom::FeatureState::kDisabledByUser,
            manager()->GetFeatureStates()[mojom::Feature::kInstantTethering]);
  VerifyFeatureStateChange(3u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kDisabledByUser);

  test_pref_service()->SetBoolean(kInstantTetheringFeatureAllowedPrefName,
                                  false);
  EXPECT_EQ(mojom::FeatureState::kDisabledByPolicy,
            manager()->GetFeatureStates()[mojom::Feature::kInstantTethering]);
  VerifyFeatureStateChange(4u /* expected_index */,
                           mojom::Feature::kInstantTethering,
                           mojom::FeatureState::kDisabledByPolicy);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, Messages) {
  TryAllUnverifiedHostStatesAndVerifyFeatureState(mojom::Feature::kMessages);

  SetVerifiedHost();
  EXPECT_EQ(mojom::FeatureState::kNotSupportedByChromebook,
            manager()->GetFeatureStates()[mojom::Feature::kMessages]);

  SetSoftwareFeatureState(true /* use_local_device */,
                          cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT,
                          cryptauth::SoftwareFeatureState::kSupported);
  EXPECT_EQ(mojom::FeatureState::kNotSupportedByPhone,
            manager()->GetFeatureStates()[mojom::Feature::kMessages]);
  VerifyFeatureStateChange(1u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kNotSupportedByPhone);

  SetSoftwareFeatureState(false /* use_local_device */,
                          cryptauth::SoftwareFeature::SMS_CONNECT_HOST,
                          cryptauth::SoftwareFeatureState::kEnabled);
  EXPECT_EQ(mojom::FeatureState::kEnabledByUser,
            manager()->GetFeatureStates()[mojom::Feature::kMessages]);
  VerifyFeatureStateChange(2u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kEnabledByUser);

  test_pref_service()->SetBoolean(kAndroidMessagesFeatureEnabledPrefName,
                                  false);
  EXPECT_EQ(mojom::FeatureState::kDisabledByUser,
            manager()->GetFeatureStates()[mojom::Feature::kMessages]);
  VerifyFeatureStateChange(3u /* expected_index */, mojom::Feature::kMessages,
                           mojom::FeatureState::kDisabledByUser);
}

TEST_F(MultiDeviceSetupFeatureStateManagerImplTest, SmartLock) {
  TryAllUnverifiedHostStatesAndVerifyFeatureState(mojom::Feature::kSmartLock);

  SetVerifiedHost();
  EXPECT_EQ(mojom::FeatureState::kNotSupportedByChromebook,
            manager()->GetFeatureStates()[mojom::Feature::kSmartLock]);

  SetSoftwareFeatureState(true /* use_local_device */,
                          cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT,
                          cryptauth::SoftwareFeatureState::kSupported);
  EXPECT_EQ(mojom::FeatureState::kUnavailableInsufficientSecurity,
            manager()->GetFeatureStates()[mojom::Feature::kSmartLock]);
  VerifyFeatureStateChange(
      1u /* expected_index */, mojom::Feature::kSmartLock,
      mojom::FeatureState::kUnavailableInsufficientSecurity);

  SetSoftwareFeatureState(false /* use_local_device */,
                          cryptauth::SoftwareFeature::EASY_UNLOCK_HOST,
                          cryptauth::SoftwareFeatureState::kEnabled);
  EXPECT_EQ(mojom::FeatureState::kEnabledByUser,
            manager()->GetFeatureStates()[mojom::Feature::kSmartLock]);
  VerifyFeatureStateChange(2u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kEnabledByUser);

  test_pref_service()->SetBoolean(kSmartLockFeatureEnabledPrefName, false);
  EXPECT_EQ(mojom::FeatureState::kDisabledByUser,
            manager()->GetFeatureStates()[mojom::Feature::kSmartLock]);
  VerifyFeatureStateChange(3u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kDisabledByUser);

  test_pref_service()->SetBoolean(kSmartLockFeatureAllowedPrefName, false);
  EXPECT_EQ(mojom::FeatureState::kDisabledByPolicy,
            manager()->GetFeatureStates()[mojom::Feature::kSmartLock]);
  VerifyFeatureStateChange(4u /* expected_index */, mojom::Feature::kSmartLock,
                           mojom::FeatureState::kDisabledByPolicy);
}

}  // namespace multidevice_setup

}  // namespace chromeos
