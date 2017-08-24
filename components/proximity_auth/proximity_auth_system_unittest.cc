// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/proximity_auth_system.h"

#include "base/command_line.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cryptauth/remote_device.h"
#include "components/proximity_auth/fake_lock_handler.h"
#include "components/proximity_auth/fake_remote_device_life_cycle.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/mock_proximity_auth_client.h"
#include "components/proximity_auth/proximity_auth_profile_pref_manager.h"
#include "components/proximity_auth/switches.h"
#include "components/proximity_auth/unlock_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using cryptauth::RemoteDevice;
using cryptauth::RemoteDeviceList;
using testing::AnyNumber;
using testing::AtLeast;
using testing::InSequence;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;
using testing::_;

namespace proximity_auth {

namespace {

const char kUser1[] = "user1";
const char kUser2[] = "user2";

const int64_t kLastPasswordEntryTimestampMs = 123456L;
const int64_t kTimestampBeforeReauthMs = 123457L;
const int64_t kTimestampAfterReauthMs = 123457890123L;

void CompareRemoteDeviceLists(const RemoteDeviceList& list1,
                              const RemoteDeviceList& list2) {
  ASSERT_EQ(list1.size(), list2.size());
  for (size_t i = 0; i < list1.size(); ++i) {
    RemoteDevice device1 = list1[i];
    RemoteDevice device2 = list2[i];
    EXPECT_EQ(device1.public_key, device2.public_key);
  }
}

// Creates a RemoteDevice object for |user_id| with |name|.
RemoteDevice CreateRemoteDevice(const std::string& user_id,
                                const std::string& name) {
  return RemoteDevice(user_id, name + "_pk", name, name + "_btaddr",
                      name + "_psk", name + "_challenge");
}

// Mock implementation of UnlockManager.
class MockUnlockManager : public UnlockManager {
 public:
  MockUnlockManager() {}
  ~MockUnlockManager() override {}
  MOCK_METHOD0(IsUnlockAllowed, bool());
  MOCK_METHOD1(SetRemoteDeviceLifeCycle, void(RemoteDeviceLifeCycle*));
  MOCK_METHOD0(OnLifeCycleStateChanged, void());
  MOCK_METHOD1(OnAuthAttempted, void(mojom::AuthType));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUnlockManager);
};

// Mock implementation of ProximityAuthProfilePrefManager.
class MockProximityAuthPrefManager : public ProximityAuthProfilePrefManager {
 public:
  MockProximityAuthPrefManager() : ProximityAuthProfilePrefManager(nullptr) {}
  ~MockProximityAuthPrefManager() override {}
  MOCK_CONST_METHOD0(GetLastPasswordEntryTimestampMs, int64_t());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProximityAuthPrefManager);
};

// Harness for ProximityAuthSystem to make it testable.
class TestableProximityAuthSystem : public ProximityAuthSystem {
 public:
  TestableProximityAuthSystem(ScreenlockType screenlock_type,
                              ProximityAuthClient* proximity_auth_client,
                              std::unique_ptr<UnlockManager> unlock_manager,
                              std::unique_ptr<base::Clock> clock,
                              ProximityAuthPrefManager* pref_manager)
      : ProximityAuthSystem(screenlock_type,
                            proximity_auth_client,
                            std::move(unlock_manager),
                            std::move(clock),
                            pref_manager),
        life_cycle_(nullptr) {}
  ~TestableProximityAuthSystem() override {}

  FakeRemoteDeviceLifeCycle* life_cycle() { return life_cycle_; }

 private:
  std::unique_ptr<RemoteDeviceLifeCycle> CreateRemoteDeviceLifeCycle(
      const RemoteDevice& remote_device) override {
    std::unique_ptr<FakeRemoteDeviceLifeCycle> life_cycle(
        new FakeRemoteDeviceLifeCycle(remote_device));
    life_cycle_ = life_cycle.get();
    return std::move(life_cycle);
  }

  FakeRemoteDeviceLifeCycle* life_cycle_;

  DISALLOW_COPY_AND_ASSIGN(TestableProximityAuthSystem);
};

}  // namespace

