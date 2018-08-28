// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/host_verifier_impl.h"

#include <memory>

#include "base/macros.h"
#include "base/test/simple_test_clock.h"
#include "base/timer/mock_timer.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_host_backend_delegate.h"
#include "chromeos/services/multidevice_setup/fake_host_verifier.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const int64_t kTestTimeMs = 1500000000000;

constexpr const cryptauth::SoftwareFeature kPotentialHostSoftwareFeatures[] = {
    cryptauth::SoftwareFeature::EASY_UNLOCK_HOST,
    cryptauth::SoftwareFeature::MAGIC_TETHER_HOST,
    cryptauth::SoftwareFeature::SMS_CONNECT_HOST};

const char kRetryTimestampPrefName[] =
    "multidevice_setup.current_retry_timestamp_ms";
const char kLastUsedTimeDeltaMsPrefName[] =
    "multidevice_setup.last_used_time_delta_ms";

const int64_t kFirstRetryDeltaMs = 10 * 60 * 1000;
const double kExponentialBackoffMultiplier = 1.5;

enum class HostState { kHostNotSet, kHostSetButNotVerified, kHostVerified };

}  // namespace

class MultiDeviceSetupHostVerifierImplTest : public testing::Test {
 protected:
  MultiDeviceSetupHostVerifierImplTest()
      : test_device_(cryptauth::CreateRemoteDeviceRefForTest()) {}
  ~MultiDeviceSetupHostVerifierImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_host_backend_delegate_ = std::make_unique<FakeHostBackendDelegate>();

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();

    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    HostVerifierImpl::RegisterPrefs(test_pref_service_->registry());

    test_clock_ = std::make_unique<base::SimpleTestClock>();
    test_clock_->SetNow(base::Time::FromJavaTime(kTestTimeMs));
  }

  void TearDown() override {
    if (fake_observer_)
      host_verifier_->RemoveObserver(fake_observer_.get());
  }

  void CreateVerifier(HostState initial_host_state,
                      int64_t initial_timer_pref_value = 0,
                      int64_t initial_time_delta_pref_value = 0) {
    SetHostState(initial_host_state);
    test_pref_service_->SetInt64(kRetryTimestampPrefName,
                                 initial_timer_pref_value);
    test_pref_service_->SetInt64(kLastUsedTimeDeltaMsPrefName,
                                 initial_time_delta_pref_value);

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    host_verifier_ = HostVerifierImpl::Factory::Get()->BuildInstance(
        fake_host_backend_delegate_.get(), fake_device_sync_client_.get(),
        test_pref_service_.get(), test_clock_.get(), std::move(mock_timer));

    fake_observer_ = std::make_unique<FakeHostVerifierObserver>();
    host_verifier_->AddObserver(fake_observer_.get());
  }

  void SetHostState(HostState host_state) {
    for (const auto& feature : kPotentialHostSoftwareFeatures) {
      GetMutableRemoteDevice(test_device_)->software_features[feature] =
          host_state == HostState::kHostVerified
              ? cryptauth::SoftwareFeatureState::kEnabled
              : cryptauth::SoftwareFeatureState::kSupported;
    }

    if (host_state == HostState::kHostNotSet)
      fake_host_backend_delegate_->NotifyHostChangedOnBackend(base::nullopt);
    else
      fake_host_backend_delegate_->NotifyHostChangedOnBackend(test_device_);

    fake_device_sync_client_->NotifyNewDevicesSynced();
  }

  void VerifyState(bool expected_is_verified,
                   size_t expected_num_verified_events,
                   int64_t expected_retry_timestamp_value,
                   int64_t expected_retry_delta_value) {
    EXPECT_EQ(expected_is_verified, host_verifier_->IsHostVerified());
    EXPECT_EQ(expected_num_verified_events,
              fake_observer_->num_host_verifications());
    EXPECT_EQ(expected_retry_timestamp_value,
              test_pref_service_->GetInt64(kRetryTimestampPrefName));
    EXPECT_EQ(expected_retry_delta_value,
              test_pref_service_->GetInt64(kLastUsedTimeDeltaMsPrefName));

    // If a retry timestamp is set, the timer should be running.
    EXPECT_EQ(expected_retry_timestamp_value != 0, mock_timer_->IsRunning());
  }

  void VerifyFindEligibleDevicesCalled() {
    fake_device_sync_client_->InvokePendingFindEligibleDevicesCallback(
        device_sync::mojom::NetworkRequestResult::kSuccess,
        cryptauth::RemoteDeviceRefList() /* eligible_devices */,
        cryptauth::RemoteDeviceRefList() /* ineligible_devices */);
  }

  void SimulateTimePassing(const base::TimeDelta& delta,
                           bool simulate_timeout = false) {
    test_clock_->Advance(delta);

    if (simulate_timeout)
      mock_timer_->Fire();
  }

  FakeHostBackendDelegate* fake_host_backend_delegate() {
    return fake_host_backend_delegate_.get();
  }

 private:
  cryptauth::RemoteDeviceRef test_device_;

  std::unique_ptr<FakeHostVerifierObserver> fake_observer_;
  std::unique_ptr<FakeHostBackendDelegate> fake_host_backend_delegate_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<base::SimpleTestClock> test_clock_;
  base::MockOneShotTimer* mock_timer_ = nullptr;

  std::unique_ptr<HostVerifier> host_verifier_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupHostVerifierImplTest);
};

