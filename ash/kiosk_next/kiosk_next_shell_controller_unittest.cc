// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/kiosk_next/kiosk_next_shell_controller.h"

#include <memory>
#include <utility>

#include "ash/kiosk_next/kiosk_next_shell_observer.h"
#include "ash/kiosk_next/mock_kiosk_next_shell_client.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"

namespace ash {
namespace {

const char kTestUserEmail[] = "primary_user1@test.com";

KioskNextShellController* GetKioskNextShellController() {
  return Shell::Get()->kiosk_next_shell_controller();
}

class MockKioskNextShellObserver : public KioskNextShellObserver {
 public:
  MockKioskNextShellObserver() = default;
  ~MockKioskNextShellObserver() override = default;

  MOCK_METHOD0(OnKioskNextEnabled, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKioskNextShellObserver);
};

class KioskNextShellControllerTest : public AshTestBase {
 public:
  KioskNextShellControllerTest() = default;

  ~KioskNextShellControllerTest() override = default;

  void SetUp() override {
    set_start_session(false);
    AshTestBase::SetUp();
    client_ = BindMockKioskNextShellClient();
    scoped_feature_list_.InitAndEnableFeature(features::kKioskNextShell);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    client_.reset();
  }

 protected:
  void LoginKioskNextUser() {
    TestSessionControllerClient* session_controller_client =
        GetSessionControllerClient();

    // Create session for KioskNext user.
    session_controller_client->AddUserSession(
        kTestUserEmail, user_manager::USER_TYPE_REGULAR,
        true /* enable_settings */, false /* provide_pref_service */);

    // Create a KioskNext User and register its preferences.
    auto pref_service = std::make_unique<TestingPrefServiceSimple>();

    // Initialize the default preferences.
    Shell::RegisterUserProfilePrefs(pref_service->registry(),
                                    true /* for_test */);

    // Set the user's KioskNextShell preference.
    pref_service->SetUserPref(prefs::kKioskNextShellEnabled,
                              std::make_unique<base::Value>(true));

    // Provide PrefService for test user.
    Shell::Get()->session_controller()->ProvideUserPrefServiceForTest(
        AccountId::FromUserEmail(kTestUserEmail), std::move(pref_service));

    session_controller_client->SwitchActiveUser(
        AccountId::FromUserEmail(kTestUserEmail));
    session_controller_client->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

  std::unique_ptr<MockKioskNextShellClient> client_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KioskNextShellControllerTest);
};

// Ensures that KioskNextShell is not enabled when conditions are not met.
TEST_F(KioskNextShellControllerTest, TestKioskNextNotEnabled) {
  // No user has logged in yet.
  EXPECT_FALSE(GetKioskNextShellController()->IsEnabled());

  // Login a regular user whose KioskNext pref has not been enabled.
  SimulateNewUserFirstLogin("primary_user@test.com");

  // KioskNextShell is not enabled for regular users by default.
  EXPECT_FALSE(GetKioskNextShellController()->IsEnabled());
}

// Ensures that KioskNextShell is enabled when conditions are met.
// Ensures that LaunchKioskNextShell is called when KioskNextUser logs in.
TEST_F(KioskNextShellControllerTest, TestKioskNextLaunchShellWhenEnabled) {
  EXPECT_CALL(*client_, LaunchKioskNextShell(::testing::_)).Times(1);
  LoginKioskNextUser();
  EXPECT_TRUE(GetKioskNextShellController()->IsEnabled());
}

// Ensures that observers are notified when KioskNextUser logs in.
TEST_F(KioskNextShellControllerTest, TestKioskNextObserverNotification) {
  auto observer = std::make_unique<MockKioskNextShellObserver>();
  GetKioskNextShellController()->AddObserver(observer.get());
  EXPECT_CALL(*observer, OnKioskNextEnabled).Times(1);
  LoginKioskNextUser();
  GetKioskNextShellController()->RemoveObserver(observer.get());
}

}  // namespace
}  // namespace ash
