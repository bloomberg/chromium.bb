// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/power_handler.h"

#include "content/browser/power_profiler/power_profiler_service.h"

namespace content {
namespace devtools {
namespace power {

typedef DevToolsProtocolClient::Response Response;

PowerHandler::PowerHandler() : enabled_(false) {
}

PowerHandler::~PowerHandler() {
  if (enabled_)
    PowerProfilerService::GetInstance()->RemoveObserver(this);
}

void PowerHandler::SetClient(scoped_ptr<Client> client) {
  client_.swap(client);
}

void PowerHandler::OnPowerEvent(const PowerEventVector& events) {
  std::vector<scoped_refptr<PowerEvent>> event_list;
  for (const auto& event : events) {
    DCHECK(event.type < content::PowerEvent::ID_COUNT);
    // Use internal value to be consistent with Blink's
    // monotonicallyIncreasingTime.
    double timestamp = event.time.ToInternalValue() /
        static_cast<double>(base::Time::kMicrosecondsPerMillisecond);
    event_list.push_back(PowerEvent::Create()
        ->set_type(kPowerTypeNames[event.type])
        ->set_timestamp(timestamp)
        ->set_value(event.value));
  }
  client_->DataAvailable(DataAvailableParams::Create()->set_value(event_list));
}

void PowerHandler::Detached() {
  if (enabled_) {
    PowerProfilerService::GetInstance()->RemoveObserver(this);
    enabled_ = false;
  }
}

Response PowerHandler::Start() {
  if (!PowerProfilerService::GetInstance()->IsAvailable())
    return Response::InternalError("Power profiler service unavailable");
  if (!enabled_) {
    PowerProfilerService::GetInstance()->AddObserver(this);
    enabled_ = true;
  }
  return Response::OK();
}

Response PowerHandler::End() {
  if (!PowerProfilerService::GetInstance()->IsAvailable())
    return Response::InternalError("Power profiler service unavailable");
  if (enabled_) {
    PowerProfilerService::GetInstance()->RemoveObserver(this);
    enabled_ = false;
  }
  return Response::OK();
}

Response PowerHandler::CanProfilePower(bool* result) {
  *result = PowerProfilerService::GetInstance()->IsAvailable();
  return Response::OK();
}

Response PowerHandler::GetAccuracyLevel(std::string* result) {
  if (!PowerProfilerService::GetInstance()->IsAvailable())
    return Response::InternalError("Power profiler service unavailable");
  *result = PowerProfilerService::GetInstance()->GetAccuracyLevel();
  return Response::OK();
}

}  // namespace power
}  // namespace devtools
}  // namespace content
