// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier_impl.h"
#include "chromeos/services/multidevice_setup/eligible_host_devices_provider_impl.h"
#include "chromeos/services/multidevice_setup/fake_account_status_change_delegate.h"
#include "chromeos/services/multidevice_setup/fake_account_status_change_delegate_notifier.h"
#include "chromeos/services/multidevice_setup/fake_eligible_host_devices_provider.h"
#include "chromeos/services/multidevice_setup/fake_feature_state_manager.h"
#include "chromeos/services/multidevice_setup/fake_feature_state_observer.h"
#include "chromeos/services/multidevice_setup/fake_host_backend_delegate.h"
#include "chromeos/services/multidevice_setup/fake_host_status_observer.h"
#include "chromeos/services/multidevice_setup/fake_host_status_provider.h"
#include "chromeos/services/multidevice_setup/fake_host_verifier.h"
#include "chromeos/services/multidevice_setup/fake_setup_flow_completion_recorder.h"
#include "chromeos/services/multidevice_setup/feature_state_manager_impl.h"
#include "chromeos/services/multidevice_setup/host_backend_delegate_impl.h"
#include "chromeos/services/multidevice_setup/host_status_provider_impl.h"
#include "chromeos/services/multidevice_setup/host_verifier_impl.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_impl.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/services/multidevice_setup/setup_flow_completion_recorder_impl.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const size_t kNumTestDevices = 3;

cryptauth::RemoteDeviceList RefListToRawList(
    const cryptauth::RemoteDeviceRefList& ref_list) {
  cryptauth::RemoteDeviceList raw_list;
  std::transform(ref_list.begin(), ref_list.end(), std::back_inserter(raw_list),
                 [](const cryptauth::RemoteDeviceRef ref) {
                   return *GetMutableRemoteDevice(ref);
                 });
  return raw_list;
}

base::Optional<cryptauth::RemoteDevice> RefToRaw(
    const base::Optional<cryptauth::RemoteDeviceRef>& ref) {
  if (!ref)
    return base::nullopt;

  return *GetMutableRemoteDevice(*ref);
}

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
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service)
      : fake_host_backend_delegate_factory_(fake_host_backend_delegate_factory),
        expected_device_sync_client_(expected_device_sync_client),
        expected_testing_pref_service_(expected_testing_pref_service) {}

  ~FakeHostVerifierFactory() override = default;

  FakeHostVerifier* instance() { return instance_; }

 private:
  // HostVerifierImpl::Factory:
  std::unique_ptr<HostVerifier> BuildInstance(
      HostBackendDelegate* host_backend_delegate,
      device_sync::DeviceSyncClient* device_sync_client,
      PrefService* pref_service,
      base::Clock* clock,
      std::unique_ptr<base::OneShotTimer> timer) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(fake_host_backend_delegate_factory_->instance(),
              host_backend_delegate);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);

    auto instance = std::make_unique<FakeHostVerifier>();
    instance_ = instance.get();
    return instance;
  }

  FakeHostBackendDelegateFactory* fake_host_backend_delegate_factory_;
  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;
  sync_preferences::TestingPrefServiceSyncable* expected_testing_pref_service_;

  FakeHostVerifier* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeHostVerifierFactory);
};

class FakeHostStatusProviderFactory : public HostStatusProviderImpl::Factory {
 public:
  FakeHostStatusProviderFactory(
      FakeEligibleHostDevicesProviderFactory*
          fake_eligible_host_devices_provider_factory,
      FakeHostBackendDelegateFactory* fake_host_backend_delegate_factory,
      FakeHostVerifierFactory* fake_host_verifier_factory,
      device_sync::FakeDeviceSyncClient* expected_device_sync_client)
      : fake_eligible_host_devices_provider_factory_(
            fake_eligible_host_devices_provider_factory),
        fake_host_backend_delegate_factory_(fake_host_backend_delegate_factory),
        fake_host_verifier_factory_(fake_host_verifier_factory),
        expected_device_sync_client_(expected_device_sync_client) {}

  ~FakeHostStatusProviderFactory() override = default;

  FakeHostStatusProvider* instance() { return instance_; }