TEST_F(MultiDeviceSetupHostVerifierImplTest, StartWithoutHost_SetAndVerify) {
  CreateVerifier(HostState::kHostNotSet);

  SetHostState(HostState::kHostSetButNotVerified);
  VerifyFindEligibleDevicesCalled();
  VerifyState(
      false /* expected_is_verified */, 0u /* expected_num_verified_events */,
      kTestTimeMs + kFirstRetryDeltaMs /* expected_retry_timestamp_value */,
      kFirstRetryDeltaMs /* expected_retry_delta_value */);

  SimulateTimePassing(base::TimeDelta::FromMinutes(1));
  SetHostState(HostState::kHostVerified);
  VerifyState(true /* expected_is_verified */,
              1u /* expected_num_verified_events */,
              0 /* expected_retry_timestamp_value */,
              0 /* expected_retry_delta_value */);
}

TEST_F(MultiDeviceSetupHostVerifierImplTest, StartWithoutHost_Retry) {
  CreateVerifier(HostState::kHostNotSet);

  SetHostState(HostState::kHostSetButNotVerified);
  VerifyFindEligibleDevicesCalled();
  VerifyState(
      false /* expected_is_verified */, 0u /* expected_num_verified_events */,
      kTestTimeMs + kFirstRetryDeltaMs /* expected_retry_timestamp_value */,
      kFirstRetryDeltaMs /* expected_retry_delta_value */);

  // Simulate enough time pasing to time out and retry.
  SimulateTimePassing(base::TimeDelta::FromMilliseconds(kFirstRetryDeltaMs),
                      true /* simulate_timeout */);
  VerifyFindEligibleDevicesCalled();
  VerifyState(false /* expected_is_verified */,
              0u /* expected_num_verified_events */,
              kTestTimeMs + kFirstRetryDeltaMs +
                  kFirstRetryDeltaMs * kExponentialBackoffMultiplier
              /* expected_retry_timestamp_value */,
              kFirstRetryDeltaMs * kExponentialBackoffMultiplier
              /* expected_retry_delta_value */);

  // Simulate the next retry timeout passing..
  SimulateTimePassing(base::TimeDelta::FromMilliseconds(
                          kFirstRetryDeltaMs * kExponentialBackoffMultiplier),
                      true /* simulate_timeout */);
  VerifyFindEligibleDevicesCalled();
  VerifyState(false /* expected_is_verified */,
              0u /* expected_num_verified_events */,
              kTestTimeMs + kFirstRetryDeltaMs +
                  kFirstRetryDeltaMs * kExponentialBackoffMultiplier +
                  kFirstRetryDeltaMs * kExponentialBackoffMultiplier *
                      kExponentialBackoffMultiplier
              /* expected_retry_timestamp_value */,
              kFirstRetryDeltaMs * kExponentialBackoffMultiplier *
                  kExponentialBackoffMultiplier
              /* expected_retry_delta_value */);

  // Succeed.
  SetHostState(HostState::kHostVerified);
  VerifyState(true /* expected_is_verified */,
              1u /* expected_num_verified_events */,
              0 /* expected_retry_timestamp_value */,
              0 /* expected_retry_delta_value */);
}

TEST_F(MultiDeviceSetupHostVerifierImplTest,
       StartWithUnverifiedHost_NoInitialPrefs) {
  CreateVerifier(HostState::kHostSetButNotVerified);

  VerifyFindEligibleDevicesCalled();
  VerifyState(
      false /* expected_is_verified */, 0u /* expected_num_verified_events */,
      kTestTimeMs + kFirstRetryDeltaMs /* expected_retry_timestamp_value */,
      kFirstRetryDeltaMs /* expected_retry_delta_value */);
}

