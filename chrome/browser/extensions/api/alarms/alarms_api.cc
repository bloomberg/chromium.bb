// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/alarms/alarms_api.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/alarms/alarm_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/extensions/api/alarms.h"
#include "extensions/common/error_utils.h"

namespace alarms = extensions::api::alarms;

namespace extensions {

namespace {

const char kDefaultAlarmName[] = "";
const char kAlarmNotFound[] = "No alarm named '*' exists.";
const char kBothRelativeAndAbsoluteTime[] =
    "Cannot set both when and delayInMinutes.";
const char kNoScheduledTime[] =
    "Must set at least one of when, delayInMinutes, or periodInMinutes.";
const int kReleaseDelayMinimum = 1;
const int kDevDelayMinimum = 0;

bool ValidateAlarmCreateInfo(const std::string& alarm_name,
                             const alarms::AlarmCreateInfo& create_info,
                             const Extension* extension,
                             std::string* error,
                             std::vector<std::string>* warnings) {
  if (create_info.delay_in_minutes.get() &&
      create_info.when.get()) {
    *error = kBothRelativeAndAbsoluteTime;
    return false;
  }
  if (create_info.delay_in_minutes == NULL &&
      create_info.when == NULL &&
      create_info.period_in_minutes == NULL) {
    *error = kNoScheduledTime;
    return false;
  }

  // Users can always use an absolute timeout to request an arbitrarily-short or
  // negative delay.  We won't honor the short timeout, but we can't check it
  // and warn the user because it would introduce race conditions (say they
  // compute a long-enough timeout, but then the call into the alarms interface
  // gets delayed past the boundary).  However, it's still worth warning about
  // relative delays that are shorter than we'll honor.
  if (create_info.delay_in_minutes.get()) {
    if (*create_info.delay_in_minutes < kReleaseDelayMinimum) {
      COMPILE_ASSERT(kReleaseDelayMinimum == 1, update_warning_message_below);
      if (extension->location() == Manifest::LOAD)
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            "Alarm delay is less than minimum of 1 minutes."
            " In released .crx, alarm \"*\" will fire in approximately"
            " 1 minutes.",
            alarm_name));
      else
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            "Alarm delay is less than minimum of 1 minutes."
            " Alarm \"*\" will fire in approximately 1 minutes.",
            alarm_name));
    }
  }
  if (create_info.period_in_minutes.get()) {
    if (*create_info.period_in_minutes < kReleaseDelayMinimum) {
      COMPILE_ASSERT(kReleaseDelayMinimum == 1, update_warning_message_below);
      if (extension->location() == Manifest::LOAD)
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            "Alarm period is less than minimum of 1 minutes."
            " In released .crx, alarm \"*\" will fire approximately"
            " every 1 minutes.",
            alarm_name));
      else
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            "Alarm period is less than minimum of 1 minutes."
            " Alarm \"*\" will fire approximately every 1 minutes.",
            alarm_name));
    }
  }

  return true;
}

}  // namespace

AlarmsCreateFunction::AlarmsCreateFunction()
    : clock_(new base::DefaultClock()), owns_clock_(true) {}

AlarmsCreateFunction::AlarmsCreateFunction(base::Clock* clock)
    : clock_(clock), owns_clock_(false) {}

AlarmsCreateFunction::~AlarmsCreateFunction() {
  if (owns_clock_)
    delete clock_;
}

bool AlarmsCreateFunction::RunImpl() {
  scoped_ptr<alarms::Create::Params> params(
      alarms::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const std::string& alarm_name =
      params->name.get() ? *params->name : kDefaultAlarmName;
  std::vector<std::string> warnings;
  if (!ValidateAlarmCreateInfo(
          alarm_name, params->alarm_info, GetExtension(), &error_, &warnings))
    return false;
  for (std::vector<std::string>::const_iterator it = warnings.begin();
       it != warnings.end(); ++it)
    WriteToConsole(content::CONSOLE_MESSAGE_LEVEL_WARNING, *it);

  Alarm alarm(alarm_name,
              params->alarm_info,
              base::TimeDelta::FromMinutes(
                  GetExtension()->location() == Manifest::LOAD ?
                  kDevDelayMinimum : kReleaseDelayMinimum),
              clock_->Now());
  ExtensionSystem::Get(profile())->alarm_manager()->AddAlarm(
      extension_id(), alarm);

  return true;
}

bool AlarmsGetFunction::RunImpl() {
  scoped_ptr<alarms::Get::Params> params(alarms::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string name = params->name.get() ? *params->name : kDefaultAlarmName;
  const Alarm* alarm =
      ExtensionSystem::Get(profile())->alarm_manager()->GetAlarm(
          extension_id(), name);

  if (!alarm) {
    error_ = ErrorUtils::FormatErrorMessage(kAlarmNotFound, name);
    return false;
  }

  results_ = alarms::Get::Results::Create(*alarm->js_alarm);
  return true;
}

bool AlarmsGetAllFunction::RunImpl() {
  const AlarmManager::AlarmList* alarms =
      ExtensionSystem::Get(profile())->alarm_manager()->GetAllAlarms(
          extension_id());
  if (alarms) {
    std::vector<linked_ptr<extensions::api::alarms::Alarm> > create_arg;
    create_arg.reserve(alarms->size());
    for (size_t i = 0, size = alarms->size(); i < size; ++i) {
      create_arg.push_back((*alarms)[i].js_alarm);
    }
    results_ = alarms::GetAll::Results::Create(create_arg);
  } else {
    SetResult(new base::ListValue());
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
    error_ = ErrorUtils::FormatErrorMessage(kAlarmNotFound, name);
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
