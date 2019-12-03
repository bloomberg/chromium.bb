// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/debug_logs_manager.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_debug_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace bluetooth {

namespace {

constexpr char kTestGooglerEmail[] = "user@google.com";
constexpr char kTestNonGooglerEmail[] = "user@gmail.com";

}  // namespace

class DebugLogsManagerTest : public testing::Test {
 public:
  DebugLogsManagerTest() = default;

  void SetUp() override {
    DebugLogsManager::RegisterPrefs(prefs_.registry());

    auto fake_bluetooth_debug_manager_client =
        std::make_unique<bluez::FakeBluetoothDebugManagerClient>();
    fake_bluetooth_debug_manager_client_ =
        fake_bluetooth_debug_manager_client.get();

    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();
    dbus_setter->SetBluetoothDebugManagerClient(
        std::unique_ptr<bluez::BluetoothDebugManagerClient>(
            std::move(fake_bluetooth_debug_manager_client)));
  }

  void TearDown() override {
    debug_logs_manager_.reset();
    bluez::BluezDBusManager::Shutdown();
  }

  void SetDebugFlagState(bool debug_flag_enabled) {
    feature_list_.InitWithFeatureState(
        chromeos::features::kShowBluetoothDebugLogToggle, debug_flag_enabled);
  }

  void InstantiateDebugManager(const char* email) {
    debug_logs_manager_ = std::make_unique<DebugLogsManager>(email, &prefs_);
  }

  void DestroyDebugManager() { debug_logs_manager_.reset(); }

  DebugLogsManager* debug_manager() const { return debug_logs_manager_.get(); }

  bluez::FakeBluetoothDebugManagerClient* fake_bluetooth_debug_manager_client()
      const {
    return fake_bluetooth_debug_manager_client_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  bluez::FakeBluetoothDebugManagerClient* fake_bluetooth_debug_manager_client_;
  std::unique_ptr<DebugLogsManager> debug_logs_manager_;
  TestingPrefServiceSimple prefs_;

  DISALLOW_COPY_AND_ASSIGN(DebugLogsManagerTest);
};

TEST_F(DebugLogsManagerTest, FlagNotEnabled) {
  SetDebugFlagState(false /* debug_flag_enabled */);
  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kNotSupported);
}

TEST_F(DebugLogsManagerTest, NonGoogler) {
  SetDebugFlagState(true /* debug_flag_enabled */);
  InstantiateDebugManager(kTestNonGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kNotSupported);
}

TEST_F(DebugLogsManagerTest, ChangeDebugLogsState) {
  SetDebugFlagState(true /* debug_flag_enabled */);
  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedButDisabled);

  debug_manager()->ChangeDebugLogsState(
      true /* should_debug_logs_be_enabled */);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedAndEnabled);

  // Despite DebugLogsManager is destroyed, the state of Debug logs is saved
  DestroyDebugManager();
  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedAndEnabled);

  debug_manager()->ChangeDebugLogsState(
      false /* should_debug_logs_be_enabled */);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedButDisabled);

  DestroyDebugManager();
  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedButDisabled);
}

TEST_F(DebugLogsManagerTest, SendVerboseLogsRequestUponLoginAndLogout) {
  SetDebugFlagState(true /* debug_flag_enabled */);
  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->dispatcher_level(), 0);
  debug_manager()->ChangeDebugLogsState(
      true /* should_debug_logs_be_enabled */);
  // Dispatcher level is updated only on login/logout, so now it stays the same.
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->dispatcher_level(), 0);

  // Deletion and Recreation of DebugManager simulates logout-login event
  DestroyDebugManager();
  InstantiateDebugManager(kTestGooglerEmail);
  // After login, dispatcher level should change
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->dispatcher_level(), 1);

  DestroyDebugManager();
  // After logout, dispatcher level should reset to 0
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->dispatcher_level(), 0);

  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->dispatcher_level(), 1);

  debug_manager()->ChangeDebugLogsState(
      false /* should_debug_logs_be_enabled */);
  // Dispatcher level is updated only on login/logout, so now it stays the same.
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->dispatcher_level(), 1);
  DestroyDebugManager();
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->dispatcher_level(), 0);

  InstantiateDebugManager(kTestGooglerEmail);
  // dispatcher level should still be 0 because logging is disabled
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->dispatcher_level(), 0);
}

TEST_F(DebugLogsManagerTest, RetryUponSetVerboseLogsFailure) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  SetDebugFlagState(true /* debug_flag_enabled */);
  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->dispatcher_level(), 0);
  debug_manager()->ChangeDebugLogsState(
      true /* should_debug_logs_be_enabled */);

  DestroyDebugManager();
  fake_bluetooth_debug_manager_client()->MakeNextSetLogLevelsFail();
  InstantiateDebugManager(kTestGooglerEmail);
  task_environment.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(fake_bluetooth_debug_manager_client()->set_log_levels_fail_count(),
            1);
  // Message is re-sent upon failing, eventually dispatcher level should change.
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->dispatcher_level(), 1);
}

}  // namespace bluetooth

}  // namespace chromeos
