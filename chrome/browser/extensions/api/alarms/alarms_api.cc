// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/alarms/alarms_api.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/alarms/alarm_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/extensions/api/experimental_alarms.h"
#include "chrome/common/extensions/extension_error_utils.h"

namespace alarms = extensions::api::experimental_alarms;

namespace extensions {

namespace {

const char kDefaultAlarmName[] = "";
const char kAlarmNotFound[] = "No alarm named '*' exists.";

}

bool AlarmsCreateFunction::RunImpl() {
  scoped_ptr<alarms::Create::Params> params(
      alarms::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  linked_ptr<AlarmManager::Alarm> alarm(new AlarmManager::Alarm());
  alarm->name = params->name.get() ? *params->name : kDefaultAlarmName;
  alarm->delay_in_seconds = params->alarm_info.delay_in_seconds;
  alarm->repeating = params->alarm_info.repeating.get() ?
      *params->alarm_info.repeating : false;
  ExtensionSystem::Get(profile())->alarm_manager()->AddAlarm(
      extension_id(), alarm);

  return true;
}

bool AlarmsGetFunction::RunImpl() {
  scoped_ptr<alarms::Get::Params> params(
      alarms::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string name = params->name.get() ? *params->name : kDefaultAlarmName;
  const AlarmManager::Alarm* alarm =
      ExtensionSystem::Get(profile())->alarm_manager()->GetAlarm(
          extension_id(), name);

  if (!alarm) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kAlarmNotFound, name);
    return false;
  }

  result_.reset(alarms::Get::Result::Create(*alarm));
  return true;
}

bool AlarmsGetAllFunction::RunImpl() {
  const AlarmManager::AlarmList* alarms =
      ExtensionSystem::Get(profile())->alarm_manager()->GetAllAlarms(
          extension_id());
  if (alarms) {
    result_.reset(alarms::GetAll::Result::Create(*alarms));
  } else {
    result_.reset(new base::ListValue());
  }
  return true;
}

bool AlarmsClearFunction::RunImpl() {
  scoped_ptr<alarms::Clear::Params> params(
      alarms::Clear::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string name = params->name.get() ? *params->name : kDefaultAlarmName;
  bool success = ExtensionSystem::Get(profile())->alarm_manager()->RemoveAlarm(
     extension_id(), name);

  if (!success) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kAlarmNotFound, name);
    return false;
  }

  return true;
}

bool AlarmsClearAllFunction::RunImpl() {
  ExtensionSystem::Get(profile())->alarm_manager()->RemoveAllAlarms(
     extension_id());
  return true;
}

}  // namespace extensions
