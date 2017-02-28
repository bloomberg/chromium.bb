// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_data.h"

#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_job_info.h"

namespace content {

BackgroundFetchJobData::BackgroundFetchJobData(
    BackgroundFetchRequestInfos request_infos)
    : request_infos_(std::move(request_infos)) {}

BackgroundFetchJobData::~BackgroundFetchJobData() {}

// TODO(harkness): Provide more detail about status and where the returned data
// is now available.
bool BackgroundFetchJobData::BackgroundFetchRequestInfoComplete(
    const std::string& fetch_guid) {
  // Make sure that the request was expected to be in-progress.
  auto index_iter = request_info_index_.find(fetch_guid);
  DCHECK(index_iter != request_info_index_.end());
  DCHECK_EQ(fetch_guid, request_infos_[index_iter->second].guid());

  // Set the request as complete and delete it from the in-progress index.
  request_infos_[index_iter->second].set_complete(true);
  request_info_index_.erase(index_iter);

  // Return a boolean indicating whether there are more requests to be
  // processed.
  return next_request_info_ != request_infos_.size();
}

const BackgroundFetchRequestInfo&
BackgroundFetchJobData::GetNextBackgroundFetchRequestInfo() {
  DCHECK(next_request_info_ != request_infos_.size());

  const BackgroundFetchRequestInfo& next_request =
      request_infos_[next_request_info_];
  DCHECK(!next_request.complete());
  request_info_index_[next_request.guid()] = next_request_info_++;

  return next_request;
}

bool BackgroundFetchJobData::IsComplete() const {
  return ((next_request_info_ == request_infos_.size()) &&
          request_info_index_.empty());
}

bool BackgroundFetchJobData::HasRequestsRemaining() const {
  return next_request_info_ != request_infos_.size();
}

}  // namespace content
