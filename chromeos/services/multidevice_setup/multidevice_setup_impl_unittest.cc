// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier_impl.h"
#include "chromeos/services/multidevice_setup/eligible_host_devices_provider_impl.h"
#include "chromeos/services/multidevice_setup/fake_account_status_change_delegate.h"
#include "chromeos/services/multidevice_setup/fake_account_status_change_delegate_notifier.h"
#include "chromeos/services/multidevice_setup/fake_eligible_host_devices_provider.h"
#include "chromeos/services/multidevice_setup/fake_host_backend_delegate.h"
#include "chromeos/services/multidevice_setup/fake_host_verifier.h"
#include "chromeos/services/multidevice_setup/fake_setup_flow_completion_recorder.h"
#include "chromeos/services/multidevice_setup/host_backend_delegate_impl.h"
#include "chromeos/services/multidevice_setup/host_verifier_impl.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_impl.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/services/multidevice_setup/setup_flow_completion_recorder_impl.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

class FakeEligibleHostDevicesProviderFactory
    : public EligibleHostDevicesProviderImpl::Factory {
 public:
  FakeEligibleHostDevicesProviderFactory(
      device_sync::FakeDeviceSyncClient* expected_device_sync_client)
      : expected_device_sync_client_(expected_device_sync_client) {}

  ~FakeEligibleHostDevicesProviderFactory() override = default;

  FakeEligibleHostDevicesProvider* instance() { return instance_; }

 private:
  // EligibleHostDevicesProviderImpl::Factory:
  std::unique_ptr<EligibleHostDevicesProvider> BuildInstance(
      device_sync::DeviceSyncClient* device_sync_client) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);

    auto instance = std::make_unique<FakeEligibleHostDevicesProvider>();
    instance_ = instance.get();
    return instance;
  }

  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;

  FakeEligibleHostDevicesProvider* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeEligibleHostDevicesProviderFactory);
};

class FakeHostBackendDelegateFactory : public HostBackendDelegateImpl::Factory {
 public:
  FakeHostBackendDelegateFactory(
      FakeEligibleHostDevicesProviderFactory*
          fake_eligible_host_devices_provider_factory,
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service,
      device_sync::FakeDeviceSyncClient* expected_device_sync_client)
      : fake_eligible_host_devices_provider_factory_(
            fake_eligible_host_devices_provider_factory),
        expected_testing_pref_service_(expected_testing_pref_service),
        expected_device_sync_client_(expected_device_sync_client) {}

  ~FakeHostBackendDelegateFactory() override = default;

  FakeHostBackendDelegate* instance() { return instance_; }

 private:
  // HostBackendDelegateImpl::Factory:
  std::unique_ptr<HostBackendDelegate> BuildInstance(
      EligibleHostDevicesProvider* eligible_host_devices_provider,
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      std::unique_ptr<base::OneShotTimer> timer) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(fake_eligible_host_devices_provider_factory_->instance(),
              eligible_host_devices_provider);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);

    auto instance = std::make_unique<FakeHostBackendDelegate>();
    instance_ = instance.get();
    return instance;
  }

  FakeEligibleHostDevicesProviderFactory*
      fake_eligible_host_devices_provider_factory_;
  sync_preferences::TestingPrefServiceSyncable* expected_testing_pref_service_;
  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;

  FakeHostBackendDelegate* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeHostBackendDelegateFactory);
};

class FakeHostVerifierFactory : public HostVerifierImpl::Factory {
 public:
  FakeHostVerifierFactory(
      FakeHostBackendDelegateFactory* fake_host_backend_delegate_factory,
      device_sync::FakeDeviceSyncClient* expected_device_sync_client,
      secure_channel::FakeSecureChannelClient* expected_secure_channel_client)
      : fake_host_backend_delegate_factory_(fake_host_backend_delegate_factory),
        expected_device_sync_client_(expected_device_sync_client),
        expected_secure_channel_client_(expected_secure_channel_client) {}

  ~FakeHostVerifierFactory() override = default;

  FakeHostVerifier* instance() { return instance_; }

 private:
  // HostVerifierImpl::Factory:
  std::unique_ptr<HostVerifier> BuildInstance(
      HostBackendDelegate* host_backend_delegate,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(fake_host_backend_delegate_factory_->instance(),
              host_backend_delegate);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);
    EXPECT_EQ(expected_secure_channel_client_, secure_channel_client);

    auto instance = std::make_unique<FakeHostVerifier>();
    instance_ = instance.get();
    return instance;
  }

  FakeHostBackendDelegateFactory* fake_host_backend_delegate_factory_;
  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;
  secure_channel::FakeSecureChannelClient* expected_secure_channel_client_;

  FakeHostVerifier* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeHostVerifierFactory);
};

