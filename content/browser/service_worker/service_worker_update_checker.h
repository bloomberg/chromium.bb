// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_

#include "base/callback.h"

namespace content {

class ServiceWorkerRegistration;

class ServiceWorkerUpdateChecker {
 public:
  using UpdateStatusCallback = base::OnceCallback<void(bool)>;

  ServiceWorkerUpdateChecker(
      scoped_refptr<ServiceWorkerRegistration> registration);
  ~ServiceWorkerUpdateChecker();

  // |callback| is always triggered when Start() finishes. If the scripts are
  // found to have any changes, the argument of |callback| is true and otherwise
  // false.
  void Start(UpdateStatusCallback callback);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_