TEST_F(MultiDeviceSetupHostVerifierImplTest,
       StartWithUnverifiedHost_InitialPrefs_HasNotPassedRetryTime) {
  // Simulate starting up the device to find that the retry timer is in 5
  // minutes.
  CreateVerifier(HostState::kHostSetButNotVerified,
                 kTestTimeMs + base::TimeDelta::FromMinutes(5).InMilliseconds()
                 /* initial_timer_pref_value */,
                 kFirstRetryDeltaMs /* initial_time_delta_pref_value */);

  SimulateTimePassing(base::TimeDelta::FromMinutes(5),
                      true /* simulate_timeout */);
  VerifyFindEligibleDevicesCalled();
  VerifyState(false /* expected_is_verified */,
              0u /* expected_num_verified_events */,
              kTestTimeMs + base::TimeDelta::FromMinutes(5).InMilliseconds() +
                  kFirstRetryDeltaMs * kExponentialBackoffMultiplier
              /* expected_retry_timestamp_value */,
              kFirstRetryDeltaMs * kExponentialBackoffMultiplier
              /* expected_retry_delta_value */);
}

TEST_F(MultiDeviceSetupHostVerifierImplTest,
       StartWithUnverifiedHost_InitialPrefs_AlreadyPassedRetryTime) {
  // Simulate starting up the device to find that the retry timer had already
  // fired 5 minutes ago.
  CreateVerifier(HostState::kHostSetButNotVerified,
                 kTestTimeMs - base::TimeDelta::FromMinutes(5).InMilliseconds()
                 /* initial_timer_pref_value */,
                 kFirstRetryDeltaMs /* initial_time_delta_pref_value */);

  VerifyFindEligibleDevicesCalled();
  VerifyState(false /* expected_is_verified */,
              0u /* expected_num_verified_events */,
              kTestTimeMs - base::TimeDelta::FromMinutes(5).InMilliseconds() +
                  kFirstRetryDeltaMs * kExponentialBackoffMultiplier
              /* expected_retry_timestamp_value */,
              kFirstRetryDeltaMs * kExponentialBackoffMultiplier
              /* expected_retry_delta_value */);
}

TEST_F(MultiDeviceSetupHostVerifierImplTest,
       StartWithUnverifiedHost_InitialPrefs_AlreadyPassedMultipleRetryTimes) {
  // Simulate starting up the device to find that the retry timer had already
  // fired 20 minutes ago.
  CreateVerifier(HostState::kHostSetButNotVerified,
                 kTestTimeMs - base::TimeDelta::FromMinutes(20).InMilliseconds()
                 /* initial_timer_pref_value */,
                 kFirstRetryDeltaMs /* initial_time_delta_pref_value */);

  // Because the first delta is 10 minutes, the second delta is 10 * 1.5 = 15
  // minutes. In this case, that means that *two* previous timeouts were missed,
  // so the third one should be scheduled.
  VerifyFindEligibleDevicesCalled();
  VerifyState(false /* expected_is_verified */,
              0u /* expected_num_verified_events */,
              kTestTimeMs - base::TimeDelta::FromMinutes(20).InMilliseconds() +
                  kFirstRetryDeltaMs * kExponentialBackoffMultiplier +
                  kFirstRetryDeltaMs * kExponentialBackoffMultiplier *
                      kExponentialBackoffMultiplier
              /* expected_retry_timestamp_value */,
              kFirstRetryDeltaMs * kExponentialBackoffMultiplier *
                  kExponentialBackoffMultiplier
              /* expected_retry_delta_value */);
}

TEST_F(MultiDeviceSetupHostVerifierImplTest,
       StartWithVerifiedHost_HostChanges) {
  CreateVerifier(HostState::kHostVerified);
  VerifyState(true /* expected_is_verified */,
              0u /* expected_num_verified_events */,
              0 /* expected_retry_timestamp_value */,
              0 /* expected_retry_delta_value */);

  SetHostState(HostState::kHostNotSet);
  VerifyState(false /* expected_is_verified */,
              0u /* expected_num_verified_events */,
              0 /* expected_retry_timestamp_value */,
              0 /* expected_retry_delta_value */);

  SetHostState(HostState::kHostSetButNotVerified);
  VerifyFindEligibleDevicesCalled();
  VerifyState(
      false /* expected_is_verified */, 0u /* expected_num_verified_events */,
      kTestTimeMs + kFirstRetryDeltaMs /* expected_retry_timestamp_value */,
      kFirstRetryDeltaMs /* expected_retry_delta_value */);
}

TEST_F(MultiDeviceSetupHostVerifierImplTest,
       StartWithVerifiedHost_PendingRemoval) {
  CreateVerifier(HostState::kHostVerified);
  VerifyState(true /* expected_is_verified */,
              0u /* expected_num_verified_events */,
              0 /* expected_retry_timestamp_value */,
              0 /* expected_retry_delta_value */);

  fake_host_backend_delegate()->AttemptToSetMultiDeviceHostOnBackend(
      base::nullopt /* host_device */);
  VerifyState(false /* expected_is_verified */,
              0u /* expected_num_verified_events */,
              0 /* expected_retry_timestamp_value */,
              0 /* expected_retry_delta_value */);
}

}  // namespace multidevice_setup

}  // namespace chromeos
