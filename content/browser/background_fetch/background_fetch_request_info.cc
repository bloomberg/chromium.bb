// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_request_info.h"

#include <string>

#include "content/public/browser/download_item.h"

namespace content {

BackgroundFetchRequestInfo::BackgroundFetchRequestInfo(
    int request_index,
    const ServiceWorkerFetchRequest& fetch_request)
    : request_index_(request_index), fetch_request_(fetch_request) {}

BackgroundFetchRequestInfo::~BackgroundFetchRequestInfo() {}

void BackgroundFetchRequestInfo::PopulateDownloadState(
    DownloadItem* download_item,
    DownloadInterruptReason download_interrupt_reason) {
  DCHECK(!download_state_populated_);

  download_guid_ = download_item->GetGuid();
  download_state_ = download_item->GetState();

  download_state_populated_ = true;
}

void BackgroundFetchRequestInfo::PopulateResponseFromDownloadItem(
    DownloadItem* download_item) {
  DCHECK(!response_data_populated_);

  url_chain_ = download_item->GetUrlChain();
  file_path_ = download_item->GetTargetFilePath();
  file_size_ = download_item->GetReceivedBytes();
  response_time_ = download_item->GetEndTime();

  response_data_populated_ = true;
}

const std::vector<GURL>& BackgroundFetchRequestInfo::GetURLChain() const {
  DCHECK(response_data_populated_);
  return url_chain_;
}

const base::FilePath& BackgroundFetchRequestInfo::GetFilePath() const {
  DCHECK(response_data_populated_);
  return file_path_;
}

int64_t BackgroundFetchRequestInfo::GetFileSize() const {
  DCHECK(response_data_populated_);
  return file_size_;
}

const base::Time& BackgroundFetchRequestInfo::GetResponseTime() const {
  DCHECK(response_data_populated_);
  return response_time_;
}

}  // namespace content
