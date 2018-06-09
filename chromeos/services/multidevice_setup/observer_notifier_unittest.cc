// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_multidevice_setup_observer.h"
#include "chromeos/services/multidevice_setup/fake_setup_flow_completion_recorder.h"
#include "chromeos/services/multidevice_setup/observer_notifier.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {
const int64_t kTestTimeMillis = 1500000000000;
constexpr const char* const kDummyKeys[] = {"publicKey0", "publicKey1",
                                            "publicKey2"};
const char kLocalDeviceKey[] = "publicKeyOfLocalDevice";

cryptauth::RemoteDeviceRef BuildHostCandidate(
    std::string public_key,
    cryptauth::SoftwareFeatureState better_together_host_state) {
  return cryptauth::RemoteDeviceRefBuilder()
      .SetPublicKey(public_key)
      .SetSoftwareFeatureState(cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
                               better_together_host_state)
      .Build();
}

cryptauth::RemoteDeviceRef BuildLocalDevice() {
  return cryptauth::RemoteDeviceRefBuilder()
      .SetPublicKey(kLocalDeviceKey)
      .SetSoftwareFeatureState(
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
          cryptauth::SoftwareFeatureState::kSupported)
      .Build();
}
}  // namespace

class ObserverNotifierTest : public testing::Test {
 protected:
  ObserverNotifierTest() : local_device_(BuildLocalDevice()) {}

  ~ObserverNotifierTest() override = default;

  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    ObserverNotifier::RegisterPrefs(test_pref_service_->registry());

    fake_observer_ = std::make_unique<FakeMultiDeviceSetupObserver>();

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    test_clock_ = std::make_unique<base::SimpleTestClock>();
    fake_setup_flow_completion_recorder_ =
        std::make_unique<FakeSetupFlowCompletionRecorder>();

    fake_device_sync_client_->set_local_device_metadata(local_device_);
    test_clock_->SetNow(base::Time::FromJavaTime(kTestTimeMillis));
  }

  void BuildObserverNotifier() {
    observer_notifier_ = ObserverNotifier::Factory::Get()->BuildInstance(
        fake_device_sync_client_.get(), test_pref_service_.get(),
        fake_setup_flow_completion_recorder_.get(), test_clock_.get());
  }

  void SetNewUserPotentialHostExistsTimestamp(int64_t timestamp) {
    test_pref_service_->SetInt64(
        ObserverNotifier::kNewUserPotentialHostExistsPrefName, timestamp);
  }

  void SetExistingUserChromebookAddedTimestamp(int64_t timestamp) {
    test_pref_service_->SetInt64(
        ObserverNotifier::kExistingUserChromebookAddedPrefName, timestamp);
  }

  void SetHostFromPreviousSession(std::string old_host_key) {
    test_pref_service_->SetString(
        ObserverNotifier::kHostPublicKeyFromMostRecentSyncPrefName,
        old_host_key);
  }

  void SetMultiDeviceSetupObserverPtr() {
    observer_notifier_->SetMultiDeviceSetupObserverPtr(
        fake_observer_->GenerateInterfacePtr());
    observer_notifier_->FlushForTesting();
  }

  void RecordSetupFlowCompletionAtEarlierTime(const base::Time& earlier_time) {
    fake_setup_flow_completion_recorder_->set_current_time(earlier_time);
    observer_notifier_->setup_flow_completion_recorder_->RecordCompletion();
  }

  void NotifyNewDevicesSynced() {
    fake_device_sync_client_->NotifyNewDevicesSynced();
    observer_notifier_->FlushForTesting();
  }

  // Represents setting preexisting devices on account and therefore does not
  // trigger a notification.
  void SetNonLocalSyncedDevices(
      cryptauth::RemoteDeviceRefList* device_ref_list) {
    // The local device should always be on the device list.
    device_ref_list->push_back(local_device_);
    fake_device_sync_client_->set_synced_devices(*device_ref_list);
  }

  void UpdateNonLocalSyncedDevices(
      cryptauth::RemoteDeviceRefList* device_ref_list) {
    SetNonLocalSyncedDevices(device_ref_list);
    NotifyNewDevicesSynced();
  }

  // Setup to simulate a the situation when the local device is ready to be
  // Better Together client enabled (i.e. enabling and syncing would trigger a
  // Chromebook added event) with a single enabled host.
  void PrepareToEnableLocalDeviceAsClient() {
    cryptauth::RemoteDeviceRefList device_ref_list =
        cryptauth::RemoteDeviceRefList{BuildHostCandidate(
            kDummyKeys[0], cryptauth::SoftwareFeatureState::kEnabled)};
    SetNonLocalSyncedDevices(&device_ref_list);
    BuildObserverNotifier();
  }

  // If the account as an enabled host, the local device will enable its
  // MultiDevice client feature in the background.
  void EnableLocalDeviceAsClient() {
    cryptauth::GetMutableRemoteDevice(local_device_)
        ->software_features
            [cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT] =
        cryptauth::SoftwareFeatureState::kEnabled;
  }

  int64_t GetNewUserPotentialHostExistsTimestamp() {
    return test_pref_service_->GetInt64(
        ObserverNotifier::kNewUserPotentialHostExistsPrefName);
  }

  int64_t GetExistingUserChromebookAddedTimestamp() {
    return test_pref_service_->GetInt64(
        ObserverNotifier::kExistingUserChromebookAddedPrefName);
  }

  FakeMultiDeviceSetupObserver* fake_observer() { return fake_observer_.get(); }

 private:
  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  cryptauth::RemoteDeviceRef local_device_;

  std::unique_ptr<FakeMultiDeviceSetupObserver> fake_observer_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<FakeSetupFlowCompletionRecorder>
      fake_setup_flow_completion_recorder_;
  std::unique_ptr<base::SimpleTestClock> test_clock_;

  std::unique_ptr<ObserverNotifier> observer_notifier_;

  DISALLOW_COPY_AND_ASSIGN(ObserverNotifierTest);
};

