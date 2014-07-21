// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_power_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/browser/power_profiler/power_profiler_service.h"

namespace content {

DevToolsPowerHandler::DevToolsPowerHandler()
    : is_profiling_(false) {
  RegisterCommandHandler(devtools::Power::start::kName,
                         base::Bind(&DevToolsPowerHandler::OnStart,
                                    base::Unretained(this)));
  RegisterCommandHandler(devtools::Power::end::kName,
                         base::Bind(&DevToolsPowerHandler::OnEnd,
                                    base::Unretained(this)));
  RegisterCommandHandler(devtools::Power::canProfilePower::kName,
                         base::Bind(&DevToolsPowerHandler::OnCanProfilePower,
                                    base::Unretained(this)));
  RegisterCommandHandler(devtools::Power::getAccuracyLevel::kName,
                         base::Bind(&DevToolsPowerHandler::OnGetAccuracyLevel,
                                    base::Unretained(this)));
}

DevToolsPowerHandler::~DevToolsPowerHandler() {
  if (is_profiling_)
    PowerProfilerService::GetInstance()->RemoveObserver(this);
}

void DevToolsPowerHandler::OnPowerEvent(const PowerEventVector& events) {
  base::DictionaryValue* params = new base::DictionaryValue();
  base::ListValue* event_list = new base::ListValue();

  std::vector<PowerEvent>::const_iterator iter;
  for (iter = events.begin(); iter != events.end(); ++iter) {
    base::DictionaryValue* event_body = new base::DictionaryValue();

    DCHECK(iter->type < PowerEvent::ID_COUNT);
    event_body->SetString("type", kPowerTypeNames[iter->type]);
    // Use internal value to be consistent with Blink's
    // monotonicallyIncreasingTime.
    event_body->SetDouble("timestamp", iter->time.ToInternalValue() /
        static_cast<double>(base::Time::kMicrosecondsPerMillisecond));
    event_body->SetDouble("value", iter->value);
    event_list->Append(event_body);
  }

  params->Set(devtools::Power::dataAvailable::kParamValue, event_list);
  SendNotification(devtools::Power::dataAvailable::kName, params);
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsPowerHandler::OnStart(
    scoped_refptr<DevToolsProtocol::Command> command) {
  if (PowerProfilerService::GetInstance()->IsAvailable()) {
    PowerProfilerService::GetInstance()->AddObserver(this);
    is_profiling_ = true;
    return command->SuccessResponse(NULL);
  }

  return command->InternalErrorResponse("Power profiler service unavailable");
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsPowerHandler::OnEnd(scoped_refptr<DevToolsProtocol::Command> command) {
  if (PowerProfilerService::GetInstance()->IsAvailable()) {
    PowerProfilerService::GetInstance()->RemoveObserver(this);
    is_profiling_ = false;
    return command->SuccessResponse(NULL);
  }

  return command->InternalErrorResponse("Power profiler service unavailable");
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsPowerHandler::OnCanProfilePower(
    scoped_refptr<DevToolsProtocol::Command> command) {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetBoolean(devtools::kResult,
                     PowerProfilerService::GetInstance()->IsAvailable());

  return command->SuccessResponse(result);
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsPowerHandler::OnGetAccuracyLevel(
    scoped_refptr<DevToolsProtocol::Command> command) {
  if (PowerProfilerService::GetInstance()->IsAvailable()) {
    base::DictionaryValue* result = new base::DictionaryValue();
    result->SetString(
        devtools::kResult,
        PowerProfilerService::GetInstance()->GetAccuracyLevel());
    return command->SuccessResponse(result);
  }
  return command->InternalErrorResponse("Power profiler service unavailable");
}

void DevToolsPowerHandler::OnClientDetached() {
  if (is_profiling_)
    PowerProfilerService::GetInstance()->RemoveObserver(this);
}

}  // namespace content
