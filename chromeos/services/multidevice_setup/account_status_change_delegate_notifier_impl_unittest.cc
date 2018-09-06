// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier_impl.h"

#include <string>
#include <vector>

#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_account_status_change_delegate.h"
#include "chromeos/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/services/multidevice_setup/fake_setup_flow_completion_recorder.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const int64_t kTestTimeMillis = 1500000000000;
const std::string kFakePhoneKey = "fake-phone-key";
const std::string kFakePhoneName = "Phony Phone";
const cryptauth::RemoteDeviceRef kFakePhone =
    cryptauth::RemoteDeviceRefBuilder()
        .SetPublicKey(kFakePhoneKey)
        .SetName(kFakePhoneName)
        .Build();

}  // namespace

class MultiDeviceSetupAccountStatusChangeDelegateNotifierTest
    : public testing::Test {
 protected:
  MultiDeviceSetupAccountStatusChangeDelegateNotifierTest() = default;

  ~MultiDeviceSetupAccountStatusChangeDelegateNotifierTest() override = default;

  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    AccountStatusChangeDelegateNotifierImpl::RegisterPrefs(
        test_pref_service_->registry());

    fake_delegate_ = std::make_unique<FakeAccountStatusChangeDelegate>();
    fake_host_status_provider_ = std::make_unique<FakeHostStatusProvider>();
    test_clock_ = std::make_unique<base::SimpleTestClock>();
    fake_setup_flow_completion_recorder_ =
        std::make_unique<FakeSetupFlowCompletionRecorder>();
    test_clock_->SetNow(base::Time::FromJavaTime(kTestTimeMillis));
  }

  void BuildAccountStatusChangeDelegateNotifier() {
    delegate_notifier_ =
        AccountStatusChangeDelegateNotifierImpl::Factory::Get()->BuildInstance(
            fake_host_status_provider_.get(), test_pref_service_.get(),
            fake_setup_flow_completion_recorder_.get(), test_clock_.get());
  }

  // Following HostStatusWithDevice contract, this sets the device to null when
  // there is no set host (or the default kFakePhone otherwise).
  void SetHostWithStatus(
      mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDeviceRef>& host_device) {
    fake_host_status_provider_->SetHostWithStatus(host_status, host_device);
    delegate_notifier_->FlushForTesting();
  }

  void SetNewUserPotentialHostExistsTimestamp(int64_t timestamp) {
    test_pref_service_->SetInt64(AccountStatusChangeDelegateNotifierImpl::
                                     kNewUserPotentialHostExistsPrefName,
                                 timestamp);
  }

  void SetExistingUserChromebookAddedTimestamp(int64_t timestamp) {
    test_pref_service_->SetInt64(AccountStatusChangeDelegateNotifierImpl::
                                     kExistingUserChromebookAddedPrefName,
                                 timestamp);
  }

  void SetHostFromPreviousSession(std::string old_host_key) {
    std::string old_host_device_id =
        cryptauth::RemoteDevice::GenerateDeviceId(old_host_key);
    test_pref_service_->SetString(
        AccountStatusChangeDelegateNotifierImpl::
            kHostDeviceIdFromMostRecentHostStatusUpdatePrefName,
        old_host_device_id);
  }

  void SetAccountStatusChangeDelegatePtr() {
    delegate_notifier_->SetAccountStatusChangeDelegatePtr(
        fake_delegate_->GenerateInterfacePtr());
    delegate_notifier_->FlushForTesting();
  }

  int64_t GetNewUserPotentialHostExistsTimestamp() {
    return test_pref_service_->GetInt64(
        AccountStatusChangeDelegateNotifierImpl::
            kNewUserPotentialHostExistsPrefName);
  }

  int64_t GetExistingUserChromebookAddedTimestamp() {
    return test_pref_service_->GetInt64(
        AccountStatusChangeDelegateNotifierImpl::
            kExistingUserChromebookAddedPrefName);
  }

  FakeAccountStatusChangeDelegate* fake_delegate() {
    return fake_delegate_.get();
  }

 private:
  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<FakeAccountStatusChangeDelegate> fake_delegate_;
  std::unique_ptr<FakeHostStatusProvider> fake_host_status_provider_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<FakeSetupFlowCompletionRecorder>
      fake_setup_flow_completion_recorder_;
  std::unique_ptr<base::SimpleTestClock> test_clock_;

  std::unique_ptr<AccountStatusChangeDelegateNotifier> delegate_notifier_;

  DISALLOW_COPY_AND_ASSIGN(
      MultiDeviceSetupAccountStatusChangeDelegateNotifierTest);
};

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       SetObserverWithPotentialHost) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegatePtr();
  EXPECT_EQ(0u, fake_delegate()->num_new_user_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());

  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    base::nullopt /* host_device */);
  EXPECT_EQ(1u, fake_delegate()->num_new_user_events_handled());
  EXPECT_EQ(kTestTimeMillis, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       PotentialHostAddedLater) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegatePtr();
  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    base::nullopt /* host_device */);

  EXPECT_EQ(0u, fake_delegate()->num_new_user_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());

  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    base::nullopt /* host_device */);
  EXPECT_EQ(1u, fake_delegate()->num_new_user_events_handled());
  EXPECT_EQ(kTestTimeMillis, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       OnlyPotentialHostCausesNewUserEvent) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegatePtr();

  SetHostWithStatus(mojom::HostStatus::kNoEligibleHosts,
                    base::nullopt /* host_device */);
  EXPECT_EQ(0u, fake_delegate()->num_new_user_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());

  SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      kFakePhone);
  EXPECT_EQ(0u, fake_delegate()->num_new_user_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());

  SetHostWithStatus(mojom::HostStatus::kHostSetButNotYetVerified, kFakePhone);
  EXPECT_EQ(0u, fake_delegate()->num_new_user_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());

  SetHostWithStatus(mojom::HostStatus::kHostVerified, kFakePhone);
  EXPECT_EQ(0u, fake_delegate()->num_new_user_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoNewUserEventWithoutObserverSet) {
  BuildAccountStatusChangeDelegateNotifier();
  // All conditions for new user event are satisfied except for setting a
  // delegate.
  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    base::nullopt /* host_device */);

  EXPECT_EQ(0u, fake_delegate()->num_new_user_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       OldTimestampInPreferencesPreventsNewUserFlow) {
  BuildAccountStatusChangeDelegateNotifier();
  int64_t earlier_test_time_millis = kTestTimeMillis / 2;
  SetNewUserPotentialHostExistsTimestamp(earlier_test_time_millis);
  SetAccountStatusChangeDelegatePtr();

  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    base::nullopt /* host_device */);
  EXPECT_EQ(0u, fake_delegate()->num_new_user_events_handled());
  // Timestamp was not overwritten by clock.
  EXPECT_EQ(earlier_test_time_millis, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NotifiesObserverForHostSwitchEvents) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegatePtr();
  // Verify the delegate initializes to 0.
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  // Set initial host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, kFakePhone);
  // Host was set but has never been switched.
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  // Switch hosts.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    cryptauth::RemoteDeviceRefBuilder()
                        .SetPublicKey(kFakePhoneKey + "#1")
                        .SetName(kFakePhoneName + "#1")
                        .Build());
  EXPECT_EQ(1u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  // Switch to another new host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    cryptauth::RemoteDeviceRefBuilder()
                        .SetPublicKey(kFakePhoneKey + "#2")
                        .SetName(kFakePhoneName + "#2")
                        .Build());
  EXPECT_EQ(2u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  // Switch back to initial host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, kFakePhone);
  EXPECT_EQ(3u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       SettingSameHostTriggersNoHostSwitchedEvent) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegatePtr();
  // Set initial host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, kFakePhone);
  // Set to host with identical information.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    cryptauth::RemoteDeviceRefBuilder()
                        .SetPublicKey(kFakePhoneKey)
                        .SetName(kFakePhoneName)
                        .Build());
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       ChangingHostDevicesTriggersHostSwitchEventWhenHostNameIsUnchanged) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegatePtr();
  // Set initial host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, kFakePhone);
  // Set to host with same name but different key.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    cryptauth::RemoteDeviceRefBuilder()
                        .SetPublicKey(kFakePhoneKey + "alternate")
                        .SetName(kFakePhoneName)
                        .Build());
  EXPECT_EQ(1u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       VerifyingHostTriggersNoHostSwtichEvent) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegatePtr();
  // Set initial host but do not verify
  SetHostWithStatus(mojom::HostStatus::kHostSetButNotYetVerified, kFakePhone);
  // Verify host
  SetHostWithStatus(mojom::HostStatus::kHostVerified, kFakePhone);
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       HostSwitchedBetweenSessions) {
  SetHostFromPreviousSession(kFakePhoneKey + "-old");
  BuildAccountStatusChangeDelegateNotifier();
  // Host switched between sessions.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, kFakePhone);
  SetAccountStatusChangeDelegatePtr();
  EXPECT_EQ(1u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoHostSwitchedEventWithoutExistingHost) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegatePtr();
  // No enabled host initially.
  SetHostWithStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                    base::nullopt /* host_device */);
  // Set host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, kFakePhone);
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoHostSwitchedEventWithoutObserverSet) {
  BuildAccountStatusChangeDelegateNotifier();
  // Set initial host.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, kFakePhone);
  // All conditions for host switched event are satisfied except for setting a
  // delegate.
  SetHostWithStatus(mojom::HostStatus::kHostVerified,
                    cryptauth::RemoteDeviceRefBuilder()
                        .SetPublicKey(kFakePhoneKey + "alternate")
                        .SetName(kFakePhoneName + "alternate")
                        .Build());
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NotifiesObserverForChromebookAddedEvents) {
  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegatePtr();
  // Verify the delegate initializes to 0.
  EXPECT_EQ(
      0u, fake_delegate()->num_existing_user_chromebook_added_events_handled());

  // Host is set from another Chromebook while this one is logged in.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, kFakePhone);
  EXPECT_EQ(
      1u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       ChromebookAddedBetweenSessionsTriggersEvents) {
  BuildAccountStatusChangeDelegateNotifier();
  // Host is set before this Chromebook is logged in.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, kFakePhone);

  SetAccountStatusChangeDelegatePtr();
  EXPECT_EQ(
      1u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       OldTimestampInPreferencesDoesNotPreventChromebookAddedEvent) {
  int64_t earlier_test_time_millis = kTestTimeMillis / 2;
  SetExistingUserChromebookAddedTimestamp(earlier_test_time_millis);
  // Check timestamp was written.
  EXPECT_EQ(earlier_test_time_millis,
            GetExistingUserChromebookAddedTimestamp());

  BuildAccountStatusChangeDelegateNotifier();
  SetAccountStatusChangeDelegatePtr();

  SetHostWithStatus(mojom::HostStatus::kHostVerified, kFakePhone);
  EXPECT_EQ(
      1u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
  // Timestamp was overwritten by clock.
  EXPECT_EQ(kTestTimeMillis, GetExistingUserChromebookAddedTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoChromebookAddedEventWithoutObserverSet) {
  BuildAccountStatusChangeDelegateNotifier();
  // Triggers event check. Note that all conditions for Chromebook added event
  // are satisfied except for setting a delegate.
  SetHostWithStatus(mojom::HostStatus::kHostVerified, kFakePhone);
  EXPECT_EQ(
      0u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
}

}  // namespace multidevice_setup

}  // namespace chromeos
