// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_registration_id.h"

#include "content/common/service_worker/service_worker_types.h"

namespace content {

BackgroundFetchRegistrationId::BackgroundFetchRegistrationId()
    : service_worker_registration_id_(kInvalidServiceWorkerRegistrationId) {}

BackgroundFetchRegistrationId::BackgroundFetchRegistrationId(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& tag)
    : service_worker_registration_id_(service_worker_registration_id),
      origin_(origin),
      tag_(tag) {}

BackgroundFetchRegistrationId::BackgroundFetchRegistrationId(
    BackgroundFetchRegistrationId&& other) = default;

BackgroundFetchRegistrationId::~BackgroundFetchRegistrationId() = default;

BackgroundFetchRegistrationId& BackgroundFetchRegistrationId::operator=(
    const BackgroundFetchRegistrationId& other) = default;

bool BackgroundFetchRegistrationId::operator==(
    const BackgroundFetchRegistrationId& other) const {
  return other.service_worker_registration_id_ ==
             service_worker_registration_id_ &&
         other.origin_ == origin_ && other.tag_ == tag_;
}

bool BackgroundFetchRegistrationId::operator!=(
    const BackgroundFetchRegistrationId& other) const {
  return !(*this == other);
}

bool BackgroundFetchRegistrationId::operator<(
    const BackgroundFetchRegistrationId& other) const {
  return service_worker_registration_id_ <
             other.service_worker_registration_id_ ||
         origin_ < other.origin_ || tag_ < other.tag_;
}

bool BackgroundFetchRegistrationId::is_null() const {
  return service_worker_registration_id_ == kInvalidServiceWorkerRegistrationId;
}

}  // namespace content