class FakeSetupFlowCompletionRecorderFactory
    : public SetupFlowCompletionRecorderImpl::Factory {
 public:
  FakeSetupFlowCompletionRecorderFactory(
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service)
      : expected_testing_pref_service_(expected_testing_pref_service) {}

  ~FakeSetupFlowCompletionRecorderFactory() override = default;

  FakeSetupFlowCompletionRecorder* instance() { return instance_; }

 private:
  // SetupFlowCompletionRecorderImpl::Factory:
  std::unique_ptr<SetupFlowCompletionRecorder> BuildInstance(
      PrefService* pref_service,
      base::Clock* clock) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);

    auto instance = std::make_unique<FakeSetupFlowCompletionRecorder>();
    instance_ = instance.get();
    return instance;
  }

  sync_preferences::TestingPrefServiceSyncable* expected_testing_pref_service_;

  FakeSetupFlowCompletionRecorder* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeSetupFlowCompletionRecorderFactory);
};

class FakeAccountStatusChangeDelegateNotifierFactory
    : public AccountStatusChangeDelegateNotifierImpl::Factory {
 public:
  FakeAccountStatusChangeDelegateNotifierFactory(
      device_sync::FakeDeviceSyncClient* expected_device_sync_client,
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service,
      FakeSetupFlowCompletionRecorderFactory*
          fake_setup_flow_completion_recorder_factory)
      : expected_device_sync_client_(expected_device_sync_client),
        expected_testing_pref_service_(expected_testing_pref_service),
        fake_setup_flow_completion_recorder_factory_(
            fake_setup_flow_completion_recorder_factory) {}

  ~FakeAccountStatusChangeDelegateNotifierFactory() override = default;

  FakeAccountStatusChangeDelegateNotifier* instance() { return instance_; }

 private:
  // AccountStatusChangeDelegateNotifierImpl::Factory:
  std::unique_ptr<AccountStatusChangeDelegateNotifier> BuildInstance(
      device_sync::DeviceSyncClient* device_sync_client,
      PrefService* pref_service,
      SetupFlowCompletionRecorder* setup_flow_completion_recorder,
      base::Clock* clock) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);
    EXPECT_EQ(fake_setup_flow_completion_recorder_factory_->instance(),
              setup_flow_completion_recorder);

    auto instance = std::make_unique<FakeAccountStatusChangeDelegateNotifier>();
    instance_ = instance.get();
    return instance;
  }

  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;
  sync_preferences::TestingPrefServiceSyncable* expected_testing_pref_service_;
  FakeSetupFlowCompletionRecorderFactory*
      fake_setup_flow_completion_recorder_factory_;

  FakeAccountStatusChangeDelegateNotifier* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeAccountStatusChangeDelegateNotifierFactory);
};

}  // namespace

class MultiDeviceSetupImplTest : public testing::Test {
 protected:
  MultiDeviceSetupImplTest() = default;
  ~MultiDeviceSetupImplTest() override = default;

  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();

    fake_eligible_host_devices_provider_factory_ =
        std::make_unique<FakeEligibleHostDevicesProviderFactory>(
            fake_device_sync_client_.get());
    EligibleHostDevicesProviderImpl::Factory::SetFactoryForTesting(
        fake_eligible_host_devices_provider_factory_.get());

    fake_host_backend_delegate_factory_ =
        std::make_unique<FakeHostBackendDelegateFactory>(
            fake_eligible_host_devices_provider_factory_.get(),
            test_pref_service_.get(), fake_device_sync_client_.get());
    HostBackendDelegateImpl::Factory::SetFactoryForTesting(
        fake_host_backend_delegate_factory_.get());

    fake_host_verifier_factory_ = std::make_unique<FakeHostVerifierFactory>(
        fake_host_backend_delegate_factory_.get(),
        fake_device_sync_client_.get(), fake_secure_channel_client_.get());
    HostVerifierImpl::Factory::SetFactoryForTesting(
        fake_host_verifier_factory_.get());

    fake_setup_flow_completion_recorder_factory_ =
        std::make_unique<FakeSetupFlowCompletionRecorderFactory>(
            test_pref_service_.get());
    SetupFlowCompletionRecorderImpl::Factory::SetFactoryForTesting(
        fake_setup_flow_completion_recorder_factory_.get());

    fake_account_status_change_delegate_notifier_factory_ =
        std::make_unique<FakeAccountStatusChangeDelegateNotifierFactory>(
            fake_device_sync_client_.get(), test_pref_service_.get(),
            fake_setup_flow_completion_recorder_factory_.get());
    AccountStatusChangeDelegateNotifierImpl::Factory::SetFactoryForTesting(
        fake_account_status_change_delegate_notifier_factory_.get());

