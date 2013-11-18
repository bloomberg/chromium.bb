// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include "content/browser/service_worker/service_worker_registration.h"

namespace content {

ServiceWorkerVersion::ServiceWorkerVersion(
    ServiceWorkerRegistration* registration)
    : is_shutdown_(false), registration_(registration) {}

ServiceWorkerVersion::~ServiceWorkerVersion() { DCHECK(is_shutdown_); }

void ServiceWorkerVersion::Shutdown() {
  is_shutdown_ = true;
  registration_ = NULL;
}

}  // namespace content