class ProximityAuthSystemTest : public testing::Test {
 protected:
  ProximityAuthSystemTest()
      : pref_manager_(new NiceMock<MockProximityAuthPrefManager>()),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_) {}

  void SetUp() override {
    user1_remote_devices_.push_back(
        CreateRemoteDevice(kUser1, "user1_device1"));
    user1_remote_devices_.push_back(
        CreateRemoteDevice(kUser1, "user1_device2"));

    user2_remote_devices_.push_back(
        CreateRemoteDevice(kUser2, "user2_device1"));
    user2_remote_devices_.push_back(
        CreateRemoteDevice(kUser2, "user2_device2"));
    user2_remote_devices_.push_back(
        CreateRemoteDevice(kUser2, "user2_device3"));

    InitProximityAuthSystem(ProximityAuthSystem::SESSION_LOCK);
    proximity_auth_system_->SetRemoteDevicesForUser(
        AccountId::FromUserEmail(kUser1), user1_remote_devices_);
    proximity_auth_system_->Start();
    LockScreen();
  }

  void TearDown() override { UnlockScreen(); }

  void InitProximityAuthSystem(ProximityAuthSystem::ScreenlockType type) {
    std::unique_ptr<MockUnlockManager> unlock_manager(
        new NiceMock<MockUnlockManager>());
    unlock_manager_ = unlock_manager.get();

    std::unique_ptr<base::SimpleTestClock> clock =
        base::MakeUnique<base::SimpleTestClock>();
    clock_ = clock.get();

    clock_->SetNow(base::Time::FromJavaTime(kTimestampBeforeReauthMs));
    ON_CALL(*pref_manager_, GetLastPasswordEntryTimestampMs())
        .WillByDefault(Return(kLastPasswordEntryTimestampMs));

    proximity_auth_system_.reset(new TestableProximityAuthSystem(
        type, &proximity_auth_client_, std::move(unlock_manager),
        std::move(clock), pref_manager_.get()));
  }

  void LockScreen() {
    ScreenlockBridge::Get()->SetFocusedUser(AccountId());
    ScreenlockBridge::Get()->SetLockHandler(&lock_handler_);
  }

  void FocusUser(const std::string& user_id) {
    ScreenlockBridge::Get()->SetFocusedUser(AccountId::FromUserEmail(user_id));
  }

  void UnlockScreen() { ScreenlockBridge::Get()->SetLockHandler(nullptr); }

  void SimulateSuspend() {
    proximity_auth_system_->OnSuspend();
    proximity_auth_system_->OnSuspendDone();
    task_runner_->RunUntilIdle();
  }

  FakeRemoteDeviceLifeCycle* life_cycle() {
    return proximity_auth_system_->life_cycle();
  }

  FakeLockHandler lock_handler_;
  NiceMock<MockProximityAuthClient> proximity_auth_client_;
  std::unique_ptr<TestableProximityAuthSystem> proximity_auth_system_;
  MockUnlockManager* unlock_manager_;
  base::SimpleTestClock* clock_;
  std::unique_ptr<MockProximityAuthPrefManager> pref_manager_;

  RemoteDeviceList user1_remote_devices_;
  RemoteDeviceList user2_remote_devices_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;

 private:
  ScopedDisableLoggingForTesting disable_logging_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthSystemTest);
};

TEST_F(ProximityAuthSystemTest, SetRemoteDevicesForUser_NotStarted) {
  InitProximityAuthSystem(ProximityAuthSystem::SESSION_LOCK);

  AccountId account1 = AccountId::FromUserEmail(kUser1);
  AccountId account2 = AccountId::FromUserEmail(kUser2);
  proximity_auth_system_->SetRemoteDevicesForUser(account1,
                                                  user1_remote_devices_);
  proximity_auth_system_->SetRemoteDevicesForUser(account2,
                                                  user2_remote_devices_);

  CompareRemoteDeviceLists(
      user1_remote_devices_,
      proximity_auth_system_->GetRemoteDevicesForUser(account1));

  CompareRemoteDeviceLists(
      user2_remote_devices_,
      proximity_auth_system_->GetRemoteDevicesForUser(account2));

  CompareRemoteDeviceLists(
      RemoteDeviceList(),
      proximity_auth_system_->GetRemoteDevicesForUser(
          AccountId::FromUserEmail("non_existent_user@google.com")));
}

