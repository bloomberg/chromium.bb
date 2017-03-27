// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_info.h"

#include "base/guid.h"

#include "content/browser/background_fetch/background_fetch_job_response_data.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"

namespace content {

BackgroundFetchJobInfo::BackgroundFetchJobInfo(
    const std::string& tag,
    const url::Origin& origin,
    int64_t service_worker_registration_id)
    : guid_(base::GenerateGUID()),
      tag_(tag),
      origin_(origin),
      service_worker_registration_id_(service_worker_registration_id) {}

BackgroundFetchJobInfo::~BackgroundFetchJobInfo() = default;

void BackgroundFetchJobInfo::set_job_response_data(
    std::unique_ptr<BackgroundFetchJobResponseData> job_response_data) {
  job_response_data_ = std::move(job_response_data);
}

bool BackgroundFetchJobInfo::IsComplete() const {
  return !HasRequestsRemaining() && active_requests_.empty();
}

bool BackgroundFetchJobInfo::HasRequestsRemaining() const {
  return next_request_index_ != num_requests_;
}

void BackgroundFetchJobInfo::AddActiveRequest(
    std::unique_ptr<BackgroundFetchRequestInfo> request) {
  active_requests_.insert(std::move(request));
  next_request_index_++;
}

BackgroundFetchRequestInfo* BackgroundFetchJobInfo::GetActiveRequest(
    const std::string& request_guid) const {
  for (auto& request : active_requests_) {
    if (request->guid() == request_guid)
      return request.get();
  }
  return nullptr;
}

void BackgroundFetchJobInfo::RemoveActiveRequest(
    const std::string& request_guid) {
  // active_requests_.erase(request_guid);
  for (const auto& request : active_requests_) {
    if (request->guid() == request_guid) {
      active_requests_.erase(request);
      return;
    }
  }
}

}  // namespace content
