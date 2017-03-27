// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_request_info.h"

#include <string>

#include "base/guid.h"
#include "content/public/browser/download_item.h"

namespace content {

BackgroundFetchRequestInfo::BackgroundFetchRequestInfo() = default;

BackgroundFetchRequestInfo::BackgroundFetchRequestInfo(const GURL& url,
                                                       const std::string& tag)
    : guid_(base::GenerateGUID()), url_(url), tag_(tag) {}

BackgroundFetchRequestInfo::BackgroundFetchRequestInfo(
    const BackgroundFetchRequestInfo& request)
    : guid_(request.guid_),
      url_(request.url_),
      tag_(request.tag_),
      download_guid_(request.download_guid_),
      state_(request.state_),
      interrupt_reason_(request.interrupt_reason_),
      file_path_(request.file_path_) {}

BackgroundFetchRequestInfo::~BackgroundFetchRequestInfo() {}

bool BackgroundFetchRequestInfo::IsComplete() const {
  return (state_ == DownloadItem::DownloadState::COMPLETE ||
          state_ == DownloadItem::DownloadState::CANCELLED);
}

}  // namespace content
