// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/unlock_manager.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/cryptauth/cryptauth_test_util.h"
#include "components/cryptauth/fake_secure_context.h"
#include "components/cryptauth/secure_context.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/messenger.h"
#include "components/proximity_auth/mock_proximity_auth_client.h"
#include "components/proximity_auth/proximity_monitor.h"
#include "components/proximity_auth/remote_device_life_cycle.h"
#include "components/proximity_auth/remote_status_update.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#endif  // defined(OS_CHROMEOS)

using testing::AtLeast;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::_;

namespace proximity_auth {
namespace {

// The sign-in challenge to send to the remote device.
const char kChallenge[] = "sign-in challenge";
const char kSignInSecret[] = "decrypted challenge";

// Note that the trust agent state is currently ignored by the UnlockManager
// implementation.
RemoteStatusUpdate kRemoteScreenUnlocked = {
    USER_PRESENT, SECURE_SCREEN_LOCK_ENABLED, TRUST_AGENT_UNSUPPORTED};
RemoteStatusUpdate kRemoteScreenLocked = {
    USER_ABSENT, SECURE_SCREEN_LOCK_ENABLED, TRUST_AGENT_UNSUPPORTED};
RemoteStatusUpdate kRemoteScreenlockDisabled = {
    USER_PRESENT, SECURE_SCREEN_LOCK_DISABLED, TRUST_AGENT_UNSUPPORTED};
RemoteStatusUpdate kRemoteScreenlockStateUnknown = {
    USER_PRESENCE_UNKNOWN, SECURE_SCREEN_LOCK_STATE_UNKNOWN,
    TRUST_AGENT_UNSUPPORTED};

class MockRemoteDeviceLifeCycle : public RemoteDeviceLifeCycle {
 public:
  MockRemoteDeviceLifeCycle() {}
  ~MockRemoteDeviceLifeCycle() override {}

  MOCK_METHOD0(Start, void());
  MOCK_CONST_METHOD0(GetRemoteDevice, cryptauth::RemoteDevice());
  MOCK_CONST_METHOD0(GetState, State());
  MOCK_METHOD0(GetMessenger, Messenger*());
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
};

class MockMessenger : public Messenger {
 public:
  MockMessenger() {}
  ~MockMessenger() override {}

  MOCK_METHOD1(AddObserver, void(MessengerObserver* observer));
  MOCK_METHOD1(RemoveObserver, void(MessengerObserver* observer));
  MOCK_CONST_METHOD0(SupportsSignIn, bool());
  MOCK_METHOD0(DispatchUnlockEvent, void());
  MOCK_METHOD1(RequestDecryption, void(const std::string& challenge));
  MOCK_METHOD0(RequestUnlock, void());
  MOCK_CONST_METHOD0(GetSecureContext, cryptauth::SecureContext*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMessenger);
};

class MockProximityMonitor : public ProximityMonitor {
 public:
  MockProximityMonitor() {
    ON_CALL(*this, GetStrategy())
        .WillByDefault(Return(ProximityMonitor::Strategy::NONE));
    ON_CALL(*this, IsUnlockAllowed()).WillByDefault(Return(true));
    ON_CALL(*this, IsInRssiRange()).WillByDefault(Return(false));
  }
  ~MockProximityMonitor() override {}

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(GetStrategy, Strategy());
  MOCK_CONST_METHOD0(IsUnlockAllowed, bool());
  MOCK_CONST_METHOD0(IsInRssiRange, bool());
  MOCK_METHOD0(RecordProximityMetricsOnAuthSuccess, void());
  MOCK_METHOD1(AddObserver, void(ProximityMonitorObserver*));
  MOCK_METHOD1(RemoveObserver, void(ProximityMonitorObserver*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProximityMonitor);
};

class FakeLockHandler : public ScreenlockBridge::LockHandler {
 public:
  FakeLockHandler() {}
  ~FakeLockHandler() override {}