    multidevice_setup_ = MultiDeviceSetupImpl::Factory::Get()->BuildInstance(
        test_pref_service_.get(), fake_device_sync_client_.get(),
        fake_secure_channel_client_.get());
  }

  void TearDown() override {
    EligibleHostDevicesProviderImpl::Factory::SetFactoryForTesting(nullptr);
    HostBackendDelegateImpl::Factory::SetFactoryForTesting(nullptr);
    HostVerifierImpl::Factory::SetFactoryForTesting(nullptr);
    SetupFlowCompletionRecorderImpl::Factory::SetFactoryForTesting(nullptr);
    AccountStatusChangeDelegateNotifierImpl::Factory::SetFactoryForTesting(
        nullptr);
  }

  void CallSetAccountStatusChangeDelegate() {
    EXPECT_FALSE(fake_account_status_change_delegate_);

    fake_account_status_change_delegate_ =
        std::make_unique<FakeAccountStatusChangeDelegate>();

    EXPECT_FALSE(fake_account_status_change_delegate_notifier()->delegate());
    multidevice_setup_->SetAccountStatusChangeDelegate(
        fake_account_status_change_delegate_->GenerateInterfacePtr());
    EXPECT_TRUE(fake_account_status_change_delegate_notifier()->delegate());
  }

  bool CallTriggerEventForDebugging(mojom::EventTypeForDebugging type) {
    EXPECT_FALSE(last_debug_event_success_);

    base::RunLoop run_loop;
    multidevice_setup_->TriggerEventForDebugging(
        type, base::BindOnce(&MultiDeviceSetupImplTest::OnDebugEventTriggered,
                             base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    bool success = *last_debug_event_success_;
    last_debug_event_success_.reset();

    // If the delegate was set, fire off any pending Mojo messages.
    if (success)
      fake_account_status_change_delegate_notifier()->FlushForTesting();

    return success;
  }

  FakeAccountStatusChangeDelegateNotifier*
  fake_account_status_change_delegate_notifier() {
    return fake_account_status_change_delegate_notifier_factory_->instance();
  }

  FakeAccountStatusChangeDelegate* fake_account_status_change_delegate() {
    return fake_account_status_change_delegate_.get();
  }

 private:
  void OnDebugEventTriggered(base::OnceClosure quit_closure, bool success) {
    last_debug_event_success_ = success;
    std::move(quit_closure).Run();
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;

  std::unique_ptr<FakeEligibleHostDevicesProviderFactory>
      fake_eligible_host_devices_provider_factory_;
  std::unique_ptr<FakeHostBackendDelegateFactory>
      fake_host_backend_delegate_factory_;
  std::unique_ptr<FakeHostVerifierFactory> fake_host_verifier_factory_;
  std::unique_ptr<FakeSetupFlowCompletionRecorderFactory>
      fake_setup_flow_completion_recorder_factory_;
  std::unique_ptr<FakeAccountStatusChangeDelegateNotifierFactory>
      fake_account_status_change_delegate_notifier_factory_;

  std::unique_ptr<FakeAccountStatusChangeDelegate>
      fake_account_status_change_delegate_;
  base::Optional<bool> last_debug_event_success_;

  std::unique_ptr<mojom::MultiDeviceSetup> multidevice_setup_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupImplTest);
};

TEST_F(MultiDeviceSetupImplTest, AccountStatusChangeDelegate) {
  // All requests to trigger debug events should fail before the delegate has
  // been set.
  EXPECT_FALSE(CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kNewUserPotentialHostExists));
  EXPECT_FALSE(CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kExistingUserConnectedHostSwitched));
  EXPECT_FALSE(CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kExistingUserNewChromebookAdded));

  CallSetAccountStatusChangeDelegate();

  // All debug trigger events should now succeed.
  EXPECT_TRUE(CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kNewUserPotentialHostExists));
  EXPECT_EQ(
      1u, fake_account_status_change_delegate()->num_new_user_events_handled());

  EXPECT_TRUE(CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kExistingUserConnectedHostSwitched));
  EXPECT_EQ(1u, fake_account_status_change_delegate()
                    ->num_existing_user_host_switched_events_handled());

  EXPECT_TRUE(CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kExistingUserNewChromebookAdded));
  EXPECT_EQ(1u, fake_account_status_change_delegate()
                    ->num_existing_user_chromebook_added_events_handled());
}

}  // namespace multidevice_setup

}  // namespace chromeos
