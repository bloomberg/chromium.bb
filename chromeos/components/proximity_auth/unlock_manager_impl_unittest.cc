// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/unlock_manager_impl.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/proximity_auth/fake_lock_handler.h"
#include "chromeos/components/proximity_auth/fake_remote_device_life_cycle.h"
#include "chromeos/components/proximity_auth/messenger.h"
#include "chromeos/components/proximity_auth/mock_proximity_auth_client.h"
#include "chromeos/components/proximity_auth/proximity_monitor.h"
#include "chromeos/components/proximity_auth/remote_device_life_cycle.h"
#include "chromeos/components/proximity_auth/remote_status_update.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_client_channel.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class MockMessenger : public Messenger {
 public:
  MockMessenger() {}
  ~MockMessenger() override {}

  MOCK_METHOD1(AddObserver, void(MessengerObserver* observer));
  MOCK_METHOD1(RemoveObserver, void(MessengerObserver* observer));
  MOCK_METHOD0(DispatchUnlockEvent, void());
  MOCK_METHOD1(RequestDecryption, void(const std::string& challenge));
  MOCK_METHOD0(RequestUnlock, void());
  MOCK_CONST_METHOD0(GetConnection, chromeos::secure_channel::Connection*());
  MOCK_CONST_METHOD0(GetChannel, chromeos::secure_channel::ClientChannel*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMessenger);
};

class MockProximityMonitor : public ProximityMonitor {
 public:
  explicit MockProximityMonitor(base::OnceClosure destroy_callback)
      : destroy_callback_(std::move(destroy_callback)), started_(false) {
    ON_CALL(*this, IsUnlockAllowed()).WillByDefault(Return(true));
  }
  ~MockProximityMonitor() override { std::move(destroy_callback_).Run(); }

  void Start() override { started_ = true; }
  void Stop() override {}
  MOCK_CONST_METHOD0(IsUnlockAllowed, bool());
  MOCK_METHOD0(RecordProximityMetricsOnAuthSuccess, void());

  bool started() { return started_; }

 private:
  base::OnceClosure destroy_callback_;
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(MockProximityMonitor);
};

class TestUnlockManager : public UnlockManagerImpl {
 public:
  TestUnlockManager(ProximityAuthSystem::ScreenlockType screenlock_type,
                    ProximityAuthClient* proximity_auth_client)
      : UnlockManagerImpl(screenlock_type, proximity_auth_client) {}
  ~TestUnlockManager() override {}

  using UnlockManager::OnAuthAttempted;
  using MessengerObserver::OnUnlockEventSent;
  using MessengerObserver::OnRemoteStatusUpdate;
  using MessengerObserver::OnDecryptResponse;
  using MessengerObserver::OnUnlockResponse;
  using MessengerObserver::OnDisconnected;

  MockProximityMonitor* proximity_monitor() { return proximity_monitor_; }
  bool proximity_monitor_destroyed() { return proximity_monitor_destroyed_; }

 private:
  std::unique_ptr<ProximityMonitor> CreateProximityMonitor(
      RemoteDeviceLifeCycle* life_cycle) override {
    std::unique_ptr<MockProximityMonitor> proximity_monitor(
        new NiceMock<MockProximityMonitor>(
            base::BindOnce(&TestUnlockManager::OnProximityMonitorDestroyed,
                           base::Unretained(this))));
    proximity_monitor_destroyed_ = false;

    proximity_monitor_ = proximity_monitor.get();
    return std::move(proximity_monitor);
  }

  void OnProximityMonitorDestroyed() { proximity_monitor_destroyed_ = true; }

  // Owned by the super class.
  MockProximityMonitor* proximity_monitor_ = nullptr;
  bool proximity_monitor_destroyed_ = false;

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

class ProximityAuthUnlockManagerImplTest : public testing::Test {
 public:
  ProximityAuthUnlockManagerImplTest()
      : remote_device_(chromeos::multidevice::CreateRemoteDeviceRefForTest()),
        local_device_(chromeos::multidevice::CreateRemoteDeviceRefForTest()),
        life_cycle_(remote_device_, local_device_),
        fake_client_channel_(
            std::make_unique<chromeos::secure_channel::FakeClientChannel>()),
        bluetooth_adapter_(CreateAndRegisterMockBluetoothAdapter()),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_) {}

  ~ProximityAuthUnlockManagerImplTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();

    ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(true));

    ON_CALL(messenger_, GetChannel())
        .WillByDefault(Return(fake_client_channel_.get()));
    life_cycle_.set_messenger(&messenger_);
    life_cycle_.set_channel(fake_client_channel_.get());
  }

  void TearDown() override {
    // Make sure to verify the mock prior to the destruction of the unlock
    // manager, as otherwise it's impossible to tell whether calls to Stop()
    // occur as a side-effect of the destruction or from the code intended to be
    // under test.
    if (proximity_monitor())
      testing::Mock::VerifyAndClearExpectations(proximity_monitor());

    unlock_manager_.reset();

    chromeos::PowerManagerClient::Shutdown();
  }

  void CreateUnlockManager(
      ProximityAuthSystem::ScreenlockType screenlock_type) {
    unlock_manager_.reset(
        new TestUnlockManager(screenlock_type, &proximity_auth_client_));

    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_bluetooth_suspension_recovery_timer_ = mock_timer.get();
    unlock_manager_->SetBluetoothSuspensionRecoveryTimerForTesting(
        std::move(mock_timer));
  }

  void SimulateUserPresentState() {
    unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
    life_cycle_.ChangeState(
        RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
    unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);
  }

  void RunPendingTasks() { task_runner_->RunPendingTasks(); }

  MockProximityMonitor* proximity_monitor() {
    return unlock_manager_ ? unlock_manager_->proximity_monitor() : nullptr;
  }

  bool proximity_monitor_destroyed() {
    return unlock_manager_ ? unlock_manager_->proximity_monitor_destroyed()
                           : false;
  }

 protected:
  chromeos::multidevice::RemoteDeviceRef remote_device_;
  chromeos::multidevice::RemoteDeviceRef local_device_;
  FakeRemoteDeviceLifeCycle life_cycle_;
  std::unique_ptr<chromeos::secure_channel::FakeClientChannel>
      fake_client_channel_;

  // Mock used for verifying interactions with the Bluetooth subsystem.
  scoped_refptr<device::MockBluetoothAdapter> bluetooth_adapter_;

  NiceMock<MockProximityAuthClient> proximity_auth_client_;
  NiceMock<MockMessenger> messenger_;
  std::unique_ptr<TestUnlockManager> unlock_manager_;
  base::MockOneShotTimer* mock_bluetooth_suspension_recovery_timer_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;
  FakeLockHandler lock_handler_;
  chromeos::multidevice::ScopedDisableLoggingForTesting disable_logging_;
};

TEST_F(ProximityAuthUnlockManagerImplTest, IsUnlockAllowed_InitialState) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_SessionLock_AllGood) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_TRUE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest, IsUnlockAllowed_SignIn_AllGood) {
  CreateUnlockManager(ProximityAuthSystem::SIGN_IN);
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);

  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_TRUE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_DisallowedByProximityMonitor) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);

  ON_CALL(*proximity_monitor(), IsUnlockAllowed()).WillByDefault(Return(false));
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);
  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteDeviceLifeCycleIsNull) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenUnlocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteScreenlockStateLocked) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenLocked);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest, IsUnlockAllowed_UserIsSecondary) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnRemoteStatusUpdate({USER_PRESENCE_SECONDARY,
                                         SECURE_SCREEN_LOCK_ENABLED,
                                         TRUST_AGENT_UNSUPPORTED});

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_PrimaryUserInBackground) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnRemoteStatusUpdate({USER_PRESENCE_BACKGROUND,
                                         SECURE_SCREEN_LOCK_ENABLED,
                                         TRUST_AGENT_UNSUPPORTED});

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteScreenlockStateUnknown) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenlockStateUnknown);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteScreenlockStateDisabled) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  unlock_manager_->OnRemoteStatusUpdate(kRemoteScreenlockDisabled);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       IsUnlockAllowed_RemoteScreenlockStateNotYetReceived) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);

  EXPECT_FALSE(unlock_manager_->IsUnlockAllowed());
}

