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
    : guid_(request.guid_), url_(request.url_), tag_(request.tag_) {}

BackgroundFetchRequestInfo::~BackgroundFetchRequestInfo() = default;

}  // namespace content
