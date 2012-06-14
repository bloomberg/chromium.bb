// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/alarms/alarms_api.h"

#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/alarms/alarm_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/extensions/api/alarms.h"
#include "chrome/common/extensions/extension_error_utils.h"

namespace alarms = extensions::api::alarms;

namespace extensions {

namespace {

const char kDefaultAlarmName[] = "";
const char kAlarmNotFound[] = "No alarm named '*' exists.";
const char kDelayLessThanMinimum[] = "* is less than minimum of * minutes.";
const char kDelayIsNonInteger[] = "* is not an integer value.";
const char kBothRelativeAndAbsoluteTime[] =
    "Cannot set both when and delayInMinutes.";
const char kNoScheduledTime[] =
    "Must set at least one of when, delayInMinutes, or periodInMinutes.";
const int kReleaseDelayMinimum = 5;
const int kDevDelayMinimum = 0;

bool ValidateDelay(double delay_in_minutes,
                   const Extension* extension,
                   const std::string& delay_or_period,
                   std::string* error) {
  double delay_minimum = kDevDelayMinimum;
  if (extension->location() != Extension::LOAD) {
    // In release mode we check for integer delay values and a stricter delay
    // minimum.
    if (delay_in_minutes != static_cast<int>(delay_in_minutes)) {
      *error = ExtensionErrorUtils::FormatErrorMessage(
          kDelayIsNonInteger,
          delay_or_period);
      return false;
    }
    delay_minimum = kReleaseDelayMinimum;
  }

  // Validate against our found delay minimum.
  if (delay_in_minutes < delay_minimum) {
    *error = ExtensionErrorUtils::FormatErrorMessage(
        kDelayLessThanMinimum,
        delay_or_period,
        base::DoubleToString(delay_minimum));
    return false;
  }

  return true;
}

bool ValidateAlarmCreateInfo(const alarms::AlarmCreateInfo& create_info,
                             const Extension* extension,
                             std::string* error) {
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
    if (!ValidateDelay(*create_info.delay_in_minutes,
                       extension, "Delay", error))
      return false;
  }
  if (create_info.period_in_minutes.get()) {
    if (!ValidateDelay(*create_info.period_in_minutes,
                       extension, "Period", error))
      return false;
  }

  return true;
}

}  // namespace

AlarmsCreateFunction::AlarmsCreateFunction()
    : now_(&base::Time::Now) {
}

bool AlarmsCreateFunction::RunImpl() {
  scoped_ptr<alarms::Create::Params> params(
      alarms::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (!ValidateAlarmCreateInfo(
          params->alarm_info, GetExtension(), &error_))
    return false;

  Alarm alarm(params->name.get() ? *params->name : kDefaultAlarmName,
              params->alarm_info,
              base::TimeDelta::FromMinutes(
                  GetExtension()->location() == Extension::LOAD ?
                  kDevDelayMinimum : kReleaseDelayMinimum),
              now_);
  ExtensionSystem::Get(profile())->alarm_manager()->AddAlarm(
      extension_id(), alarm);

  return true;
}

bool AlarmsGetFunction::RunImpl() {
  scoped_ptr<alarms::Get::Params> params(
      alarms::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string name = params->name.get() ? *params->name : kDefaultAlarmName;
  const Alarm* alarm =
      ExtensionSystem::Get(profile())->alarm_manager()->GetAlarm(
          extension_id(), name);

  if (!alarm) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kAlarmNotFound, name);
    return false;
  }

  result_.reset(alarms::Get::Result::Create(*alarm->js_alarm));
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
    result_.reset(alarms::GetAll::Result::Create(create_arg));
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