 private:
  // HostStatusProviderImpl::Factory:
  std::unique_ptr<HostStatusProvider> BuildInstance(
      EligibleHostDevicesProvider* eligible_host_devices_provider,
      HostBackendDelegate* host_backend_delegate,
      HostVerifier* host_verifier,
      device_sync::DeviceSyncClient* device_sync_client) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(fake_eligible_host_devices_provider_factory_->instance(),
              eligible_host_devices_provider);
    EXPECT_EQ(fake_host_backend_delegate_factory_->instance(),
              host_backend_delegate);
    EXPECT_EQ(fake_host_verifier_factory_->instance(), host_verifier);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);

    auto instance = std::make_unique<FakeHostStatusProvider>();
    instance_ = instance.get();
    return instance;
  }

  FakeEligibleHostDevicesProviderFactory*
      fake_eligible_host_devices_provider_factory_;
  FakeHostBackendDelegateFactory* fake_host_backend_delegate_factory_;
  FakeHostVerifierFactory* fake_host_verifier_factory_;
  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;

  FakeHostStatusProvider* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeHostStatusProviderFactory);
};

class FakeFeatureStateManagerFactory : public FeatureStateManagerImpl::Factory {
 public:
  FakeFeatureStateManagerFactory(
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service,
      FakeHostStatusProviderFactory* fake_host_status_provider_factory,
      device_sync::FakeDeviceSyncClient* expected_device_sync_client)
      : expected_testing_pref_service_(expected_testing_pref_service),
        fake_host_status_provider_factory_(fake_host_status_provider_factory),
        expected_device_sync_client_(expected_device_sync_client) {}

  ~FakeFeatureStateManagerFactory() override = default;

  FakeFeatureStateManager* instance() { return instance_; }

