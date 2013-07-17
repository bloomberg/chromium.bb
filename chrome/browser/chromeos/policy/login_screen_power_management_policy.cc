// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/login_screen_power_management_policy.h"

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_handler.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"

namespace policy {

namespace {
  const char kScreenDimDelayAC[] = "AC.Delays.ScreenDim";
  const char kScreenOffDelayAC[] = "AC.Delays.ScreenOff";
  const char kIdleDelayAC[] = "AC.Delays.Idle";
  const char kScreenDimDelayBattery[] = "Battery.Delays.ScreenDim";
  const char kScreenOffDelayBattery[] = "Battery.Delays.ScreenOff";
  const char kIdleDelayBattery[] = "Battery.Delays.Idle";
  const char kIdleActionAC[] = "AC.IdleAction";
  const char kIdleActionBattery[] = "Battery.IdleAction";
  const char kLidCloseAction[] = "LidCloseAction";
  const char kUserActivityScreenDimDelayScale[] =
      "UserActivityScreenDimDelayScale";

  const char kActionSuspend[]   = "Suspend";
  const char kActionShutdown[]  = "Shutdown";
  const char kActionDoNothing[] = "DoNothing";

scoped_ptr<base::Value> GetValue(const base::DictionaryValue* dict,
                                 const std::string& key,
                                 base::Value::Type type,
                                 PolicyErrorMap* errors) {
  const base::Value* value;
  if (!dict->Get(key, &value))
    return scoped_ptr<base::Value>();

  if (!value->IsType(type)) {
    if (errors) {
      errors->AddError(key::kDeviceLoginScreenPowerManagement,
                       key,
                       IDS_POLICY_TYPE_ERROR,
                       ConfigurationPolicyHandler::ValueTypeToString(type));
    }
    return scoped_ptr<base::Value>();
  }

  return scoped_ptr<base::Value>(value->DeepCopy());
}

scoped_ptr<base::Value> GetInteger(const base::DictionaryValue* dict,
                                   const std::string& key,
                                   int minimum,
                                   PolicyErrorMap* errors) {
  scoped_ptr<base::Value> value =
      GetValue(dict, key, base::Value::TYPE_INTEGER, errors);
  int integer;
  if (!value || !value->GetAsInteger(&integer) || integer >= minimum)
    return value.Pass();

  if (errors) {
    errors->AddError(key::kDeviceLoginScreenPowerManagement,
                     key,
                     IDS_POLICY_OUT_OF_RANGE_ERROR,
                     base::IntToString(integer));
  }
  return scoped_ptr<base::Value>();
}

scoped_ptr<base::Value> GetAction(const base::DictionaryValue* dict,
                                  const std::string& key,
                                  PolicyErrorMap* errors) {
  scoped_ptr<base::Value> value =
      GetValue(dict, key, base::Value::TYPE_STRING, errors);
  std::string action;
  if (!value || !value->GetAsString(&action))
    return value.Pass();

  if (action == kActionSuspend) {
    return scoped_ptr<base::Value>(new base::FundamentalValue(
        chromeos::PowerPolicyController::ACTION_SUSPEND));
  }
  if (action == kActionShutdown) {
    return scoped_ptr<base::Value>(new base::FundamentalValue(
        chromeos::PowerPolicyController::ACTION_SHUT_DOWN));
  }
  if (action == kActionDoNothing) {
    return scoped_ptr<base::Value>(new base::FundamentalValue(
        chromeos::PowerPolicyController::ACTION_DO_NOTHING));
  }

  if (errors) {
    errors->AddError(key::kDeviceLoginScreenPowerManagement,
                     key,
                     IDS_POLICY_OUT_OF_RANGE_ERROR,
                     action);
  }
  return scoped_ptr<base::Value>();
}

}  // namespace

LoginScreenPowerManagementPolicy::LoginScreenPowerManagementPolicy() {
}

LoginScreenPowerManagementPolicy::~LoginScreenPowerManagementPolicy() {
}

bool LoginScreenPowerManagementPolicy::Init(const std::string& json,
                                            PolicyErrorMap* errors) {
  std::string error;
  scoped_ptr<base::Value> root(base::JSONReader::ReadAndReturnError(
      json, base::JSON_ALLOW_TRAILING_COMMAS, NULL, &error));
  base::DictionaryValue* dict = NULL;
  if (!root || !root->GetAsDictionary(&dict)) {
    if (errors) {
      errors->AddError(key::kDeviceLoginScreenPowerManagement,
                       IDS_POLICY_JSON_PARSE_ERROR,
                       error);
    }
    // Errors in JSON decoding are fatal.
    return false;
  }

  screen_dim_delay_ac_ = GetInteger(dict, kScreenDimDelayAC, 0, errors);
  screen_off_delay_ac_ = GetInteger(dict, kScreenOffDelayAC, 0, errors);
  idle_delay_ac_ = GetInteger(dict, kIdleDelayAC, 0, errors);
  screen_dim_delay_battery_ =
      GetInteger(dict, kScreenDimDelayBattery, 0, errors);
  screen_off_delay_battery_ =
      GetInteger(dict, kScreenOffDelayBattery, 0, errors);
  idle_delay_battery_ = GetInteger(dict, kIdleDelayBattery, 0, errors);
  idle_action_ac_ = GetAction(dict, kIdleActionAC, errors);
  idle_action_battery_ = GetAction(dict, kIdleActionBattery, errors);
  lid_close_action_ = GetAction(dict, kLidCloseAction, errors);
  user_activity_screen_dim_delay_scale_ =
      GetInteger(dict, kUserActivityScreenDimDelayScale, 100, errors);

  // Validation errors for individual power policies are non-fatal as other
  // power policies that pass validation will still be applied.
  return true;
}

const base::Value*
    LoginScreenPowerManagementPolicy::GetScreenDimDelayAC() const {
  return screen_dim_delay_ac_.get();
}

const base::Value*
    LoginScreenPowerManagementPolicy::GetScreenOffDelayAC() const {
  return screen_off_delay_ac_.get();
}

const base::Value* LoginScreenPowerManagementPolicy::GetIdleDelayAC() const {
  return idle_delay_ac_.get();
}

const base::Value*
    LoginScreenPowerManagementPolicy::GetScreenDimDelayBattery() const {
  return screen_dim_delay_battery_.get();
}

const base::Value*
    LoginScreenPowerManagementPolicy::GetScreenOffDelayBattery() const {
  return screen_off_delay_battery_.get();
}

const base::Value*
    LoginScreenPowerManagementPolicy::GetIdleDelayBattery() const {
  return idle_delay_battery_.get();
}

const base::Value* LoginScreenPowerManagementPolicy::GetIdleActionAC() const {
  return idle_action_ac_.get();
}

const base::Value*
    LoginScreenPowerManagementPolicy::GetIdleActionBattery() const {
  return idle_action_battery_.get();
}

const base::Value* LoginScreenPowerManagementPolicy::GetLidCloseAction() const {
  return lid_close_action_.get();
}

const base::Value* LoginScreenPowerManagementPolicy::
    GetUserActivityScreenDimDelayScale() const {
  return user_activity_screen_dim_delay_scale_.get();
}

} // namespace policy