TEST_F(ProximityAuthUnlockManagerImplTest, SetRemoteDeviceLifeCycle_SetToNull) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::INACTIVE));
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       SetRemoteDeviceLifeCycle_ExistingRemoteDeviceLifeCycle) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       SetRemoteDeviceLifeCycle_AuthenticationFailed) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);

  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::PHONE_NOT_AUTHENTICATED));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
}

TEST_F(ProximityAuthUnlockManagerImplTest, SetRemoteDeviceLifeCycle_WakingUp) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);

  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       SetRemoteDeviceLifeCycle_TimesOutBeforeConnection) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  life_cycle_.set_messenger(nullptr);
  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::NO_PHONE));
  // Simulate timing out before a connection is established.
  RunPendingTasks();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       SetRemoteDeviceLifeCycle_NullRemoteDeviceLifeCycle_NoProximityMonitor) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();
  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);
}

TEST_F(
    ProximityAuthUnlockManagerImplTest,
    SetRemoteDeviceLifeCycle_ConnectingRemoteDeviceLifeCycle_StopsProximityMonitor) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);
  EXPECT_TRUE(proximity_monitor_destroyed());
}

TEST_F(
    ProximityAuthUnlockManagerImplTest,
    SetRemoteDeviceLifeCycle_ConnectedRemoteDeviceLifeCycle_StartsProximityMonitor) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  EXPECT_TRUE(proximity_monitor()->started());
}

// Regression test for crbug.com/931929. Capture the case where the phone is
// connected to, connection is lost, and then a new connection is made shortly
// after.
TEST_F(ProximityAuthUnlockManagerImplTest,
       SetRemoteDeviceLifeCycle_TwiceConnectedRemoteDeviceLifeCycle) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);

  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  EXPECT_TRUE(proximity_monitor()->started());

  // Simulate the phone connection being lost. The ProximityMonitor is stale
  // and should have been destroyed.
  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);
  EXPECT_TRUE(proximity_monitor_destroyed());

  life_cycle_.ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
  EXPECT_FALSE(proximity_monitor_destroyed());
  EXPECT_TRUE(proximity_monitor()->started());
}

TEST_F(ProximityAuthUnlockManagerImplTest, BluetoothAdapterNotPresent) {
  ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(false));

  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::NO_BLUETOOTH));

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_FALSE(life_cycle_.started());
}

TEST_F(ProximityAuthUnlockManagerImplTest, BluetoothAdapterPowerChanges) {
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(false));

  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::NO_BLUETOOTH));

  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_FALSE(life_cycle_.started());

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(true));
  bluetooth_adapter_->NotifyAdapterPoweredChanged(true);
  EXPECT_TRUE(life_cycle_.started());
}

TEST_F(
    ProximityAuthUnlockManagerImplTest,
    CacheBluetoothAdapterStateAfterSuspendAndResume_AttemptConnectionWhileBluetoothAdapterIsStillRecovering) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ASSERT_FALSE(mock_bluetooth_suspension_recovery_timer_->IsRunning());

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_LID_CLOSED);

  // Simulate https://crbug.com/986896 by returning false for presence and power
  // directly after resuming, but do not fire
  // |mock_bluetooth_suspension_recovery_timer_|, simulating that not enough
  // time has passed for the BluetoothAdapter to recover. It's expected under
  // these conditions that:
  // * ProximityAuthClient::UpdateScreenlockState() never be called with
  //   ScreenlockState::NO_BLUETOOTH.
  // * ProximityAuthClient::UpdateScreenlockState() only be called once with
  //   ScreenlockState::BLUETOOTH_CONNECTING, because it should only be called
  //   on when the ScreenlockState value changes.
  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::NO_BLUETOOTH))
      .Times(0);
  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));

  ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(false));
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(false));

  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  EXPECT_TRUE(mock_bluetooth_suspension_recovery_timer_->IsRunning());

  // Simulate how ProximityAuthSystem, the owner of UnlockManager, reacts to
  // resume: providing a new RemoteDeviceLifeCycle. This shouldn't trigger a new
  // call to ProximityAuthClient::UpdateScreenlockState().
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_TRUE(life_cycle_.started());

  EXPECT_TRUE(mock_bluetooth_suspension_recovery_timer_->IsRunning());
}