TEST_F(ObserverNotifierTest, SetObserverWithSupportedHost) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kNotSupported),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildObserverNotifier();

  SetMultiDeviceSetupObserverPtr();
  EXPECT_EQ(1u, fake_observer()->num_new_user_events_handled());
  EXPECT_EQ(kTestTimeMillis, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(ObserverNotifierTest, SupportedHostAddedLater) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{BuildHostCandidate(
          kDummyKeys[0], cryptauth::SoftwareFeatureState::kNotSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildObserverNotifier();

  SetMultiDeviceSetupObserverPtr();
  EXPECT_EQ(0u, fake_observer()->num_new_user_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());

  device_ref_list = cryptauth::RemoteDeviceRefList{
      BuildHostCandidate(kDummyKeys[0],
                         cryptauth::SoftwareFeatureState::kNotSupported),
      BuildHostCandidate(kDummyKeys[1],
                         cryptauth::SoftwareFeatureState::kSupported)};
  UpdateNonLocalSyncedDevices(&device_ref_list);
  EXPECT_EQ(1u, fake_observer()->num_new_user_events_handled());
  EXPECT_EQ(kTestTimeMillis, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(ObserverNotifierTest, ExistingEnabledHostPreventsNewUserEvent) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kNotSupported),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported),
          BuildHostCandidate(kDummyKeys[2],
                             cryptauth::SoftwareFeatureState::kEnabled)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildObserverNotifier();

  SetMultiDeviceSetupObserverPtr();
  EXPECT_EQ(0u, fake_observer()->num_new_user_events_handled());
}

TEST_F(ObserverNotifierTest, NoNewUserEventWithoutObserverSet) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kNotSupported),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildObserverNotifier();
  // All conditions for new user event are now satisfied except for setting an
  // observer.

  // No observer is set before the sync.
  NotifyNewDevicesSynced();
  EXPECT_EQ(0u, fake_observer()->num_new_user_events_handled());
}

