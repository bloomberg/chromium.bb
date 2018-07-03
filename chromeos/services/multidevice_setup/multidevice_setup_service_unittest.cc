// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_account_status_change_delegate.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_impl.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

class FakeMultiDeviceSetupFactory : public MultiDeviceSetupImpl::Factory {
 public:
  FakeMultiDeviceSetupFactory(
      sync_preferences::TestingPrefServiceSyncable*
          expected_testing_pref_service,
      device_sync::FakeDeviceSyncClient* expected_device_sync_client,
      secure_channel::FakeSecureChannelClient* expected_secure_channel_client)
      : expected_testing_pref_service_(expected_testing_pref_service),
        expected_device_sync_client_(expected_device_sync_client),
        expected_secure_channel_client_(expected_secure_channel_client) {}

  ~FakeMultiDeviceSetupFactory() override = default;

  FakeMultiDeviceSetup* instance() { return instance_; }

 private:
  std::unique_ptr<mojom::MultiDeviceSetup> BuildInstance(
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client) override {
    EXPECT_FALSE(instance_);
    EXPECT_EQ(expected_testing_pref_service_, pref_service);
    EXPECT_EQ(expected_device_sync_client_, device_sync_client);
    EXPECT_EQ(expected_secure_channel_client_, secure_channel_client);

    auto instance = std::make_unique<FakeMultiDeviceSetup>();
    instance_ = instance.get();
    return instance;
  }

  sync_preferences::TestingPrefServiceSyncable* expected_testing_pref_service_;
  device_sync::FakeDeviceSyncClient* expected_device_sync_client_;
  secure_channel::FakeSecureChannelClient* expected_secure_channel_client_;

  FakeMultiDeviceSetup* instance_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FakeMultiDeviceSetupFactory);
};

}  // namespace

class MultiDeviceSetupServiceTest : public testing::Test {
 protected:
  MultiDeviceSetupServiceTest()
      : test_device_(cryptauth::CreateRemoteDeviceRefForTest()) {}
  ~MultiDeviceSetupServiceTest() override = default;

  // testing::Test:
  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();