TEST_F(
    ProximityAuthUnlockManagerImplTest,
    CacheBluetoothAdapterStateAfterSuspendAndResume_AttemptConnectionOnceBluetoothAdapterHasHadTimeToRecover) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ASSERT_FALSE(mock_bluetooth_suspension_recovery_timer_->IsRunning());

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_LID_CLOSED);

  // Simulate https://crbug.com/986896 by returning false for presence and power
  // directly after resuming, and then fire
  // |mock_bluetooth_suspension_recovery_timer_|, simulating that enough time
  // has passed for the BluetoothAdapter to recover - this means that Bluetooth
  // is truly off after resume and the user should be visually informed as such.
  // It's expected under these conditions that:
  // * ProximityAuthClient::UpdateScreenlockState() only be called once with
  //   ScreenlockState::NO_BLUETOOTH, but after the timer fires (this is
  //   impossible to explicitly do in code with mocks, unfortunately).
  // * ProximityAuthClient::UpdateScreenlockState() only be called once with
  //   ScreenlockState::BLUETOOTH_CONNECTING, directly after SuspendDone.
  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::NO_BLUETOOTH));
  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));

  ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(false));
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(false));

  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  EXPECT_TRUE(mock_bluetooth_suspension_recovery_timer_->IsRunning());

  // Simulate how ProximityAuthSystem, the owner of UnlockManager, reacts to
  // resume: providing a new RemoteDeviceLifeCycle. This shouldn't trigger a new
  // call to ProximityAuthClient::UpdateScreenlockState().
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_TRUE(life_cycle_.started());

  ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(false));
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(false));

  EXPECT_TRUE(mock_bluetooth_suspension_recovery_timer_->IsRunning());

  // This leads to ProximityAuthClient::UpdateScreenlockState() being called
  // with ScreenlockState::NO_BLUETOOTH.
  mock_bluetooth_suspension_recovery_timer_->Fire();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       BluetoothOffMessageShownImmediatelyIfBluetoothWasOffBeforeSuspend) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  ON_CALL(*bluetooth_adapter_, IsPresent()).WillByDefault(Return(false));
  ON_CALL(*bluetooth_adapter_, IsPowered()).WillByDefault(Return(false));

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_LID_CLOSED);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::NO_BLUETOOTH));
  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING))
      .Times(0);

  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();

  // Simulate how ProximityAuthSystem, the owner of UnlockManager, reacts to
  // resume: providing a new RemoteDeviceLifeCycle.
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_FALSE(life_cycle_.started());
}

