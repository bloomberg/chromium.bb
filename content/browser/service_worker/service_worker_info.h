// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INFO_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INFO_H_

#include <vector>

#include "content/browser/service_worker/service_worker_version.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerVersionInfo {
 public:
  ServiceWorkerVersionInfo();
  ServiceWorkerVersionInfo(ServiceWorkerVersion::RunningStatus running_status,
                           ServiceWorkerVersion::Status status,
                           int64 version_id,
                           int process_id,
                           int thread_id);
  ~ServiceWorkerVersionInfo();

  bool is_null;
  ServiceWorkerVersion::RunningStatus running_status;
  ServiceWorkerVersion::Status status;
  int64 version_id;
  int process_id;
  int thread_id;
};

class ServiceWorkerRegistrationInfo {
 public:
  ServiceWorkerRegistrationInfo(
      const GURL& script_url,
      const GURL& pattern,
      const ServiceWorkerVersionInfo& active_version,
      const ServiceWorkerVersionInfo& pending_version);
  ~ServiceWorkerRegistrationInfo();

  GURL script_url;
  GURL pattern;

  ServiceWorkerVersionInfo active_version;
  ServiceWorkerVersionInfo pending_version;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INFO_H_
