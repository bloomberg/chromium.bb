// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_request_info.h"

#include <string>

#include "content/public/browser/download_item.h"

namespace content {

BackgroundFetchRequestInfo::BackgroundFetchRequestInfo() = default;

BackgroundFetchRequestInfo::BackgroundFetchRequestInfo(
    int request_index,
    const ServiceWorkerFetchRequest& fetch_request)
    : request_index_(request_index), fetch_request_(fetch_request) {}

BackgroundFetchRequestInfo::BackgroundFetchRequestInfo(
    const BackgroundFetchRequestInfo& request)
    : request_index_(request.request_index_),
      fetch_request_(request.fetch_request_),
      download_guid_(request.download_guid_),
      state_(request.state_),
      interrupt_reason_(request.interrupt_reason_),
      file_path_(request.file_path_),
      received_bytes_(request.received_bytes_) {}

BackgroundFetchRequestInfo::~BackgroundFetchRequestInfo() {}

bool BackgroundFetchRequestInfo::IsComplete() const {
  return (state_ == DownloadItem::DownloadState::COMPLETE ||
          state_ == DownloadItem::DownloadState::CANCELLED);
}

const GURL& BackgroundFetchRequestInfo::GetURL() const {
  return fetch_request_.url;
}

}  // namespace content