 private:
  // FeatureStateManagerImpl::Factory:
  std::unique_ptr<FeatureStateManager> BuildInstance(
      PrefService* pref_service,
      HostStatusProvider* host_status_provider,
      device_sync::DeviceSyncClient* device_sync_client) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);
    EXPECT_EQ(fake_host_status_provider_factory_->instance(),
              host_status_provider);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);

    auto instance = std::make_unique<FakeFeatureStateManager>();
    instance_ = instance.get();
    return instance;
  }

  sync_preferences::TestingPrefServiceSyncable* expected_testing_pref_service_;
  FakeHostStatusProviderFactory* fake_host_status_provider_factory_;
  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;

  FakeFeatureStateManager* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeFeatureStateManagerFactory);
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
  MultiDeviceSetupImplTest()
      : test_devices_(
            cryptauth::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
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
        fake_device_sync_client_.get(), test_pref_service_.get());
    HostVerifierImpl::Factory::SetFactoryForTesting(
        fake_host_verifier_factory_.get());

    fake_host_status_provider_factory_ =
        std::make_unique<FakeHostStatusProviderFactory>(
            fake_eligible_host_devices_provider_factory_.get(),
            fake_host_backend_delegate_factory_.get(),
            fake_host_verifier_factory_.get(), fake_device_sync_client_.get());
    HostStatusProviderImpl::Factory::SetFactoryForTesting(
        fake_host_status_provider_factory_.get());

    fake_feature_state_manager_factory_ =
        std::make_unique<FakeFeatureStateManagerFactory>(
            test_pref_service_.get(), fake_host_status_provider_factory_.get(),
            fake_device_sync_client_.get());
    FeatureStateManagerImpl::Factory::SetFactoryForTesting(
        fake_feature_state_manager_factory_.get());

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
    HostStatusProviderImpl::Factory::SetFactoryForTesting(nullptr);
    FeatureStateManagerImpl::Factory::SetFactoryForTesting(nullptr);
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

  cryptauth::RemoteDeviceList CallGetEligibleHostDevices() {
    base::RunLoop run_loop;
    multidevice_setup_->GetEligibleHostDevices(
        base::BindOnce(&MultiDeviceSetupImplTest::OnEligibleDevicesFetched,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    cryptauth::RemoteDeviceList eligible_devices_list =
        *last_eligible_devices_list_;
    last_eligible_devices_list_.reset();

    return eligible_devices_list;
  }

  bool CallSetHostDevice(const std::string& host_device_id) {
    base::RunLoop run_loop;
    multidevice_setup_->SetHostDevice(
        host_device_id,
        base::BindOnce(&MultiDeviceSetupImplTest::OnHostSet,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    bool success = *last_set_host_success_;
    last_set_host_success_.reset();

    return success;
  }

  std::pair<mojom::HostStatus, base::Optional<cryptauth::RemoteDevice>>
  CallGetHostStatus() {
    base::RunLoop run_loop;
    multidevice_setup_->GetHostStatus(
        base::BindOnce(&MultiDeviceSetupImplTest::OnHostStatusReceived,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    std::pair<mojom::HostStatus, base::Optional<cryptauth::RemoteDevice>>
        host_status_update = *last_host_status_;
    last_host_status_.reset();

    return host_status_update;
  }

  bool CallSetFeatureEnabledState(mojom::Feature feature, bool enabled) {
    base::RunLoop run_loop;
    multidevice_setup_->SetFeatureEnabledState(
        feature, enabled,
        base::BindOnce(&MultiDeviceSetupImplTest::OnSetFeatureEnabled,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    bool success = *last_set_host_success_;
    last_set_host_success_.reset();

    return success;
  }

  base::flat_map<mojom::Feature, mojom::FeatureState> CallGetFeatureStates() {
    base::RunLoop run_loop;
    multidevice_setup_->GetFeatureStates(
        base::BindOnce(&MultiDeviceSetupImplTest::OnGetFeatureStates,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    base::flat_map<mojom::Feature, mojom::FeatureState> feature_states_map =
        *last_get_feature_states_result_;
    last_get_feature_states_result_.reset();

    return feature_states_map;
  }

  bool CallRetrySetHostNow() {
    base::RunLoop run_loop;
    multidevice_setup_->RetrySetHostNow(
        base::BindOnce(&MultiDeviceSetupImplTest::OnHostRetried,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    bool success = *last_retry_success_;
    last_retry_success_.reset();

    return success;
  }

  bool CallTriggerEventForDebugging(mojom::EventTypeForDebugging type) {
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

  void VerifyCurrentHostStatus(
      mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDeviceRef>& host_device,
      FakeHostStatusObserver* observer = nullptr,
      size_t expected_observer_index = 0u) {
    std::pair<mojom::HostStatus, base::Optional<cryptauth::RemoteDevice>>
        host_status_and_device = CallGetHostStatus();
    EXPECT_EQ(host_status, host_status_and_device.first);
    EXPECT_EQ(RefToRaw(host_device), host_status_and_device.second);

    if (!observer)
      return;

    EXPECT_EQ(host_status,
              observer->host_status_updates()[expected_observer_index].first);
    EXPECT_EQ(RefToRaw(host_device),
              observer->host_status_updates()[expected_observer_index].second);
  }

  void SendPendingObserverMessages() {
    MultiDeviceSetupImpl* derived_ptr =
        static_cast<MultiDeviceSetupImpl*>(multidevice_setup_.get());
    derived_ptr->FlushForTesting();
  }

  FakeAccountStatusChangeDelegate* fake_account_status_change_delegate() {
    return fake_account_status_change_delegate_.get();
  }

  FakeEligibleHostDevicesProvider* fake_eligible_host_devices_provider() {
    return fake_eligible_host_devices_provider_factory_->instance();
  }

  FakeHostBackendDelegate* fake_host_backend_delegate() {
    return fake_host_backend_delegate_factory_->instance();
  }

  FakeHostVerifier* fake_host_verifier() {
    return fake_host_verifier_factory_->instance();
  }

  FakeHostStatusProvider* fake_host_status_provider() {
    return fake_host_status_provider_factory_->instance();
  }

  FakeFeatureStateManager* fake_feature_state_manager() {
    return fake_feature_state_manager_factory_->instance();
  }

  FakeSetupFlowCompletionRecorder* fake_setup_flow_completion_recorder() {
    return fake_setup_flow_completion_recorder_factory_->instance();
  }

  FakeAccountStatusChangeDelegateNotifier*
  fake_account_status_change_delegate_notifier() {
    return fake_account_status_change_delegate_notifier_factory_->instance();
  }

  cryptauth::RemoteDeviceRefList& test_devices() { return test_devices_; }

  mojom::MultiDeviceSetup* multidevice_setup() {
    return multidevice_setup_.get();
  }

 private:
  void OnEligibleDevicesFetched(
      base::OnceClosure quit_closure,
      const cryptauth::RemoteDeviceList& eligible_devices_list) {
    EXPECT_FALSE(last_eligible_devices_list_);
    last_eligible_devices_list_ = eligible_devices_list;
    std::move(quit_closure).Run();
  }

  void OnHostSet(base::OnceClosure quit_closure, bool success) {
    EXPECT_FALSE(last_set_host_success_);
    last_set_host_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnHostStatusReceived(
      base::OnceClosure quit_closure,
      mojom::HostStatus host_status,
      const base::Optional<cryptauth::RemoteDevice>& host_device) {
    EXPECT_FALSE(last_host_status_);
    last_host_status_ = std::make_pair(host_status, host_device);
    std::move(quit_closure).Run();
  }

  void OnSetFeatureEnabled(base::OnceClosure quit_closure, bool success) {
    EXPECT_FALSE(last_set_host_success_);
    last_set_host_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnGetFeatureStates(
      base::OnceClosure quit_closure,
      const base::flat_map<mojom::Feature, mojom::FeatureState>&
          feature_states_map) {
    EXPECT_FALSE(last_get_feature_states_result_);
    last_get_feature_states_result_ = feature_states_map;
    std::move(quit_closure).Run();
  }

  void OnHostRetried(base::OnceClosure quit_closure, bool success) {
    EXPECT_FALSE(last_retry_success_);
    last_retry_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnDebugEventTriggered(base::OnceClosure quit_closure, bool success) {
    EXPECT_FALSE(last_debug_event_success_);
    last_debug_event_success_ = success;
    std::move(quit_closure).Run();
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  cryptauth::RemoteDeviceRefList test_devices_;

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
  std::unique_ptr<FakeHostStatusProviderFactory>
      fake_host_status_provider_factory_;
  std::unique_ptr<FakeFeatureStateManagerFactory>
      fake_feature_state_manager_factory_;
  std::unique_ptr<FakeSetupFlowCompletionRecorderFactory>
      fake_setup_flow_completion_recorder_factory_;
  std::unique_ptr<FakeAccountStatusChangeDelegateNotifierFactory>
      fake_account_status_change_delegate_notifier_factory_;

  std::unique_ptr<FakeAccountStatusChangeDelegate>
      fake_account_status_change_delegate_;

  base::Optional<bool> last_debug_event_success_;
  base::Optional<cryptauth::RemoteDeviceList> last_eligible_devices_list_;
  base::Optional<bool> last_set_host_success_;
  base::Optional<
      std::pair<mojom::HostStatus, base::Optional<cryptauth::RemoteDevice>>>
      last_host_status_;
  base::Optional<bool> last_set_feature_enabled_state_success_;
  base::Optional<base::flat_map<mojom::Feature, mojom::FeatureState>>
      last_get_feature_states_result_;
  base::Optional<bool> last_retry_success_;

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

TEST_F(MultiDeviceSetupImplTest, FeatureStateChanges) {
  auto observer = std::make_unique<FakeFeatureStateObserver>();
  multidevice_setup()->AddFeatureStateObserver(
      observer->GenerateInterfacePtr());

  EXPECT_EQ(mojom::FeatureState::kUnavailableNoVerifiedHost,
            CallGetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);

  fake_feature_state_manager()->SetFeatureState(
      mojom::Feature::kBetterTogetherSuite,
      mojom::FeatureState::kNotSupportedByChromebook);
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kNotSupportedByChromebook,
            CallGetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  EXPECT_EQ(1u, observer->feature_state_updates().size());

  fake_feature_state_manager()->SetFeatureState(
      mojom::Feature::kBetterTogetherSuite,
      mojom::FeatureState::kEnabledByUser);
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kEnabledByUser,
            CallGetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  EXPECT_EQ(2u, observer->feature_state_updates().size());

  EXPECT_TRUE(CallSetFeatureEnabledState(mojom::Feature::kBetterTogetherSuite,
                                         false /* enabled */));
  SendPendingObserverMessages();
  EXPECT_EQ(mojom::FeatureState::kDisabledByUser,
            CallGetFeatureStates()[mojom::Feature::kBetterTogetherSuite]);
  EXPECT_EQ(3u, observer->feature_state_updates().size());
}

TEST_F(MultiDeviceSetupImplTest, ComprehensiveHostTest) {
  // Start with no eligible devices.
  EXPECT_TRUE(CallGetEligibleHostDevices().empty());
  VerifyCurrentHostStatus(mojom::HostStatus::kNoEligibleHosts,
                          base::nullopt /* host_device */);

  // Cannot retry without a host.
  EXPECT_FALSE(CallRetrySetHostNow());

  // Add a status observer.
  auto observer = std::make_unique<FakeHostStatusObserver>();
  multidevice_setup()->AddHostStatusObserver(observer->GenerateInterfacePtr());

  // Simulate a sync occurring; now, all of the test devices are eligible hosts.
  fake_eligible_host_devices_provider()->set_eligible_host_devices(
      test_devices());
  EXPECT_EQ(RefListToRawList(test_devices()), CallGetEligibleHostDevices());
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kEligibleHostExistsButNoHostSet,
      base::nullopt /* host_device */);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                          base::nullopt /* host_device */, observer.get(),
                          0u /* expected_observer_index */);

  // There are eligible hosts, but none is set; thus, cannot retry.
  EXPECT_FALSE(CallRetrySetHostNow());

  // Set an invalid host as the host device; this should fail.
  EXPECT_FALSE(CallSetHostDevice("invalidHostDeviceId"));
  EXPECT_FALSE(fake_host_backend_delegate()->HasPendingHostRequest());

  // Set device 0 as the host; this should succeed.
  EXPECT_TRUE(CallSetHostDevice(test_devices()[0].GetDeviceId()));
  EXPECT_TRUE(fake_host_backend_delegate()->HasPendingHostRequest());
  EXPECT_EQ(test_devices()[0],
            fake_host_backend_delegate()->GetPendingHostRequest());
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      test_devices()[0]);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(
      mojom::HostStatus::kHostSetLocallyButWaitingForBackendConfirmation,
      test_devices()[0], observer.get(), 1u /* expected_observer_index */);

  // It should now be possible to retry.
  EXPECT_TRUE(CallRetrySetHostNow());

  // Simulate the retry succeeding and the host being set on the back-end.
  fake_host_backend_delegate()->NotifyHostChangedOnBackend(test_devices()[0]);
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostSetButNotYetVerified, test_devices()[0]);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(mojom::HostStatus::kHostSetButNotYetVerified,
                          test_devices()[0], observer.get(),
                          2u /* expected_observer_index */);

  // It should still be possible to retry (this time, retrying verification).
  EXPECT_TRUE(CallRetrySetHostNow());

  // Simulate verification succeeding.
  fake_host_verifier()->set_is_host_verified(true);
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kHostVerified, test_devices()[0]);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(mojom::HostStatus::kHostVerified, test_devices()[0],
                          observer.get(), 3u /* expected_observer_index */);

  // Remove the host.
  multidevice_setup()->RemoveHostDevice();
  fake_host_verifier()->set_is_host_verified(false);
  fake_host_status_provider()->SetHostWithStatus(
      mojom::HostStatus::kEligibleHostExistsButNoHostSet,
      base::nullopt /* host_device */);
  SendPendingObserverMessages();
  VerifyCurrentHostStatus(mojom::HostStatus::kEligibleHostExistsButNoHostSet,
                          base::nullopt /* host_device */, observer.get(),
                          4u /* expected_observer_index */);

  // Simulate the host being removed on the back-end.
  fake_host_backend_delegate()->NotifyHostChangedOnBackend(base::nullopt);
}

}  // namespace multidevice_setup

}  // namespace chromeos
