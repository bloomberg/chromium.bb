// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_job_info.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"

namespace content {

BackgroundFetchDataManager::BackgroundFetchDataManager(
    BackgroundFetchContext* background_fetch_context)
    : background_fetch_context_(background_fetch_context) {
  DCHECK(background_fetch_context_);
  // TODO(harkness) Read from persistent storage and recreate requests.
}

BackgroundFetchDataManager::~BackgroundFetchDataManager() = default;

BackgroundFetchJobData* BackgroundFetchDataManager::CreateRequest(
    const BackgroundFetchJobInfo& job_info,
    BackgroundFetchRequestInfos request_infos) {
  JobIdentifier id(job_info.service_worker_registration_id(), job_info.tag());
  // Ensure that this is not a duplicate request.
  if (service_worker_tag_map_.find(id) != service_worker_tag_map_.end()) {
    DVLOG(1) << "Origin " << job_info.origin()
             << " has already created a batch request with tag "
             << job_info.tag();
    // TODO(harkness) Figure out how to return errors like this.
    return nullptr;
  }
  if (batch_map_.find(job_info.guid()) != batch_map_.end()) {
    DVLOG(1) << "Job with UID " << job_info.guid() << " already exists.";
    // TODO(harkness) Figure out how to return errors like this.
    return nullptr;
  }

  // Add the request to our maps and return a JobData to track the individual
  // files in the request.
  service_worker_tag_map_[id] = job_info.guid();
  // TODO(harkness): When a job is complete, remove the JobData from the map.
  batch_map_[job_info.guid()] =
      base::MakeUnique<BackgroundFetchJobData>(std::move(request_infos));
  return batch_map_[job_info.guid()].get();
}

}  // namespace content