TEST_F(ObserverNotifierTest, OldTimestampInPreferencesPreventsNewUserFlow) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kNotSupported),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildObserverNotifier();
  int64_t earlier_test_time_millis = kTestTimeMillis / 2;
  SetNewUserPotentialHostExistsTimestamp(earlier_test_time_millis);

  SetMultiDeviceSetupObserverPtr();
  EXPECT_EQ(0u, fake_observer()->num_new_user_events_handled());
  // Timestamp was not overwritten by clock.
  EXPECT_EQ(earlier_test_time_millis, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(ObserverNotifierTest, MultipleSyncsOnlyTriggerOneNewUserEvent) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{BuildHostCandidate(
          kDummyKeys[0], cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildObserverNotifier();

  SetMultiDeviceSetupObserverPtr();
  EXPECT_EQ(1u, fake_observer()->num_new_user_events_handled());

  NotifyNewDevicesSynced();
  EXPECT_EQ(1u, fake_observer()->num_new_user_events_handled());

  NotifyNewDevicesSynced();
  EXPECT_EQ(1u, fake_observer()->num_new_user_events_handled());
}

TEST_F(ObserverNotifierTest, NotifiesObserverForHostSwitchEvents) {
  const std::string initial_host_key = kDummyKeys[0];
  const std::string alternative_host_key_1 = kDummyKeys[1];
  const std::string alternative_host_key_2 = kDummyKeys[2];
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(initial_host_key,
                             cryptauth::SoftwareFeatureState::kEnabled),
          BuildHostCandidate(alternative_host_key_1,
                             cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildObserverNotifier();
  EnableLocalDeviceAsClient();

  SetMultiDeviceSetupObserverPtr();
  // Verify the observer initializes to 0.
  EXPECT_EQ(0u,
            fake_observer()->num_existing_user_host_switched_events_handled());

  // Switch hosts.
  device_ref_list = cryptauth::RemoteDeviceRefList{
      BuildHostCandidate(initial_host_key,
                         cryptauth::SoftwareFeatureState::kSupported),
      BuildHostCandidate(alternative_host_key_1,
                         cryptauth::SoftwareFeatureState::kEnabled)};
  UpdateNonLocalSyncedDevices(&device_ref_list);
  EXPECT_EQ(1u,
            fake_observer()->num_existing_user_host_switched_events_handled());

  // Adding and enabling a new host device (counts as a switch).
  device_ref_list = cryptauth::RemoteDeviceRefList{
      BuildHostCandidate(initial_host_key,
                         cryptauth::SoftwareFeatureState::kSupported),
      BuildHostCandidate(alternative_host_key_1,
                         cryptauth::SoftwareFeatureState::kSupported),
      BuildHostCandidate(alternative_host_key_2,
                         cryptauth::SoftwareFeatureState::kEnabled)};
  UpdateNonLocalSyncedDevices(&device_ref_list);
  EXPECT_EQ(2u,
            fake_observer()->num_existing_user_host_switched_events_handled());

  // Removing new device and switching back to initial host.
  device_ref_list = cryptauth::RemoteDeviceRefList{
      BuildHostCandidate(initial_host_key,
                         cryptauth::SoftwareFeatureState::kEnabled),
      BuildHostCandidate(alternative_host_key_1,
                         cryptauth::SoftwareFeatureState::kSupported)};
  UpdateNonLocalSyncedDevices(&device_ref_list);
  EXPECT_EQ(3u,
            fake_observer()->num_existing_user_host_switched_events_handled());
}

TEST_F(ObserverNotifierTest, HostSwitchedBetweenSessions) {
  const std::string old_host_key = kDummyKeys[0];
  const std::string new_host_key = kDummyKeys[1];
  SetHostFromPreviousSession(old_host_key);
  // Host switched between sessions.
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{BuildHostCandidate(
          new_host_key, cryptauth::SoftwareFeatureState::kEnabled)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildObserverNotifier();
  EnableLocalDeviceAsClient();

  SetMultiDeviceSetupObserverPtr();
  EXPECT_EQ(1u,
            fake_observer()->num_existing_user_host_switched_events_handled());
}

TEST_F(ObserverNotifierTest,
       NoHostSwitchedEventWithoutLocalDeviceClientEnabled) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kEnabled),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildObserverNotifier();

  SetMultiDeviceSetupObserverPtr();

  // Switch hosts.
  device_ref_list = cryptauth::RemoteDeviceRefList{
      BuildHostCandidate(kDummyKeys[0],
                         cryptauth::SoftwareFeatureState::kSupported),
      BuildHostCandidate(kDummyKeys[1],
                         cryptauth::SoftwareFeatureState::kEnabled)};
  // All conditions for host switched event are now satisfied except for the
  // local device's Better Together client feature enabled.

  // No observer is set before the update (which includes a sync).
  UpdateNonLocalSyncedDevices(&device_ref_list);
  EXPECT_EQ(0u,
            fake_observer()->num_existing_user_host_switched_events_handled());
}

TEST_F(ObserverNotifierTest, NoHostSwitchedEventWithoutExistingHost) {
  // No enabled host initially.
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kSupported),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildObserverNotifier();

  SetMultiDeviceSetupObserverPtr();

  // Enable one of the host devices.
  device_ref_list = cryptauth::RemoteDeviceRefList{
      BuildHostCandidate(kDummyKeys[0],
                         cryptauth::SoftwareFeatureState::kEnabled),
      BuildHostCandidate(kDummyKeys[1],
                         cryptauth::SoftwareFeatureState::kSupported)};
  EnableLocalDeviceAsClient();
  UpdateNonLocalSyncedDevices(&device_ref_list);
  EXPECT_EQ(0u,
            fake_observer()->num_existing_user_host_switched_events_handled());
}

TEST_F(ObserverNotifierTest, NoHostSwitchedEventWithoutObserverSet) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kEnabled),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildObserverNotifier();
  EnableLocalDeviceAsClient();

  // Switch hosts.
  device_ref_list = cryptauth::RemoteDeviceRefList{
      BuildHostCandidate(kDummyKeys[0],
                         cryptauth::SoftwareFeatureState::kSupported),
      BuildHostCandidate(kDummyKeys[1],
                         cryptauth::SoftwareFeatureState::kEnabled)};
  // All conditions for host switched event are now satisfied except for setting
  // an observer.

  // No observer is set before the update (which includes a sync).
  UpdateNonLocalSyncedDevices(&device_ref_list);
  EXPECT_EQ(0u,
            fake_observer()->num_existing_user_host_switched_events_handled());
}