  // LockHandler:
  void ShowBannerMessage(const base::string16& message) override {}
  void ShowUserPodCustomIcon(
      const AccountId& account_id,
      const ScreenlockBridge::UserPodCustomIconOptions& icon) override {}
  void HideUserPodCustomIcon(const AccountId& account_id) override {}
  void EnableInput() override {}
  void SetAuthType(const AccountId& account_id,
                   AuthType auth_type,
                   const base::string16& auth_value) override {}
  AuthType GetAuthType(const AccountId& account_id) const override {
    return USER_CLICK;
  }
  ScreenType GetScreenType() const override { return LOCK_SCREEN; }
  void Unlock(const AccountId& account_id) override {}
  void AttemptEasySignin(const AccountId& account_id,
                         const std::string& secret,
                         const std::string& key_label) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeLockHandler);
};

class TestUnlockManager : public UnlockManager {
 public:
  TestUnlockManager(ProximityAuthSystem::ScreenlockType screenlock_type,
                    ProximityAuthClient* proximity_auth_client)
      : UnlockManager(screenlock_type, proximity_auth_client),
        proximity_monitor_(nullptr) {}
  ~TestUnlockManager() override {}

  using UnlockManager::OnAuthAttempted;
  using MessengerObserver::OnUnlockEventSent;
  using MessengerObserver::OnRemoteStatusUpdate;
  using MessengerObserver::OnDecryptResponse;
  using MessengerObserver::OnUnlockResponse;
  using MessengerObserver::OnDisconnected;
  using ScreenlockBridge::Observer::OnScreenDidLock;
  using ScreenlockBridge::Observer::OnScreenDidUnlock;
  using ScreenlockBridge::Observer::OnFocusedUserChanged;

  MockProximityMonitor* proximity_monitor() { return proximity_monitor_; }

 private:
  std::unique_ptr<ProximityMonitor> CreateProximityMonitor(
      const cryptauth::RemoteDevice& remote_device) override {
    EXPECT_EQ(cryptauth::kTestRemoteDevicePublicKey, remote_device.public_key);
    std::unique_ptr<MockProximityMonitor> proximity_monitor(
        new NiceMock<MockProximityMonitor>());
    proximity_monitor_ = proximity_monitor.get();
    return std::move(proximity_monitor);
  }

  // Owned by the super class.
  MockProximityMonitor* proximity_monitor_;

  DISALLOW_COPY_AND_ASSIGN(TestUnlockManager);
};

// Creates a mock Bluetooth adapter and sets it as the global adapter for
// testing.
scoped_refptr<device::MockBluetoothAdapter>
CreateAndRegisterMockBluetoothAdapter() {
  scoped_refptr<device::MockBluetoothAdapter> adapter =
      new NiceMock<device::MockBluetoothAdapter>();
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);
  return adapter;
}

}  // namespace

class ProximityAuthUnlockManagerTest : public testing::Test {
 public:
  ProximityAuthUnlockManagerTest()
      : remote_device_(cryptauth::CreateClassicRemoteDeviceForTest()),
        bluetooth_adapter_(CreateAndRegisterMockBluetoothAdapter()),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_) {
    ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(true));
    ON_CALL(life_cycle_, GetMessenger()).WillByDefault(Return(&messenger_));
    ON_CALL(life_cycle_, GetRemoteDevice())
        .WillByDefault(Return(remote_device_));
    ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
    ON_CALL(messenger_, GetSecureContext())
        .WillByDefault(Return(&secure_context_));

    ScreenlockBridge::Get()->SetLockHandler(&lock_handler_);

