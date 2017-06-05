// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/night_light_controller.h"

#include <cmath>
#include <limits>

#include "ash/ash_switches.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/session_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_session_controller_client.h"
#include "ash/test/test_shell_delegate.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/compositor/layer.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@nightlight";
constexpr char kUser2Email[] = "user2@nightlight";

NightLightController* GetController() {
  return Shell::Get()->night_light_controller();
}

// Tests that the color temperatures of all root layers are equal to the given
// |expected_temperature| and returns true if so, or false otherwise.
bool TestLayersTemperature(float expected_temperature) {
  const float epsilon = std::numeric_limits<float>::epsilon();
  for (aura::Window* root_window : ash::Shell::GetAllRootWindows()) {
    ui::Layer* layer = root_window->layer();
    if (std::abs(expected_temperature - layer->layer_temperature()) > epsilon) {
      NOTREACHED() << "Unexpected layer temperature (" << expected_temperature
                   << " vs. " << layer->layer_temperature() << ").";
      return false;
    }
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

  TestingPrefServiceSimple* user1_pref_service() {
    return &user1_pref_service_;
  }
  TestingPrefServiceSimple* user2_pref_service() {
    return &user2_pref_service_;
  }

  // ash::test::AshTestBase:
  void SetUp() override {
    // Explicitly enable the NightLight feature for the tests.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableNightLight);

    test::AshTestBase::SetUp();
    CreateTestUserSessions();
    Shell::RegisterPrefs(user1_pref_service_.registry());
    Shell::RegisterPrefs(user2_pref_service_.registry());

    // Simulate user 1 login.
    InjectTestPrefService(&user1_pref_service_);
    SwitchActiveUser(kUser1Email);
  }

  void CreateTestUserSessions() {
    GetSessionControllerClient()->Reset();
    GetSessionControllerClient()->AddUserSession(kUser1Email);
    GetSessionControllerClient()->AddUserSession(kUser2Email);
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
  TestingPrefServiceSimple user1_pref_service_;
  TestingPrefServiceSimple user2_pref_service_;

  DISALLOW_COPY_AND_ASSIGN(NightLightTest);
};

// Tests toggling NightLight on / off and makes sure the observer is updated and
// the layer temperatures are modified.
TEST_F(NightLightTest, TestToggle) {
  if (Shell::GetAshConfig() == Config::MASH) {
    // PrefChangeRegistrar doesn't work on mash. crbug.com/721961.
    return;
  }

  UpdateDisplay("800x600,800x600");

  TestObserver observer;
  NightLightController* controller = GetController();
  controller->SetEnabled(false);
  ASSERT_FALSE(controller->GetEnabled());
  EXPECT_TRUE(TestLayersTemperature(0.0f));
  controller->Toggle();
  EXPECT_TRUE(controller->GetEnabled());
  EXPECT_TRUE(observer.status());
  EXPECT_TRUE(TestLayersTemperature(GetController()->GetColorTemperature()));
  controller->Toggle();
  EXPECT_FALSE(controller->GetEnabled());
  EXPECT_FALSE(observer.status());
  EXPECT_TRUE(TestLayersTemperature(0.0f));
}

// Tests setting the temperature in various situations.
TEST_F(NightLightTest, TestSetTemperature) {
  if (Shell::GetAshConfig() == Config::MASH) {
    // PrefChangeRegistrar doesn't work on mash. crbug.com/721961.
    return;
  }

  UpdateDisplay("800x600,800x600");

  TestObserver observer;
  NightLightController* controller = GetController();
  controller->SetEnabled(false);
  ASSERT_FALSE(controller->GetEnabled());

  // Setting the temperature while NightLight is disabled only changes the
  // color temperature field, but root layers temperatures are not affected, nor
  // the status of NightLight itself.
  const float temperature1 = 0.2f;
  controller->SetColorTemperature(temperature1);
  EXPECT_EQ(temperature1, controller->GetColorTemperature());
  EXPECT_TRUE(TestLayersTemperature(0.0f));

  // When NightLight is enabled, temperature changes actually affect the root
  // layers temperatures.
  controller->SetEnabled(true);
  ASSERT_TRUE(controller->GetEnabled());
  const float temperature2 = 0.7f;
  controller->SetColorTemperature(temperature2);
  EXPECT_EQ(temperature2, controller->GetColorTemperature());
  EXPECT_TRUE(TestLayersTemperature(temperature2));

  // When NightLight is disabled, the value of the color temperature field
  // doesn't change, however the temperatures set on the root layers are all
  // 0.0f. Observers only receive an enabled status change notification; no
  // temperature change notification.
  controller->SetEnabled(false);
  ASSERT_FALSE(controller->GetEnabled());
  EXPECT_FALSE(observer.status());
  EXPECT_EQ(temperature2, controller->GetColorTemperature());
  EXPECT_TRUE(TestLayersTemperature(0.0f));

  // When re-enabled, the stored temperature is re-applied.
  controller->SetEnabled(true);
  EXPECT_TRUE(observer.status());
  ASSERT_TRUE(controller->GetEnabled());
  EXPECT_TRUE(TestLayersTemperature(temperature2));
}

// Tests that switching users retrieves NightLight settings for the active
// user's prefs.
TEST_F(NightLightTest, TestUserSwitchAndSettingsPersistence) {
  if (Shell::GetAshConfig() == Config::MASH) {
    // PrefChangeRegistrar doesn't work on mash. crbug.com/721961.
    return;
  }

  // Test start with user1 logged in.
  NightLightController* controller = GetController();
  controller->SetEnabled(true);
  EXPECT_TRUE(controller->GetEnabled());
  const float user1_temperature = 0.8f;
  controller->SetColorTemperature(user1_temperature);
  EXPECT_EQ(user1_temperature, controller->GetColorTemperature());
  EXPECT_TRUE(TestLayersTemperature(user1_temperature));

  // Switch to user 2, and expect NightLight to be disabled.
  InjectTestPrefService(user2_pref_service());
  SwitchActiveUser(kUser2Email);
  EXPECT_FALSE(controller->GetEnabled());
  // Changing user_2's color temperature shouldn't affect user_1's settings.
  const float user2_temperature = 0.2f;
  user2_pref_service()->SetDouble(prefs::kNightLightTemperature,
                                  user2_temperature);
  EXPECT_EQ(user2_temperature, controller->GetColorTemperature());
  EXPECT_TRUE(TestLayersTemperature(0.0f));
  EXPECT_EQ(user1_temperature,
            user1_pref_service()->GetDouble(prefs::kNightLightTemperature));

  // Switch back to user 1, to find NightLight is still enabled, and the same
  // user's color temperature are re-applied.
  InjectTestPrefService(user1_pref_service());
  SwitchActiveUser(kUser1Email);
  EXPECT_TRUE(controller->GetEnabled());
  EXPECT_EQ(user1_temperature, controller->GetColorTemperature());
  EXPECT_TRUE(TestLayersTemperature(user1_temperature));
}

// Tests that changes from outside NightLightControlled to the NightLight
// Preferences are seen by the controlled and applied properly.
TEST_F(NightLightTest, TestOutsidePreferencesChangesAreApplied) {
  if (Shell::GetAshConfig() == Config::MASH) {
    // PrefChangeRegistrar doesn't work on mash. crbug.com/721961.
    return;
  }

  // Test start with user1 logged in.
  NightLightController* controller = GetController();
  user1_pref_service()->SetBoolean(prefs::kNightLightEnabled, true);
  EXPECT_TRUE(controller->GetEnabled());
  const float temperature1 = 0.65f;
  user1_pref_service()->SetDouble(prefs::kNightLightTemperature, temperature1);
  EXPECT_EQ(temperature1, controller->GetColorTemperature());
  EXPECT_TRUE(TestLayersTemperature(temperature1));
  const float temperature2 = 0.23f;
  user1_pref_service()->SetDouble(prefs::kNightLightTemperature, temperature2);
  EXPECT_EQ(temperature2, controller->GetColorTemperature());
  EXPECT_TRUE(TestLayersTemperature(temperature2));
  user1_pref_service()->SetBoolean(prefs::kNightLightEnabled, false);
  EXPECT_FALSE(controller->GetEnabled());
}

}  // namespace

}  // namespace ash