TEST_F(ProximityAuthUnlockManagerImplTest, StartsProximityMonitor) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();
  EXPECT_TRUE(proximity_monitor()->started());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthenticationFailed_StopsProximityMonitor) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED);
  EXPECT_TRUE(proximity_monitor_destroyed());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       AuthenticationFailed_UpdatesScreenlockState) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::PHONE_NOT_AUTHENTICATED));
  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       FindingConnection_UpdatesScreenlockState) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  // Regression test for https://crbug.com/890047, ensuring that the NO_PHONE
  // status doesn't incorrectly appear for a brief moment before the
  // BLUETOOTH_CONNECTING spinner.
  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::NO_PHONE))
      .Times(0);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_TRUE(life_cycle_.started());
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       Authenticating_UpdatesScreenlockState) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);

  // Regression test for https://crbug.com/890047, ensuring that the NO_PHONE
  // status doesn't incorrectly appear for a brief moment before the
  // BLUETOOTH_CONNECTING spinner.
  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::NO_PHONE))
      .Times(0);

  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::BLUETOOTH_CONNECTING));
  unlock_manager_->SetRemoteDeviceLifeCycle(&life_cycle_);
  EXPECT_TRUE(life_cycle_.started());
  life_cycle_.ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATING);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnDecryptResponse_NoAuthAttemptInProgress) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_.get()->OnDecryptResponse(kSignInSecret);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnUnlockEventSent_NoAuthAttemptInProgress) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_.get()->OnUnlockEventSent(true);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnUnlockResponse_NoAuthAttemptInProgress) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_.get()->OnUnlockResponse(true);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_NoRemoteDeviceLifeCycle) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->SetRemoteDeviceLifeCycle(nullptr);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnAuthAttempted_UnlockNotAllowed) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  ON_CALL(*proximity_monitor(), IsUnlockAllowed()).WillByDefault(Return(false));

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnAuthAttempted_NotUserClick) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  unlock_manager_->OnAuthAttempted(mojom::AuthType::EXPAND_THEN_USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnAuthAttempted_DuplicateCall) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(messenger_, RequestUnlock()).Times(0);
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnAuthAttempted_TimesOut) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  // Simulate the timeout period elapsing.
  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  RunPendingTasks();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_DoesntTimeOutFollowingResponse) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_));
  unlock_manager_->OnUnlockResponse(false);

  // Simulate the timeout period elapsing.
  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(_)).Times(0);
  RunPendingTasks();
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_Unlock_UnlockRequestFails) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnUnlockResponse(false);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_Unlock_WithSignIn_RequestSucceeds_EventSendFails) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnUnlockResponse(true);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(false));
  unlock_manager_->OnUnlockEventSent(false);
}

TEST_F(ProximityAuthUnlockManagerImplTest,
       OnAuthAttempted_Unlock_RequestSucceeds_EventSendSucceeds) {
  CreateUnlockManager(ProximityAuthSystem::SESSION_LOCK);
  SimulateUserPresentState();

  EXPECT_CALL(messenger_, RequestUnlock());
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnUnlockResponse(true);

  EXPECT_CALL(proximity_auth_client_, FinalizeUnlock(true));
  unlock_manager_->OnUnlockEventSent(true);
}

TEST_F(ProximityAuthUnlockManagerImplTest, OnAuthAttempted_SignIn_Success) {
  CreateUnlockManager(ProximityAuthSystem::SIGN_IN);
  SimulateUserPresentState();

  std::string channel_binding_data = "channel binding data";

  EXPECT_CALL(proximity_auth_client_,
              GetChallengeForUserAndDevice(remote_device_.user_email(),
                                           remote_device_.public_key(),
                                           channel_binding_data, _))
      .WillOnce(Invoke(
          [](const std::string& user_id, const std::string& public_key,
             const std::string& channel_binding_data,
             base::Callback<void(const std::string& challenge)> callback) {
            callback.Run(kChallenge);
          }));

  EXPECT_CALL(messenger_, RequestDecryption(kChallenge));
  unlock_manager_->OnAuthAttempted(mojom::AuthType::USER_CLICK);

  std::vector<chromeos::secure_channel::mojom::ConnectionCreationDetail>
      creation_details{
          chromeos::secure_channel::mojom::ConnectionCreationDetail::
              REMOTE_DEVICE_USED_BACKGROUND_BLE_ADVERTISING};
  chromeos::secure_channel::mojom::ConnectionMetadataPtr
      connection_metadata_ptr =
          chromeos::secure_channel::mojom::ConnectionMetadata::New(
              creation_details, nullptr /* bluetooth_connection_metadata */,
              channel_binding_data);
  fake_client_channel_->InvokePendingGetConnectionMetadataCallback(
      std::move(connection_metadata_ptr));

  EXPECT_CALL(messenger_, DispatchUnlockEvent());
  unlock_manager_->OnDecryptResponse(kSignInSecret);

  EXPECT_CALL(proximity_auth_client_, FinalizeSignin(kSignInSecret));
  unlock_manager_->OnUnlockEventSent(true);
}

}  // namespace proximity_auth