#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Initialize();
#endif
  }

  ~ProximityAuthUnlockManagerTest() override {
    // Make sure to verify the mock prior to the destruction of the unlock
    // manager, as otherwise it's impossible to tell whether calls to Stop()
    // occur as a side-effect of the destruction or from the code intended to be
    // under test.
    if (proximity_monitor())
      testing::Mock::VerifyAndClearExpectations(proximity_monitor());

    // The UnlockManager must be destroyed before calling
    // chromeos::DBusThreadManager::Shutdown(), as the UnlockManager's
    // destructor references the DBusThreadManager.
    unlock_manager_.reset();

#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Shutdown();
#endif

    ScreenlockBridge::Get()->SetLockHandler(nullptr);
  }

  void CreateUnlockManager(
      ProximityAuthSystem::ScreenlockType screenlock_type) {
    unlock_manager_.reset(
        new TestUnlockManager(screenlock_type, &proximity_auth_client_));
  }

  void SimulateUserPresentState() {
    ON_CALL(life_cycle_, GetState())
        .WillByDefault(Return(RemoteDeviceLifeCycle::State::STOPPED));
    unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

    ON_CALL(life_cycle_, GetState())
        .WillByDefault(
            Return(RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
    unlock_manager_->OnLifeCycleStateChanged();

    unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);
  }

  void RunPendingTasks() { task_runner_->RunPendingTasks(); }

  MockProximityMonitor* proximity_monitor() {
    return unlock_manager_ ? unlock_manager_->proximity_monitor() : nullptr;
  }

 protected:
  cryptauth::RemoteDevice remote_device_;

  // Mock used for verifying interactions with the Bluetooth subsystem.
  scoped_refptr<device::MockBluetoothAdapter> bluetooth_adapter_;

  NiceMock<MockProximityAuthClient> proximity_auth_client_;
  NiceMock<MockRemoteDeviceLifeCycle> life_cycle_;
  NiceMock<MockMessenger> messenger_;
  std::unique_ptr<TestUnlockManager> unlock_manager_;
  cryptauth::FakeSecureContext secure_context_;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;
  FakeLockHandler lock_handler_;
  ScopedDisableLoggingForTesting disable_logging_;
};

TEST_F(ProximityAuthUnlockManagerTest, IsUnlockAllowed_InitialState) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest, IsUnlockAllowed_SessionLock_AllGood) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_TRUE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest, IsUnlockAllowed_SignIn_AllGood) {
  CreateUnlockManager(ProximityAuthSystem::SIGN_IN);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(Return(RemoteDeviceLifeCycle::State::STOPPED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->OnLifeCycleStateChanged();

  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_TRUE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_SignIn_MessengerDoesNotSupportSignIn) {
  CreateUnlockManager(ProximityAuthSystem::SIGN_IN);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(Return(RemoteDeviceLifeCycle::State::STOPPED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->OnLifeCycleStateChanged();

  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(false));
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest, IsUnlockAllowed_SignIn_MessengerIsNull) {
  CreateUnlockManager(ProximityAuthSystem::SIGN_IN);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
  ON_CALL(life_cycle_, GetMessenger()).WillByDefault(Return(nullptr));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_DisallowedByProximityMonitor) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  ON_CALL(*proximity_monitor(), IsUnlockAllowed()).WillByDefault(Return(false));
  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_SecureChannelNotEstablished) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(Return(RemoteDeviceLifeCycle::State::AUTHENTICATING));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_RemoteDeviceLifeCycleIsNull) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_RemoteScreenlockStateLocked) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenLocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_RemoteScreenlockStateUnknown) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenlockStateUnknown);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_RemoteScreenlockStateDisabled) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenlockDisabled);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest,
       IsUnlockAllowed_RemoteScreenlockStateNotYetReceived) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerTest, SetRemoteDeviceLifeCycle_SetToNull) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::INACTIVE));
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

TEST_F(ProximityAuthUnlockManagerTest,
       SetRemoteDeviceLifeCycle_ExistingRemoteDeviceLifeCycle) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, UpdateScreenlockState(_)).Times(0);
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
}

TEST_F(ProximityAuthUnlockManagerTest,
       SetRemoteDeviceLifeCycle_NullThenExistingRemoteDeviceLifeCycle) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::INACTIVE));
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::AUTHENTICATED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
}

TEST_F(ProximityAuthUnlockManagerTest,
       SetRemoteDeviceLifeCycle_AuthenticationFailed) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED));
  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::PHONE_NOT_AUTHENTICATED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
}

TEST_F(ProximityAuthUnlockManagerTest, SetRemoteDeviceLifeCycle_WakingUp) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(Return(RemoteDeviceLifeCycle::State::FINDING_CONNECTION));
  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
}

TEST_F(ProximityAuthUnlockManagerTest,
       SetRemoteDeviceLifeCycle_NullRemoteDeviceLifeCycle_NoProximityMonitor) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

