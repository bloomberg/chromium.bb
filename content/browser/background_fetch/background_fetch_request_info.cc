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

bool BackgroundFetchRequestInfo::IsComplete() const {
  return (state_ == DownloadItem::DownloadState::COMPLETE ||
          state_ == DownloadItem::DownloadState::CANCELLED);
}

const GURL& BackgroundFetchRequestInfo::GetURL() const {
  return fetch_request_.url;
}

}  // namespace content
