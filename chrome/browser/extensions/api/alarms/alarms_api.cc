// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/alarms/alarms_api.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/experimental.alarms.h"

namespace Alarms = extensions::api::experimental_alarms;

namespace {

const char kOnAlarmEvent[] = "experimental.alarms.onAlarm";
const char kDefaultAlarmName[] = "";

void AlarmCallback(Profile* profile, const std::string& extension_id) {
  // The profile could have gone away before this task was run.
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;

  ListValue args;
  std::string json_args;
  args.Append(base::Value::CreateStringValue(kDefaultAlarmName));
  base::JSONWriter::Write(&args, &json_args);
  profile->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id, kOnAlarmEvent, json_args, NULL, GURL());
}

}

namespace extensions {

bool AlarmsCreateFunction::RunImpl() {
  scoped_ptr<Alarms::Create::Params> params(
      Alarms::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(mpcomplete): Better handling of granularity. Introduce an alarm
  // manager that dispatches alarms in batches, and can also cancel previous
  // alarms. Handle the "name" parameter.
  // http://crbug.com/81758
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&AlarmCallback, profile(), extension_id()),
      base::TimeDelta::FromSeconds(params->alarm_info.delay_in_seconds));

  return true;
}

bool AlarmsGetFunction::RunImpl() {
  error_ = "Not implemented.";
  return false;
}

bool AlarmsGetAllFunction::RunImpl() {
  error_ = "Not implemented.";
  return false;
}

bool AlarmsClearFunction::RunImpl() {
  error_ = "Not implemented.";
  return false;
}

bool AlarmsClearAllFunction::RunImpl() {
  error_ = "Not implemented.";
  return false;
}

}  // namespace extensions
