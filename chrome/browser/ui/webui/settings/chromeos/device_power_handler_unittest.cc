// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_power_handler.h"

#include <memory>

#include "ash/system/power/power_status.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/values.h"
#include "chrome/browser/chromeos/power/power_prefs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

class TestPowerHandler : public PowerHandler {
 public:
  explicit TestPowerHandler(PrefService* prefs) : PowerHandler(prefs) {}

  // Pull WebUIMessageHandler::set_web_ui() into public so tests can call it.
  using PowerHandler::set_web_ui;
};

class PowerHandlerTest : public testing::Test {
 public:
  PowerHandlerTest() {
    // This initializes chromeos::DBusThreadManager.
    std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
        chromeos::DBusThreadManager::GetSetterForTesting();
    dbus_setter->SetPowerManagerClient(
        base::MakeUnique<chromeos::FakePowerManagerClient>());
    power_manager_client_ = static_cast<chromeos::FakePowerManagerClient*>(
        chromeos::DBusThreadManager::Get()->GetPowerManagerClient());
    ash::PowerStatus::Initialize();

    chromeos::PowerPrefs::RegisterUserProfilePrefs(prefs_.registry());

    handler_ = base::MakeUnique<TestPowerHandler>(&prefs_);
    test_api_ = base::MakeUnique<PowerHandler::TestAPI>(handler_.get());
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
    base::RunLoop().RunUntilIdle();
  }