TEST_F(ProximityAuthSystemTest, SetRemoteDevicesForUser_Started) {
  InitProximityAuthSystem(ProximityAuthSystem::SESSION_LOCK);
  AccountId account1 = AccountId::FromUserEmail(kUser1);
  AccountId account2 = AccountId::FromUserEmail(kUser2);
  proximity_auth_system_->SetRemoteDevicesForUser(account1,
                                                  user1_remote_devices_);
  proximity_auth_system_->Start();
  proximity_auth_system_->SetRemoteDevicesForUser(account2,
                                                  user2_remote_devices_);

  CompareRemoteDeviceLists(
      user1_remote_devices_,
      proximity_auth_system_->GetRemoteDevicesForUser(account1));

  CompareRemoteDeviceLists(
      user2_remote_devices_,
      proximity_auth_system_->GetRemoteDevicesForUser(account2));
}

TEST_F(ProximityAuthSystemTest, FocusRegisteredUser) {
  EXPECT_FALSE(life_cycle());
  EXPECT_EQ(std::string(),
            ScreenlockBridge::Get()->focused_account_id().GetUserEmail());

  RemoteDeviceLifeCycle* unlock_manager_life_cycle = nullptr;
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(_))
      .WillOnce(SaveArg<0>(&unlock_manager_life_cycle));
  FocusUser(kUser1);

  EXPECT_EQ(life_cycle(), unlock_manager_life_cycle);
  EXPECT_TRUE(life_cycle());
  EXPECT_TRUE(life_cycle()->started());
  EXPECT_EQ(kUser1, life_cycle()->GetRemoteDevice().user_id);

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AtLeast(1));
}

TEST_F(ProximityAuthSystemTest, FocusUnregisteredUser) {
  EXPECT_FALSE(life_cycle());
  EXPECT_EQ(std::string(),
            ScreenlockBridge::Get()->focused_account_id().GetUserEmail());
  EXPECT_FALSE(life_cycle());

  FocusUser(kUser2);
  EXPECT_FALSE(life_cycle());
}

TEST_F(ProximityAuthSystemTest, ToggleFocus_RegisteredUsers) {
  proximity_auth_system_->SetRemoteDevicesForUser(
      AccountId::FromUserEmail(kUser2), user2_remote_devices_);

  RemoteDeviceLifeCycle* life_cycle1 = nullptr;
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(_))
      .WillOnce(SaveArg<0>(&life_cycle1));
  FocusUser(kUser1);
  EXPECT_EQ(kUser1, life_cycle1->GetRemoteDevice().user_id);

  RemoteDeviceLifeCycle* life_cycle2 = nullptr;
  {
    InSequence sequence;
    EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
        .Times(AtLeast(1));
    EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(_))
        .WillOnce(SaveArg<0>(&life_cycle2));
  }
  FocusUser(kUser2);
  EXPECT_EQ(kUser2, life_cycle2->GetRemoteDevice().user_id);

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AtLeast(1));
}

TEST_F(ProximityAuthSystemTest, ToggleFocus_UnregisteredUsers) {
  FocusUser(kUser2);
  EXPECT_FALSE(life_cycle());

  FocusUser("unregistered-user");
  EXPECT_FALSE(life_cycle());

  FocusUser(kUser2);
  EXPECT_FALSE(life_cycle());
}

TEST_F(ProximityAuthSystemTest, ToggleFocus_RegisteredAndUnregisteredUsers) {
  // Focus User 1, who is registered. This should create a new life cycle.
  RemoteDeviceLifeCycle* life_cycle = nullptr;
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(_))
      .WillOnce(SaveArg<0>(&life_cycle));
  FocusUser(kUser1);
  EXPECT_EQ(kUser1, life_cycle->GetRemoteDevice().user_id);

  // User 2 has not been registered yet, so focusing them should not create a
  // new life cycle.
  life_cycle = nullptr;
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr));
  FocusUser(kUser2);
  EXPECT_FALSE(life_cycle);

  // Focusing back to User 1 should recreate a new life cycle.
  life_cycle = nullptr;
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(_))
      .WillOnce(SaveArg<0>(&life_cycle));
  FocusUser(kUser1);
  EXPECT_EQ(kUser1, life_cycle->GetRemoteDevice().user_id);

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AtLeast(1));
}

