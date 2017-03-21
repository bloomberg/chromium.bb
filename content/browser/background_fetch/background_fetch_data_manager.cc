// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"

namespace content {

BackgroundFetchDataManager::BackgroundFetchDataManager(
    BackgroundFetchContext* background_fetch_context)
    : background_fetch_context_(background_fetch_context) {
  DCHECK(background_fetch_context_);
  // TODO(harkness) Read from persistent storage and recreate requests.
}

BackgroundFetchDataManager::~BackgroundFetchDataManager() = default;

std::unique_ptr<BackgroundFetchJobData>
BackgroundFetchDataManager::CreateRequest(
    std::unique_ptr<BackgroundFetchJobInfo> job_info,
    BackgroundFetchRequestInfos request_infos) {
  JobIdentifier id(job_info->service_worker_registration_id(), job_info->tag());
  // Ensure that this is not a duplicate request.
  if (service_worker_tag_map_.find(id) != service_worker_tag_map_.end()) {
    DVLOG(1) << "Origin " << job_info->origin()
             << " has already created a batch request with tag "
             << job_info->tag();
    // TODO(harkness) Figure out how to return errors like this.
    return nullptr;
  }

  // Add the request to our maps and return a JobData to track the individual
  // files in the request.
  const std::string job_guid = job_info->guid();
  service_worker_tag_map_[id] = job_guid;
  WriteJobToStorage(std::move(job_info), std::move(request_infos));
  // TODO(harkness): Remove data when the job is complete.

  return base::MakeUnique<BackgroundFetchJobData>(
      ReadRequestsFromStorage(job_guid));
}

void BackgroundFetchDataManager::WriteJobToStorage(
    std::unique_ptr<BackgroundFetchJobInfo> job_info,
    BackgroundFetchRequestInfos request_infos) {
  // TODO(harkness): Replace these maps with actually writing to storage.
  // TODO(harkness): Check for job_guid clash.
  const std::string job_guid = job_info->guid();
  job_map_[job_guid] = std::move(job_info);
  request_map_[job_guid] = std::move(request_infos);
}

// TODO(harkness): This should be changed to read (and cache) small numbers of
// the RequestInfos instead of returning all of them.
BackgroundFetchRequestInfos&
BackgroundFetchDataManager::ReadRequestsFromStorage(
    const std::string& job_guid) {
  return request_map_[job_guid];
}

}  // namespace content
