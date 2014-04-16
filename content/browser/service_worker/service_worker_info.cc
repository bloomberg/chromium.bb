// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_info.h"

#include "content/common/service_worker/service_worker_types.h"

namespace content {

ServiceWorkerVersionInfo::ServiceWorkerVersionInfo()
    : is_null(true),
      running_status(ServiceWorkerVersion::STOPPED),
      status(ServiceWorkerVersion::NEW),
      version_id(kInvalidServiceWorkerVersionId),
      process_id(-1),
      thread_id(-1) {}

ServiceWorkerVersionInfo::ServiceWorkerVersionInfo(
    ServiceWorkerVersion::RunningStatus running_status,
    ServiceWorkerVersion::Status status,
    int64 version_id,
    int process_id,
    int thread_id)
    : is_null(false),
      running_status(running_status),
      status(status),
      version_id(version_id),
      process_id(process_id),
      thread_id(thread_id) {}
ServiceWorkerVersionInfo::~ServiceWorkerVersionInfo() {}

ServiceWorkerRegistrationInfo::ServiceWorkerRegistrationInfo(
    const GURL& script_url,
    const GURL& pattern,
    const ServiceWorkerVersionInfo& active_version,
    const ServiceWorkerVersionInfo& pending_version)
    : script_url(script_url),
      pattern(pattern),
      active_version(active_version),
      pending_version(pending_version) {}

ServiceWorkerRegistrationInfo::~ServiceWorkerRegistrationInfo() {}

}  // namespace content
