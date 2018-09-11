// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_update_checker.h"

#include "content/browser/service_worker/service_worker_registration.h"

namespace content {

ServiceWorkerUpdateChecker::ServiceWorkerUpdateChecker(
    scoped_refptr<ServiceWorkerRegistration> registration) {
  NOTIMPLEMENTED();
}

ServiceWorkerUpdateChecker::~ServiceWorkerUpdateChecker() = default;

void ServiceWorkerUpdateChecker::Start(UpdateStatusCallback callback) {
  std::move(callback).Run(true);
  NOTIMPLEMENTED();
}

}  // namespace content
