// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_data.h"

#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_job_info.h"

namespace content {

BackgroundFetchJobData::BackgroundFetchJobData(
    BackgroundFetchRequestInfos& request_infos)
    : request_infos_(request_infos) {}

BackgroundFetchJobData::~BackgroundFetchJobData() {}

bool BackgroundFetchJobData::UpdateBackgroundFetchRequestState(
    const std::string& fetch_guid,
    DownloadItem::DownloadState state,
    DownloadInterruptReason interrupt_reason) {
  // Make sure that the request was expected to be in-progress.
  auto index_iter = request_info_index_.find(fetch_guid);
  DCHECK(index_iter != request_info_index_.end());
  DCHECK_EQ(fetch_guid, request_infos_[index_iter->second].guid());

  // Set the state of the request and the interrupt reason.
  request_infos_[index_iter->second].set_state(state);
  request_infos_[index_iter->second].set_interrupt_reason(interrupt_reason);

  // If the new state is complete or cancelled, remove the in-progress request.
  switch (state) {
    case DownloadItem::DownloadState::COMPLETE:
    case DownloadItem::DownloadState::CANCELLED:
      request_info_index_.erase(index_iter);
    case DownloadItem::DownloadState::IN_PROGRESS:
    case DownloadItem::DownloadState::INTERRUPTED:
    case DownloadItem::DownloadState::MAX_DOWNLOAD_STATE:
      break;
  }

  // Return a boolean indicating whether there are more requests to be
  // processed.
  return next_request_info_ != request_infos_.size();
}

const BackgroundFetchRequestInfo&
BackgroundFetchJobData::GetNextBackgroundFetchRequestInfo() {
  DCHECK(next_request_info_ != request_infos_.size());

  const BackgroundFetchRequestInfo& next_request =
      request_infos_[next_request_info_];
  DCHECK_EQ(next_request.state(),
            DownloadItem::DownloadState::MAX_DOWNLOAD_STATE);
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

void BackgroundFetchJobData::SetRequestDownloadGuid(
    const std::string& request_guid,
    const std::string& download_guid) {
  auto index_iter = request_info_index_.find(request_guid);
  DCHECK(index_iter != request_info_index_.end());
  request_infos_[index_iter->second].set_download_guid(download_guid);
}
}  // namespace content
