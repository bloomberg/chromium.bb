// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_provider_host_info.h"

namespace content {

ServiceWorkerProviderHostInfo::ServiceWorkerProviderHostInfo() {}

ServiceWorkerProviderHostInfo::ServiceWorkerProviderHostInfo(
    ServiceWorkerProviderHostInfo&& other)
    : provider_id(other.provider_id),
      route_id(other.route_id),
      type(other.type),
      is_parent_frame_secure(other.is_parent_frame_secure) {}

ServiceWorkerProviderHostInfo::ServiceWorkerProviderHostInfo(
    int provider_id,
    int route_id,
    ServiceWorkerProviderType type,
    bool is_parent_frame_secure)
    : provider_id(provider_id),
      route_id(route_id),
      type(type),
      is_parent_frame_secure(is_parent_frame_secure) {}

ServiceWorkerProviderHostInfo::~ServiceWorkerProviderHostInfo() {}

}  // namespace content
