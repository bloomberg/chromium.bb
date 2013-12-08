// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_STATUS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_STATUS_H_

namespace content {

// This enum is used to describe the final state of a ServiceWorkerRegistration.
enum ServiceWorkerRegistrationStatus {
  REGISTRATION_OK,
  REGISTRATION_NOT_FOUND,
  REGISTRATION_INSTALL_FAILED,
  REGISTRATION_ACTIVATE_FAILED,
  REGISTRATION_FAILED,
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_STATUS_H_
