// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/fetch_request.h"

#include <string>

#include "base/guid.h"
#include "content/public/browser/download_item.h"

namespace content {

FetchRequest::FetchRequest() = default;

FetchRequest::FetchRequest(const url::Origin& origin,
                           const GURL& url,
                           int64_t service_worker_registration_id,
                           const std::string& tag)
    : guid_(base::GenerateGUID()),
      origin_(origin),
      url_(url),
      service_worker_registration_id_(service_worker_registration_id),
      tag_(tag) {}

FetchRequest::FetchRequest(const FetchRequest& request)
    : guid_(request.guid_),
      origin_(request.origin_),
      url_(request.url_),
      service_worker_registration_id_(request.service_worker_registration_id()),
      tag_(request.tag_) {}

FetchRequest::~FetchRequest() = default;

}  // namespace content
