// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_info.h"

#include "base/guid.h"

namespace content {

BackgroundFetchJobInfo::BackgroundFetchJobInfo(
    const std::string& tag,
    const url::Origin& origin,
    int64_t service_worker_registration_id)
    : guid_(base::GenerateGUID()),
      tag_(tag),
      origin_(origin),
      service_worker_registration_id_(service_worker_registration_id) {}

BackgroundFetchJobInfo::BackgroundFetchJobInfo() = default;

BackgroundFetchJobInfo::BackgroundFetchJobInfo(
    const BackgroundFetchJobInfo& other)
    : guid_(other.guid_),
      tag_(other.tag_),
      origin_(other.origin_),
      service_worker_registration_id_(other.service_worker_registration_id_),
      request_guids_(other.request_guids_) {}

BackgroundFetchJobInfo::~BackgroundFetchJobInfo() = default;

}  // namespace content
