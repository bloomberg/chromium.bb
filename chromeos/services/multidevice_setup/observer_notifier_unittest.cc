// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/observer_notifier.h"

#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_multidevice_setup_observer.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {
const int64_t kTestTimeMillis = 1500000000000;

cryptauth::RemoteDeviceRef BuildDeviceRef(
    cryptauth::SoftwareFeatureState better_together_host_state) {
  return cryptauth::RemoteDeviceRefBuilder()
      .SetSoftwareFeatureState(cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
                               better_together_host_state)
      .Build();
}
}  // namespace

class ObserverNotifierTest : public testing::Test {
 protected:
  ObserverNotifierTest() = default;
  ~ObserverNotifierTest() override = default;

  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    ObserverNotifier::RegisterPrefs(test_pref_service_->registry());

    fake_observer_ = std::make_unique<FakeMultiDeviceSetupObserver>();

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    test_clock_ = std::make_unique<base::SimpleTestClock>();

    test_clock_->SetNow(base::Time::FromJavaTime(kTestTimeMillis));

    observer_notifier_ = ObserverNotifier::Factory::Get()->BuildInstance(
        fake_device_sync_client_.get(), test_pref_service_.get(),
        test_clock_.get());
  }

  void TearDown() override { VerifyNoPendingMojoMessages(); }

  void SendPendingMojoMessages() { observer_notifier_->FlushForTesting(); }

  void VerifyNoPendingMojoMessages() {
    EXPECT_FALSE(observer_notifier_->observer_ptr_.IsExpectingResponse());
  }

  void SetNewUserPotentialHostExistsTimestamp(int64_t timestamp) {
    test_pref_service_->SetInt64(ObserverNotifier::kNewUserPotentialHostExists,
                                 timestamp);
  }

  void SetMultiDeviceSetupObserverPtr() {
    observer_notifier_->SetMultiDeviceSetupObserverPtr(
        fake_observer_->GenerateInterfacePtr());
  }

  void NotifyNewDevicesSynced() {
    fake_device_sync_client_->NotifyNewDevicesSynced();
  }

  void SetSyncedDevices(cryptauth::RemoteDeviceRefList device_ref_list) {
    fake_device_sync_client_->set_synced_devices(device_ref_list);
  }

  int64_t GetNewUserPotentialHostExistsTimestamp() {
    return test_pref_service_->GetInt64(
        ObserverNotifier::kNewUserPotentialHostExists);
  }

  FakeMultiDeviceSetupObserver* fake_observer() { return fake_observer_.get(); }

 private:
  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<FakeMultiDeviceSetupObserver> fake_observer_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<base::SimpleTestClock> test_clock_;

  std::unique_ptr<ObserverNotifier> observer_notifier_;

  DISALLOW_COPY_AND_ASSIGN(ObserverNotifierTest);
};

TEST_F(ObserverNotifierTest, SetObserverWithSupportedHost) {
  SetSyncedDevices(cryptauth::RemoteDeviceRefList{
      BuildDeviceRef(cryptauth::SoftwareFeatureState::kNotSupported),
      BuildDeviceRef(cryptauth::SoftwareFeatureState::kSupported)});
  SetMultiDeviceSetupObserverPtr();
  SendPendingMojoMessages();
  EXPECT_EQ(1u, fake_observer()->num_new_user_events_handled());
  EXPECT_EQ(kTestTimeMillis, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(ObserverNotifierTest, SupportedHostAddedLater) {
  SetSyncedDevices(cryptauth::RemoteDeviceRefList{
      BuildDeviceRef(cryptauth::SoftwareFeatureState::kNotSupported)});
  SetMultiDeviceSetupObserverPtr();
  VerifyNoPendingMojoMessages();
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());

  SetSyncedDevices(cryptauth::RemoteDeviceRefList{
      BuildDeviceRef(cryptauth::SoftwareFeatureState::kNotSupported),
      BuildDeviceRef(cryptauth::SoftwareFeatureState::kSupported)});
  NotifyNewDevicesSynced();
  SendPendingMojoMessages();
  EXPECT_EQ(1u, fake_observer()->num_new_user_events_handled());
  EXPECT_EQ(kTestTimeMillis, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(ObserverNotifierTest, CannotHaveEnabledHost) {
  SetSyncedDevices(cryptauth::RemoteDeviceRefList{
      BuildDeviceRef(cryptauth::SoftwareFeatureState::kNotSupported),
      BuildDeviceRef(cryptauth::SoftwareFeatureState::kSupported),
      BuildDeviceRef(cryptauth::SoftwareFeatureState::kEnabled)});
  SetMultiDeviceSetupObserverPtr();
}

TEST_F(ObserverNotifierTest, ObserverMustBeSet) {
  SetSyncedDevices(cryptauth::RemoteDeviceRefList{
      BuildDeviceRef(cryptauth::SoftwareFeatureState::kNotSupported),
      BuildDeviceRef(cryptauth::SoftwareFeatureState::kSupported)});
  NotifyNewDevicesSynced();
}

TEST_F(ObserverNotifierTest, OldTimestampInPreferencesPreventsAction) {
  SetSyncedDevices(cryptauth::RemoteDeviceRefList{
      BuildDeviceRef(cryptauth::SoftwareFeatureState::kNotSupported),
      BuildDeviceRef(cryptauth::SoftwareFeatureState::kSupported)});
  int64_t different_test_time_millis = kTestTimeMillis / 2;
  SetNewUserPotentialHostExistsTimestamp(different_test_time_millis);

  SetMultiDeviceSetupObserverPtr();
  // Observer was not notified
  VerifyNoPendingMojoMessages();
  // Timestamp was not overwritten by clock
  EXPECT_EQ(different_test_time_millis,
            GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(ObserverNotifierTest, MultipleSyncsOnlyTriggerOneEvent) {
  SetSyncedDevices(cryptauth::RemoteDeviceRefList{
      BuildDeviceRef(cryptauth::SoftwareFeatureState::kSupported)});
  SetMultiDeviceSetupObserverPtr();
  SendPendingMojoMessages();
  EXPECT_EQ(1u, fake_observer()->num_new_user_events_handled());
  NotifyNewDevicesSynced();
  VerifyNoPendingMojoMessages();
  NotifyNewDevicesSynced();
}
}  // namespace multidevice_setup

}  // namespace chromeos
