// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_background_services_context.h"

#include <algorithm>

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "url/origin.h"

namespace content {

namespace {

std::string CreateEntryKeyPrefix(devtools::proto::BackgroundService service) {
  DCHECK_NE(service, devtools::proto::BackgroundService::UNKNOWN);
  return "devtools_background_services_" + base::NumberToString(service) + "_";
}

std::string CreateEntryKey(devtools::proto::BackgroundService service) {
  return CreateEntryKeyPrefix(service) + base::GenerateGUID();
}

void DidLogServiceEvent(blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(rayankans): Log errors to UMA.
}

void UpdateDevToolsBackgroundServiceExpiration(
    BrowserContext* browser_context,
    devtools::proto::BackgroundService service,
    base::Time expiration_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetContentClient()->browser()->UpdateDevToolsBackgroundServiceExpiration(
      browser_context, service, expiration_time);
}

}  // namespace

DevToolsBackgroundServicesContext::DevToolsBackgroundServicesContext(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : browser_context_(browser_context),
      service_worker_context_(std::move(service_worker_context)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto expiration_times =
      GetContentClient()->browser()->GetDevToolsBackgroundServiceExpirations(
          browser_context_);

  for (const auto& expiration_time : expiration_times) {
    DCHECK(devtools::proto::BackgroundService_IsValid(expiration_time.first));
    expiration_times_.emplace(
        static_cast<devtools::proto::BackgroundService>(expiration_time.first),
        expiration_time.second);
  }
}

DevToolsBackgroundServicesContext::~DevToolsBackgroundServicesContext() =
    default;

void DevToolsBackgroundServicesContext::StartRecording(
    devtools::proto::BackgroundService service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(expiration_times_[service].is_null());

  // TODO(rayankans): Make the time delay finch configurable.
  base::Time expiration_time = base::Time::Now() + base::TimeDelta::FromDays(3);
  expiration_times_[service] = expiration_time;

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&UpdateDevToolsBackgroundServiceExpiration,
                     browser_context_, service, expiration_time));
}

void DevToolsBackgroundServicesContext::StopRecording(
    devtools::proto::BackgroundService service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!expiration_times_[service].is_null());
  expiration_times_.erase(service);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&UpdateDevToolsBackgroundServiceExpiration,
                     browser_context_, service, base::Time()));
}

void DevToolsBackgroundServicesContext::GetLoggedBackgroundServiceEvents(
    devtools::proto::BackgroundService service,
    GetLoggedBackgroundServiceEventsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->GetUserDataForAllRegistrationsByKeyPrefix(
      CreateEntryKeyPrefix(service),
      base::BindOnce(&DevToolsBackgroundServicesContext::DidGetUserData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DevToolsBackgroundServicesContext::DidGetUserData(
    GetLoggedBackgroundServiceEventsCallback callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<devtools::proto::BackgroundServiceState> service_states;

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    // TODO(rayankans): Log errors to UMA.
    std::move(callback).Run(service_states);
    return;
  }

  service_states.reserve(user_data.size());
  for (const auto& data : user_data) {
    devtools::proto::BackgroundServiceState service_state;
    if (!service_state.ParseFromString(data.second)) {
      // TODO(rayankans): Log errors to UMA.
      std::move(callback).Run({});
      return;
    }
    DCHECK_EQ(data.first, service_state.service_worker_registration_id());
    service_states.push_back(std::move(service_state));
  }

  std::sort(service_states.begin(), service_states.end(),
            [](const auto& state1, const auto& state2) {
              return state1.timestamp() < state2.timestamp();
            });
  std::move(callback).Run(std::move(service_states));
}

void DevToolsBackgroundServicesContext::LogTestBackgroundServiceEvent(
    uint64_t service_worker_registration_id,
    const url::Origin& origin,
    devtools::proto::TestBackgroundServiceEvent event) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto service = devtools::proto::BackgroundService::TEST_BACKGROUND_SERVICE;
  if (expiration_times_[service].is_null())
    return;

  devtools::proto::BackgroundServiceState service_state;
  service_state.set_background_service(service);
  *service_state.mutable_test_event() = std::move(event);

  LogBackgroundServiceState(service_worker_registration_id, origin,
                            std::move(service_state));
}

void DevToolsBackgroundServicesContext::LogBackgroundServiceState(
    uint64_t service_worker_registration_id,
    const url::Origin& origin,
    devtools::proto::BackgroundServiceState service_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!expiration_times_[service_state.background_service()].is_null());

  // Add common metadata.
  service_state.set_timestamp(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  service_state.set_origin(origin.GetURL().spec());
  service_state.set_service_worker_registration_id(
      service_worker_registration_id);

  service_worker_context_->StoreRegistrationUserData(
      service_worker_registration_id, origin.GetURL(),
      {{CreateEntryKey(service_state.background_service()),
        service_state.SerializeAsString()}},
      base::BindOnce(&DidLogServiceEvent));
}

}  // namespace content