TEST_F(ObserverNotifierTest, NotifiesObserverForChromebookAddedEvents) {
  PrepareToEnableLocalDeviceAsClient();
  EnableLocalDeviceAsClient();

  // Verify the observer initializes to 0.
  EXPECT_EQ(
      0u, fake_observer()->num_existing_user_chromebook_added_events_handled());

  SetMultiDeviceSetupObserverPtr();
  EXPECT_EQ(
      1u, fake_observer()->num_existing_user_chromebook_added_events_handled());

  // Another sync should not trigger an additional chromebook added event.
  NotifyNewDevicesSynced();
  EXPECT_EQ(
      1u, fake_observer()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(ObserverNotifierTest,
       OldTimestampInPreferencesDoesNotPreventChromebookAddedEvent) {
  PrepareToEnableLocalDeviceAsClient();
  EnableLocalDeviceAsClient();

  int64_t earlier_test_time_millis = kTestTimeMillis / 2;
  SetExistingUserChromebookAddedTimestamp(earlier_test_time_millis);

  SetMultiDeviceSetupObserverPtr();
  EXPECT_EQ(
      1u, fake_observer()->num_existing_user_chromebook_added_events_handled());
  // Timestamp was overwritten by clock.
  EXPECT_EQ(kTestTimeMillis, GetExistingUserChromebookAddedTimestamp());
}

TEST_F(ObserverNotifierTest,
       ChromebookAddedEventRequiresLocalDeviceToEnableClientFeature) {
  PrepareToEnableLocalDeviceAsClient();

  // Triggers event check. Note that if the local device enabled the client
  // feature, this would trigger a Chromebook added event.
  SetMultiDeviceSetupObserverPtr();
  EXPECT_EQ(
      0u, fake_observer()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(ObserverNotifierTest,
       NoChromebookAddedEventIfDeviceWasUsedForUnifiedSetupFlow) {
  PrepareToEnableLocalDeviceAsClient();
  EnableLocalDeviceAsClient();

  base::Time completion_time = base::Time::FromJavaTime(kTestTimeMillis / 2);
  RecordSetupFlowCompletionAtEarlierTime(completion_time);

  // Triggers event check. Note that if the setup flow completion had not been
  // recorded, this would trigger a Chromebook added event.
  SetMultiDeviceSetupObserverPtr();
  EXPECT_EQ(
      0u, fake_observer()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(ObserverNotifierTest, NoChromebookAddedEventWithoutObserverSet) {
  PrepareToEnableLocalDeviceAsClient();
  EnableLocalDeviceAsClient();
  // All conditions for chromebook added event are now satisfied except for
  // setting an observer.

  // No observer is set before the sync.
  NotifyNewDevicesSynced();
  EXPECT_EQ(
      0u, fake_observer()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(ObserverNotifierTest,
       DeviceSyncDoesNotNotifyObserverOfDisconnectedHostsAndClients) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kEnabled),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported),
          cryptauth::RemoteDeviceRefBuilder()
              .SetPublicKey(kDummyKeys[2])
              .SetSoftwareFeatureState(
                  cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
                  cryptauth::SoftwareFeatureState::kEnabled)
              .Build()};
  SetNonLocalSyncedDevices(&device_ref_list);
  EnableLocalDeviceAsClient();
  BuildObserverNotifier();

  SetMultiDeviceSetupObserverPtr();

  NotifyNewDevicesSynced();
  EXPECT_EQ(
      0u, fake_observer()->num_existing_user_chromebook_added_events_handled());
  EXPECT_EQ(0u,
            fake_observer()->num_existing_user_host_switched_events_handled());

  device_ref_list = cryptauth::RemoteDeviceRefList{};
  UpdateNonLocalSyncedDevices(&device_ref_list);
  // No new events caused by removing devices.
  EXPECT_EQ(
      0u, fake_observer()->num_existing_user_chromebook_added_events_handled());
  EXPECT_EQ(0u,
            fake_observer()->num_existing_user_host_switched_events_handled());
}

}  // namespace multidevice_setup

}  // namespace chromeos