TEST_F(ProximityAuthSystemTest, ToggleFocus_SameUserRefocused) {
  RemoteDeviceLifeCycle* life_cycle = nullptr;
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(_))
      .WillOnce(SaveArg<0>(&life_cycle));
  FocusUser(kUser1);
  EXPECT_EQ(kUser1, life_cycle->GetRemoteDevice().user_id);

  // Focusing the user again should be idempotent. The screenlock UI may call
  // focus on the same user multiple times.
  // SetRemoteDeviceLifeCycle() is only expected to be called once.
  FocusUser(kUser1);

  // The RemoteDeviceLifeCycle should be nulled upon destruction.
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AtLeast(1));
}

TEST_F(ProximityAuthSystemTest, RestartSystem_UnregisteredUserFocused) {
  FocusUser(kUser2);

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AnyNumber());
  proximity_auth_system_->Stop();
  proximity_auth_system_->Start();
  EXPECT_FALSE(life_cycle());
}

TEST_F(ProximityAuthSystemTest, StopSystem_RegisteredUserFocused) {
  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(NotNull()));
  FocusUser(kUser1);

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AtLeast(1));
  proximity_auth_system_->Stop();

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(NotNull()));
  proximity_auth_system_->Start();
  EXPECT_EQ(kUser1, life_cycle()->GetRemoteDevice().user_id);
}

TEST_F(ProximityAuthSystemTest, OnLifeCycleStateChanged) {
  FocusUser(kUser1);

  EXPECT_CALL(*unlock_manager_, OnLifeCycleStateChanged());
  life_cycle()->ChangeState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);

  EXPECT_CALL(*unlock_manager_, OnLifeCycleStateChanged());
  life_cycle()->ChangeState(RemoteDeviceLifeCycle::State::AUTHENTICATING);

  EXPECT_CALL(*unlock_manager_, OnLifeCycleStateChanged());
  life_cycle()->ChangeState(
      RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
}

TEST_F(ProximityAuthSystemTest, OnAuthAttempted) {
  FocusUser(kUser1);
  EXPECT_CALL(*unlock_manager_, OnAuthAttempted(_));
  proximity_auth_system_->OnAuthAttempted(AccountId::FromUserEmail(kUser1));
}

TEST_F(ProximityAuthSystemTest, Suspend_ScreenUnlocked) {
  UnlockScreen();
  EXPECT_FALSE(life_cycle());
  SimulateSuspend();
  EXPECT_FALSE(life_cycle());
}

TEST_F(ProximityAuthSystemTest, Suspend_UnregisteredUserFocused) {
  SimulateSuspend();
  EXPECT_FALSE(life_cycle());
}

TEST_F(ProximityAuthSystemTest, Suspend_RegisteredUserFocused) {
  FocusUser(kUser1);

  {
    InSequence sequence;
    EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
        .Times(AtLeast(1));
    EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(NotNull()));
    SimulateSuspend();
  }

  EXPECT_EQ(kUser1, life_cycle()->GetRemoteDevice().user_id);

  EXPECT_CALL(*unlock_manager_, SetRemoteDeviceLifeCycle(nullptr))
      .Times(AtLeast(1));
}

TEST_F(ProximityAuthSystemTest, ForcePasswordReauth) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      proximity_auth::switches::kEnableForcePasswordReauth);
  ON_CALL(*pref_manager_, GetLastPasswordEntryTimestampMs())
      .WillByDefault(Return(kTimestampAfterReauthMs));
  EXPECT_CALL(proximity_auth_client_,
              UpdateScreenlockState(ScreenlockState::PASSWORD_REAUTH));
  FocusUser(kUser1);
  EXPECT_FALSE(life_cycle());
}

}  // namespace proximity_auth
