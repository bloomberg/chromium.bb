// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/night_light_controller.h"

#include "ash/public/cpp/config.h"
#include "ash/public/cpp/session_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_session_controller_client.h"
#include "ash/test/test_shell_delegate.h"
#include "base/macros.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/compositor/layer.h"

namespace ash {

namespace {

constexpr char user_1_email[] = "user1@nightlight";
constexpr char user_2_email[] = "user2@nightlight";

NightLightController* GetController() {
  return Shell::Get()->night_light_controller();
}

// Tests that the color temperatures of all root layers are equal to the given
// |expected_temperature| and returns true if so, or false otherwise.
bool TestLayersTemperature(float expected_temperature) {
  for (aura::Window* root_window : ash::Shell::GetAllRootWindows()) {
    ui::Layer* layer = root_window->layer();
    if (expected_temperature != layer->layer_temperature())
      return false;
  }

  return true;
}

class TestObserver : public NightLightController::Observer {
 public:
  TestObserver() { GetController()->AddObserver(this); }
  ~TestObserver() override { GetController()->RemoveObserver(this); }

  // ash::NightLightController::Observer:
  void OnNightLightEnabledChanged(bool enabled) override { status_ = enabled; }

  bool status() const { return status_; }

 private:
  bool status_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class NightLightTest : public test::AshTestBase {
 public:
  NightLightTest() = default;
  ~NightLightTest() override = default;

  void CreateTestUserSessions() {
    GetSessionControllerClient()->Reset();
    GetSessionControllerClient()->AddUserSession(user_1_email);
    GetSessionControllerClient()->AddUserSession(user_2_email);
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

  void InjectTestPrefService(PrefService* pref_service) {
    ash_test_helper()->test_shell_delegate()->set_active_user_pref_service(
        pref_service);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NightLightTest);
};

// Tests toggling NightLight on / off and makes sure the observer is updated and
// the layer temperatures are modified.
TEST_F(NightLightTest, TestToggle) {
  UpdateDisplay("800x600,800x600");

  TestObserver observer;
  NightLightController* controller = GetController();
  controller->SetEnabled(false);
  ASSERT_FALSE(controller->enabled());
  EXPECT_TRUE(TestLayersTemperature(0.0f));
  controller->Toggle();
  EXPECT_TRUE(controller->enabled());
  EXPECT_TRUE(observer.status());
  EXPECT_TRUE(TestLayersTemperature(GetController()->color_temperature()));
  controller->Toggle();
  EXPECT_FALSE(controller->enabled());
  EXPECT_FALSE(observer.status());
  EXPECT_TRUE(TestLayersTemperature(0.0f));
}

// Tests that switching users retrieves NightLight settings for the active
// user's prefs.
TEST_F(NightLightTest, TestUserSwitchAndSettingsPersistence) {
  if (Shell::GetAshConfig() == Config::MASH) {
    // User switching doesn't work on mash.
    return;
  }

  CreateTestUserSessions();
  TestingPrefServiceSimple user_1_pref_service;
  TestingPrefServiceSimple user_2_pref_service;
  NightLightController::RegisterPrefs(user_1_pref_service.registry());
  NightLightController::RegisterPrefs(user_2_pref_service.registry());

  // Simulate user 1 login.
  InjectTestPrefService(&user_1_pref_service);
  SwitchActiveUser(user_1_email);
  NightLightController* controller = GetController();
  controller->SetEnabled(true);
  EXPECT_TRUE(GetController()->enabled());
  EXPECT_TRUE(TestLayersTemperature(GetController()->color_temperature()));

  // Switch to user 2, and expect NightLight to be disabled.
  InjectTestPrefService(&user_2_pref_service);
  SwitchActiveUser(user_2_email);
  EXPECT_FALSE(controller->enabled());

  // Switch back to user 1, to find NightLight is still enabled.
  InjectTestPrefService(&user_1_pref_service);
  SwitchActiveUser(user_1_email);
  EXPECT_TRUE(controller->enabled());
}

}  // namespace

}  // namespace ash