TEST_F(
    ProximityAuthUnlockManagerTest,
    SetRemoteDeviceLifeCycle_ConnectingRemoteDeviceLifeCycle_StopsProximityMonitor) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(Return(RemoteDeviceLifeCycle::State::FINDING_CONNECTION));

  EXPECT_CALL(*proximity_monitor(), Stop()).Times(AtLeast(1));
  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(
    ProximityAuthUnlockManagerTest,
    SetRemoteDeviceLifeCycle_ConnectedRemoteDeviceLifeCycle_StartsProximityMonitor) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(Return(RemoteDeviceLifeCycle::State::STOPPED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
  EXPECT_CALL(*proximity_monitor(), Start()).Times(AtLeast(1));
  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnLifeCycleStateChanged_SecureChannelEstablished_RegistersAsObserver) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();
  EXPECT_CALL(messenger_, AddObserver(unlock_manager_.get()));
  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnLifeCycleStateChanged_StartsProximityMonitor) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();
  EXPECT_CALL(*proximity_monitor(), Start()).Times(AtLeast(1));
  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnLifeCycleStateChanged_StopsProximityMonitor) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED));

  EXPECT_CALL(*proximity_monitor(), Stop()).Times(AtLeast(1));
  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnLifeCycleStateChanged_Stopped_UpdatesScreenlockState) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(Return(RemoteDeviceLifeCycle::State::STOPPED));

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::INACTIVE));
  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnLifeCycleStateChanged_AuthenticationFailed_UpdatesScreenlockState) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED));

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::PHONE_NOT_AUTHENTICATED));
  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnLifeCycleStateChanged_FindingConnection_UpdatesScreenlockState) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(Return(RemoteDeviceLifeCycle::State::STOPPED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(Return(RemoteDeviceLifeCycle::State::FINDING_CONNECTION));

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnLifeCycleStateChanged_Authenticating_UpdatesScreenlockState) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(Return(RemoteDeviceLifeCycle::State::STOPPED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(Return(RemoteDeviceLifeCycle::State::AUTHENTICATING));

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(
    ProximityAuthUnlockManagerTest,
    OnLifeCycleStateChanged_SecureChannelEstablished_UpdatesScreenlockState) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(Return(RemoteDeviceLifeCycle::State::STOPPED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->OnLifeCycleStateChanged();
}

TEST_F(ProximityAuthUnlockManagerTest, OnDisconnected_UnregistersAsObserver) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED));

  EXPECT_CALL(messenger_, RemoveObserver(unlock_manager_.get()))
      .Times(testing::AtLeast(1));
  unlock_manager_.get()->OnDisconnected();
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnScreenDidUnlock_StopsProximityMonitor) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(*proximity_monitor(), Stop());
  unlock_manager_.get()->OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::LOCK_SCREEN);
}

TEST_F(ProximityAuthUnlockManagerTest, OnScreenDidLock_StartsProximityMonitor) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(Return(RemoteDeviceLifeCycle::State::STOPPED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(
          Return(RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED));
  unlock_manager_->OnLifeCycleStateChanged();

  EXPECT_CALL(*proximity_monitor(), Start());
  unlock_manager_.get()->OnScreenDidLock(
      ScreenlockBridge::LockHandler::LOCK_SCREEN);
}

TEST_F(ProximityAuthUnlockManagerTest, OnScreenDidLock_SetsWakingUpState) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_.get()->OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::LOCK_SCREEN);

  ON_CALL(life_cycle_, GetState())
      .WillByDefault(Return(RemoteDeviceLifeCycle::State::FINDING_CONNECTION));
  unlock_manager_->OnLifeCycleStateChanged();

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_.get()->OnScreenDidLock(
      ScreenlockBridge::LockHandler::LOCK_SCREEN);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnDecryptResponse_NoAuthAttemptInProgress) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_.get()->OnDecryptResponse(kSignInSecret);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnUnlockEventSent_NoAuthAttemptInProgress) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_.get()->OnUnlockEventSent(true);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnUnlockResponse_NoAuthAttemptInProgress) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_.get()->OnUnlockResponse(true);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_NoRemoteDeviceLifeCycle) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerTest, OnAuthAttempted_UnlockNotAllowed) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  ON_CALL(*proximity_monitor(), IsUnlockAllowed()).WillByDefault(Return(false));

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerTest, OnAuthAttempted_NotUserClick) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_->OnAuthAttempted(
      ScreenlockBridge::LockHandler::EXPAND_THEN_USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerTest, OnAuthAttempted_DuplicateCall) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(messenger_, RequestUnlock()).Times(0);
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerTest, OnAuthAttempted_TimesOut) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  // Simulate the timeout period elapsing.
  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  RunPendingTasks();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_DoesntTimeOutFollowingResponse) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_));
  unlock_manager_->OnUnlockResponse(false);

  // Simulate the timeout period elapsing.
  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  RunPendingTasks();
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_Unlock_SupportsSignIn_UnlockRequestFails) {
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnUnlockResponse(false);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_Unlock_WithSignIn_RequestSucceeds_EventSendFails) {
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnUnlockResponse(true);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnUnlockEventSent(false);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_Unlock_WithSignIn_RequestSucceeds_EventSendSucceeds) {
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnUnlockResponse(true);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(true));
  unlock_manager_->OnUnlockEventSent(true);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_Unlock_DoesntSupportSignIn_UnlockEventSendFails) {
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(false));
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnUnlockEventSent(false);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_Unlock_SupportsSignIn_UnlockEventSendSucceeds) {
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(false));
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(true));
  unlock_manager_->OnUnlockEventSent(true);
}