  ~PowerHandlerTest() override {
    handler_.reset();
    ash::PowerStatus::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  // Returns a JSON representation of the contents of the last message sent to
  // WebUI about settings being changed.
  std::string GetLastSettingsChangedMessage() WARN_UNUSED_RESULT {
    for (auto it = web_ui_.call_data().rbegin();
         it != web_ui_.call_data().rend(); ++it) {
      const content::TestWebUI::CallData* data = it->get();
      std::string name;
      const base::DictionaryValue* dict = nullptr;
      data->arg1()->GetAsString(&name);
      if (data->function_name() != "cr.webUIListenerCallback" ||
          !data->arg1()->GetAsString(&name) ||
          name != PowerHandler::kPowerManagementSettingsChangedName) {
        continue;
      }
      if (!data->arg2()->GetAsDictionary(&dict)) {
        ADD_FAILURE() << "Failed to get dict from " << name << " message";
        continue;
      }
      std::string out;
      EXPECT_TRUE(base::JSONWriter::Write(*dict, &out));
      return out;
    }

    ADD_FAILURE() << PowerHandler::kPowerManagementSettingsChangedName
                  << " message was not sent";
    return std::string();
  }

  // Returns a string for the given settings that can be compared against the
  // output of GetLastSettingsChangedMessage().
  std::string CreateSettingsChangedString(
      PowerHandler::IdleBehavior idle_behavior,
      bool idle_controlled,
      PowerPolicyController::Action lid_closed_behavior,
      bool lid_closed_controlled,
      bool has_lid) {
    base::DictionaryValue dict;
    dict.SetInteger(PowerHandler::kIdleBehaviorKey,
                    static_cast<int>(idle_behavior));
    dict.SetBoolean(PowerHandler::kIdleControlledKey, idle_controlled);
    dict.SetInteger(PowerHandler::kLidClosedBehaviorKey, lid_closed_behavior);
    dict.SetBoolean(PowerHandler::kLidClosedControlledKey,
                    lid_closed_controlled);
    dict.SetBoolean(PowerHandler::kHasLidKey, has_lid);

    std::string out;
    EXPECT_TRUE(base::JSONWriter::Write(dict, &out));
    return out;
  }

  // Returns the user-set value of the integer pref identified by |name| or -1
  // if the pref is unset.
  int GetIntPref(const std::string& name) {
    const base::Value* value = prefs_.GetUserPref(name);
    return value ? value->GetInt() : -1;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  content::TestWebUI web_ui_;

  // Owned by chromeos::DBusThreadManager.
  chromeos::FakePowerManagerClient* power_manager_client_;

  std::unique_ptr<TestPowerHandler> handler_;
  std::unique_ptr<TestPowerHandler::TestAPI> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerHandlerTest);
};

// Verifies that settings are sent to WebUI when requested.
TEST_F(PowerHandlerTest, SendInitialSettings) {
  test_api_->RequestPowerManagementSettings();
  EXPECT_EQ(
      CreateSettingsChangedString(
          PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
          false /* idle_controlled */, PowerPolicyController::ACTION_SUSPEND,
          false /* lid_closed_controlled */, true /* has_lid */),
      GetLastSettingsChangedMessage());
}

// Verifies that WebUI receives updated settings when the lid state changes.
TEST_F(PowerHandlerTest, SendSettingsForLidStateChanges) {
  power_manager_client_->SetLidState(PowerManagerClient::LidState::NOT_PRESENT,
                                     base::TimeTicks());
  EXPECT_EQ(
      CreateSettingsChangedString(
          PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
          false /* idle_controlled */, PowerPolicyController::ACTION_SUSPEND,
          false /* lid_closed_controlled */, false /* has_lid */),
      GetLastSettingsChangedMessage());

  power_manager_client_->SetLidState(PowerManagerClient::LidState::OPEN,
                                     base::TimeTicks());
  EXPECT_EQ(
      CreateSettingsChangedString(
          PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
          false /* idle_controlled */, PowerPolicyController::ACTION_SUSPEND,
          false /* lid_closed_controlled */, true /* has_lid */),
      GetLastSettingsChangedMessage());
}

// Verifies that when various prefs are controlled, the corresponding settings
// are reported as controlled to WebUI.
TEST_F(PowerHandlerTest, SendSettingsForControlledPrefs) {
  // Making an arbitrary delay pref managed should result in the idle setting
  // being reported as controlled.
  prefs_.SetManagedPref(prefs::kPowerAcScreenDimDelayMs,
                        base::MakeUnique<base::Value>(10000));
  EXPECT_EQ(
      CreateSettingsChangedString(
          PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
          true /* idle_controlled */, PowerPolicyController::ACTION_SUSPEND,
          false /* lid_closed_controlled */, true /* has_lid */),
      GetLastSettingsChangedMessage());

  // Ditto for making the lid action pref managed.
  prefs_.SetManagedPref(
      prefs::kPowerLidClosedAction,
      base::MakeUnique<base::Value>(PowerPolicyController::ACTION_SUSPEND));
  EXPECT_EQ(
      CreateSettingsChangedString(
          PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
          true /* idle_controlled */, PowerPolicyController::ACTION_SUSPEND,
          true /* lid_closed_controlled */, true /* has_lid */),
      GetLastSettingsChangedMessage());
}

// Verifies that idle-related prefs are distilled into the proper WebUI
// settings.
TEST_F(PowerHandlerTest, SendIdleSettingForPrefChanges) {
  // Set a do-nothing idle action and a nonzero screen-off delay.
  prefs_.SetUserPref(
      prefs::kPowerAcIdleAction,
      base::MakeUnique<base::Value>(PowerPolicyController::ACTION_DO_NOTHING));
  prefs_.SetUserPref(prefs::kPowerAcScreenOffDelayMs,
                     base::MakeUnique<base::Value>(10000));
  EXPECT_EQ(
      CreateSettingsChangedString(
          PowerHandler::IdleBehavior::DISPLAY_OFF_STAY_AWAKE,
          false /* idle_controlled */, PowerPolicyController::ACTION_SUSPEND,
          false /* lid_closed_controlled */, true /* has_lid */),
      GetLastSettingsChangedMessage());

  // Now set the delay to zero and check that the setting goes to "display on".
  prefs_.SetUserPref(prefs::kPowerAcScreenOffDelayMs,
                     base::MakeUnique<base::Value>(0));
  EXPECT_EQ(CreateSettingsChangedString(PowerHandler::IdleBehavior::DISPLAY_ON,
                                        false /* idle_controlled */,
                                        PowerPolicyController::ACTION_SUSPEND,
                                        false /* lid_closed_controlled */,
                                        true /* has_lid */),
            GetLastSettingsChangedMessage());

  // Other idle actions should result in an "other" setting.
  prefs_.SetUserPref(prefs::kPowerAcIdleAction,
                     base::MakeUnique<base::Value>(
                         PowerPolicyController::ACTION_STOP_SESSION));
  EXPECT_EQ(CreateSettingsChangedString(
                PowerHandler::IdleBehavior::OTHER, false /* idle_controlled */,
                PowerPolicyController::ACTION_SUSPEND,
                false /* lid_closed_controlled */, true /* has_lid */),
            GetLastSettingsChangedMessage());
}

// Verifies that the lid-closed pref's value is sent directly to WebUI.
TEST_F(PowerHandlerTest, SendLidSettingForPrefChanges) {
  prefs_.SetUserPref(
      prefs::kPowerLidClosedAction,
      base::MakeUnique<base::Value>(PowerPolicyController::ACTION_SHUT_DOWN));
  EXPECT_EQ(
      CreateSettingsChangedString(
          PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
          false /* idle_controlled */, PowerPolicyController::ACTION_SHUT_DOWN,
          false /* lid_closed_controlled */, true /* has_lid */),
      GetLastSettingsChangedMessage());

  prefs_.SetUserPref(prefs::kPowerLidClosedAction,
                     base::MakeUnique<base::Value>(
                         PowerPolicyController::ACTION_STOP_SESSION));
  EXPECT_EQ(CreateSettingsChangedString(
                PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP,
                false /* idle_controlled */,
                PowerPolicyController::ACTION_STOP_SESSION,
                false /* lid_closed_controlled */, true /* has_lid */),
            GetLastSettingsChangedMessage());
}

// Verifies that requests from WebUI to update the idle behavior update prefs
// appropriately.
TEST_F(PowerHandlerTest, SetIdleBehavior) {
  // Request the "display on" setting and check that prefs are set
  // appropriately.
  test_api_->SetIdleBehavior(PowerHandler::IdleBehavior::DISPLAY_ON);
  EXPECT_EQ(PowerPolicyController::ACTION_DO_NOTHING,
            GetIntPref(prefs::kPowerAcIdleAction));
  EXPECT_EQ(0, GetIntPref(prefs::kPowerAcScreenDimDelayMs));
  EXPECT_EQ(0, GetIntPref(prefs::kPowerAcScreenOffDelayMs));
  EXPECT_EQ(0, GetIntPref(prefs::kPowerAcScreenLockDelayMs));
  EXPECT_EQ(PowerPolicyController::ACTION_DO_NOTHING,
            GetIntPref(prefs::kPowerBatteryIdleAction));
  EXPECT_EQ(0, GetIntPref(prefs::kPowerBatteryScreenDimDelayMs));
  EXPECT_EQ(0, GetIntPref(prefs::kPowerBatteryScreenOffDelayMs));
  EXPECT_EQ(0, GetIntPref(prefs::kPowerBatteryScreenLockDelayMs));

  // "Turn the display off but stay awake" should set the idle prefs but clear
  // the screen delays.
  test_api_->SetIdleBehavior(
      PowerHandler::IdleBehavior::DISPLAY_OFF_STAY_AWAKE);
  EXPECT_EQ(PowerPolicyController::ACTION_DO_NOTHING,
            GetIntPref(prefs::kPowerAcIdleAction));
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerAcScreenDimDelayMs));
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerAcScreenOffDelayMs));
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerAcScreenLockDelayMs));
  EXPECT_EQ(PowerPolicyController::ACTION_DO_NOTHING,
            GetIntPref(prefs::kPowerBatteryIdleAction));
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerBatteryScreenDimDelayMs));
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerBatteryScreenOffDelayMs));
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerBatteryScreenLockDelayMs));

  // Now switch to the "display on" setting (to set the prefs again) and check
  // that the "sleep" setting clears all the prefs.
  test_api_->SetIdleBehavior(PowerHandler::IdleBehavior::DISPLAY_ON);
  test_api_->SetIdleBehavior(PowerHandler::IdleBehavior::DISPLAY_OFF_SLEEP);
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerAcIdleAction));
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerAcScreenDimDelayMs));
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerAcScreenOffDelayMs));
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerAcScreenLockDelayMs));
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerBatteryIdleAction));
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerBatteryScreenDimDelayMs));
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerBatteryScreenOffDelayMs));
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerBatteryScreenLockDelayMs));
}

// Verifies that requests from WebUI to change the lid behavior update the pref.
TEST_F(PowerHandlerTest, SetLidBehavior) {
  // The "do nothing" setting should update the pref.
  test_api_->SetLidClosedBehavior(PowerPolicyController::ACTION_DO_NOTHING);
  EXPECT_EQ(PowerPolicyController::ACTION_DO_NOTHING,
            GetIntPref(prefs::kPowerLidClosedAction));

  // Selecting the "suspend" setting should just clear the pref.
  test_api_->SetLidClosedBehavior(PowerPolicyController::ACTION_SUSPEND);
  EXPECT_EQ(-1, GetIntPref(prefs::kPowerLidClosedAction));
}

}  // namespace settings
}  // namespace chromeos