    fake_multidevice_setup_factory_ =
        std::make_unique<FakeMultiDeviceSetupFactory>(
            test_pref_service_.get(), fake_device_sync_client_.get(),
            fake_secure_channel_client_.get());
    MultiDeviceSetupImpl::Factory::SetFactoryForTesting(
        fake_multidevice_setup_factory_.get());

    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            std::make_unique<MultiDeviceSetupService>(
                test_pref_service_.get(), fake_device_sync_client_.get(),
                fake_secure_channel_client_.get()));

    auto connector = connector_factory_->CreateConnector();
    connector->BindInterface(mojom::kServiceName, &multidevice_setup_ptr_);
    multidevice_setup_ptr_.FlushForTesting();
  }

  void TearDown() override {
    MultiDeviceSetupImpl::Factory::SetFactoryForTesting(nullptr);
  }

  void CallSetAccountStatusChangeDelegate() {
    EXPECT_FALSE(fake_account_status_change_delegate_);

    fake_account_status_change_delegate_ =
        std::make_unique<FakeAccountStatusChangeDelegate>();

    multidevice_setup_ptr_->SetAccountStatusChangeDelegate(
        fake_account_status_change_delegate_->GenerateInterfacePtr());
    multidevice_setup_ptr_.FlushForTesting();
  }

  void CallTriggerEventForDebuggingBeforeInitializationComplete(
      mojom::EventTypeForDebugging type) {
    EXPECT_FALSE(fake_account_status_change_delegate_);
    EXPECT_FALSE(last_debug_event_success_);

    base::RunLoop run_loop;
    multidevice_setup_ptr_->TriggerEventForDebugging(
        type,
        base::BindOnce(&MultiDeviceSetupServiceTest::OnDebugEventTriggered,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    // Always expected to fail before initialization completes.
    EXPECT_FALSE(*last_debug_event_success_);
    last_debug_event_success_.reset();
  }

  void CallTriggerEventForDebuggingAfterInitializationComplete(
      mojom::EventTypeForDebugging type,
      bool should_succeed) {
    EXPECT_TRUE(fake_account_status_change_delegate_);
    EXPECT_FALSE(last_debug_event_success_);

    base::RunLoop run_loop;
    multidevice_setup_ptr_->TriggerEventForDebugging(
        type,
        base::BindOnce(&MultiDeviceSetupServiceTest::OnDebugEventTriggered,
                       base::Unretained(this), run_loop.QuitClosure()));
    multidevice_setup_ptr_.FlushForTesting();

    std::pair<mojom::EventTypeForDebugging,
              FakeMultiDeviceSetup::TriggerEventForDebuggingCallback>&
        triggered_debug_event_args =
            fake_multidevice_setup()->triggered_debug_events().back();
    EXPECT_EQ(type, triggered_debug_event_args.first);
    std::move(triggered_debug_event_args.second).Run(should_succeed);

    run_loop.Run();

    EXPECT_EQ(should_succeed, *last_debug_event_success_);
    last_debug_event_success_.reset();
  }

  void FinishInitialization() {
    EXPECT_FALSE(fake_multidevice_setup());
    fake_device_sync_client_->set_local_device_metadata(test_device_);
    fake_device_sync_client_->NotifyReady();
    EXPECT_TRUE(fake_multidevice_setup());
  }

  FakeAccountStatusChangeDelegate* fake_account_status_change_delegate() {
    return fake_account_status_change_delegate_.get();
  }

  FakeMultiDeviceSetup* fake_multidevice_setup() {
    return fake_multidevice_setup_factory_->instance();
  }

  mojom::MultiDeviceSetup* multidevice_setup() {
    return multidevice_setup_ptr_.get();
  }

 private:
  void OnDebugEventTriggered(base::OnceClosure quit_closure, bool success) {
    last_debug_event_success_ = success;
    std::move(quit_closure).Run();
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;
  const cryptauth::RemoteDeviceRef test_device_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;

  std::unique_ptr<FakeMultiDeviceSetupFactory> fake_multidevice_setup_factory_;

  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<FakeAccountStatusChangeDelegate>
      fake_account_status_change_delegate_;
  base::Optional<bool> last_debug_event_success_;

  mojom::MultiDeviceSetupPtr multidevice_setup_ptr_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupServiceTest);
};

TEST_F(MultiDeviceSetupServiceTest,
       TriggerEventForDebuggingBeforeInitialization) {
  CallTriggerEventForDebuggingBeforeInitializationComplete(
      mojom::EventTypeForDebugging::kNewUserPotentialHostExists);
  CallTriggerEventForDebuggingBeforeInitializationComplete(
      mojom::EventTypeForDebugging::kExistingUserConnectedHostSwitched);
  CallTriggerEventForDebuggingBeforeInitializationComplete(
      mojom::EventTypeForDebugging::kExistingUserNewChromebookAdded);
}

TEST_F(MultiDeviceSetupServiceTest, SetDelegateBeforeInitialization) {
  CallSetAccountStatusChangeDelegate();
  FinishInitialization();
  CallTriggerEventForDebuggingAfterInitializationComplete(
      mojom::EventTypeForDebugging::kNewUserPotentialHostExists,
      true /* should_succeed */);
  CallTriggerEventForDebuggingAfterInitializationComplete(
      mojom::EventTypeForDebugging::kExistingUserConnectedHostSwitched,
      true /* should_succeed */);
  CallTriggerEventForDebuggingAfterInitializationComplete(
      mojom::EventTypeForDebugging::kExistingUserNewChromebookAdded,
      true /* should_succeed */);
}

TEST_F(MultiDeviceSetupServiceTest, FinishInitializationFirst) {
  FinishInitialization();
  CallSetAccountStatusChangeDelegate();
  CallTriggerEventForDebuggingAfterInitializationComplete(
      mojom::EventTypeForDebugging::kNewUserPotentialHostExists,
      true /* should_succeed */);
  CallTriggerEventForDebuggingAfterInitializationComplete(
      mojom::EventTypeForDebugging::kExistingUserConnectedHostSwitched,
      true /* should_succeed */);
  CallTriggerEventForDebuggingAfterInitializationComplete(
      mojom::EventTypeForDebugging::kExistingUserNewChromebookAdded,
      true /* should_succeed */);
}

}  // namespace multidevice_setup

}  // namespace chromeos