TEST_F(ProximityAuthUnlockManagerTest, OnAuthAttempted_SignIn_Success) {
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(ProximityAuthSystem::SIGN_IN);
  SimulateUserPresentState();

  std::string channel_binding_data = secure_context_.GetChannelBindingData();
  EXPECT_CALL(proximity_auth_client_,
              GetChallengeForUserAndDevice(remote_device_.user_id,
                                           remote_device_.public_key,
                                           channel_binding_data, _))
      .WillOnce(Invoke(
          [](const std::string& user_id, const std::string& public_key,
             const std::string& channel_binding_data,
             base::Callback<void(const std::string& challenge)> callback) {
            callback.Run(kChallenge);
          }));

  EXPECT_CALL(messenger_, RequestDecryption(kChallenge));
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnDecryptResponse(kSignInSecret);

  EXPECT_CALL(proximity_auth_client_, FinalizeSignin(kSignInSecret));
  unlock_manager_->OnUnlockEventSent(true);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_SignIn_UnlockEventSendFails) {
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(ProximityAuthSystem::SIGN_IN);
  SimulateUserPresentState();

  std::string channel_binding_data = secure_context_.GetChannelBindingData();
  EXPECT_CALL(proximity_auth_client_,
              GetChallengeForUserAndDevice(remote_device_.user_id,
                                           remote_device_.public_key,
                                           channel_binding_data, _))
      .WillOnce(Invoke(
          [](const std::string& user_id, const std::string& public_key,
             const std::string& channel_binding_data,
             base::Callback<void(const std::string& challenge)> callback) {
            callback.Run(kChallenge);
          }));

  EXPECT_CALL(messenger_, RequestDecryption(kChallenge));
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnDecryptResponse(kSignInSecret);

  EXPECT_CALL(proximity_auth_client_, FinalizeSignin(std::string()));
  unlock_manager_->OnUnlockEventSent(false);
}

TEST_F(ProximityAuthUnlockManagerTest,
       OnAuthAttempted_SignIn_DecryptRequestFails) {
  ON_CALL(messenger_, SupportsSignIn()).WillByDefault(Return(true));
  CreateUnlockManager(ProximityAuthSystem::SIGN_IN);
  SimulateUserPresentState();

  std::string channel_binding_data = secure_context_.GetChannelBindingData();
  EXPECT_CALL(proximity_auth_client_,
              GetChallengeForUserAndDevice(remote_device_.user_id,
                                           remote_device_.public_key,
                                           channel_binding_data, _))
      .WillOnce(Invoke(
          [](const std::string& user_id, const std::string& public_key,
             const std::string& channel_binding_data,
             base::Callback<void(const std::string& challenge)> callback) {
            callback.Run(kChallenge);
          }));

  EXPECT_CALL(messenger_, RequestDecryption(kChallenge));
  unlock_manager_->OnAuthAttempted(ScreenlockBridge::LockHandler::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeSignin(std::string()));
  unlock_manager_->OnDecryptResponse(std::string());
}

}  // namespace proximity_auth
