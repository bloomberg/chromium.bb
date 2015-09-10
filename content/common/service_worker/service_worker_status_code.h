// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_STATUS_CODE_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_STATUS_CODE_H_

#include "content/common/content_export.h"

namespace content {

// Generic service worker operation statuses.
// This enum is used in UMA histograms, so don't change the order or remove
// entries.
enum ServiceWorkerStatusCode {
  // Operation succeeded.
  SERVICE_WORKER_OK,

  // Generic operation error (more specific error code should be used in
  // general).
  SERVICE_WORKER_ERROR_FAILED,

  // Operation was aborted (e.g. due to context or child process shutdown).
  SERVICE_WORKER_ERROR_ABORT,

  // Starting a new service worker script context failed.
  SERVICE_WORKER_ERROR_START_WORKER_FAILED,

  // Could not find a renderer process to run a service worker.
  SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND,

  // Generic error code to indicate the specified item is not found.
  SERVICE_WORKER_ERROR_NOT_FOUND,

  // Generic error code to indicate the specified item already exists.
  SERVICE_WORKER_ERROR_EXISTS,

  // Install event handling failed.
  SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED,

  // Activate event handling failed.
  SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED,

  // Sending an IPC to the worker failed (often due to child process is
  // terminated).
  SERVICE_WORKER_ERROR_IPC_FAILED,

  // Operation is failed by network issue.
  SERVICE_WORKER_ERROR_NETWORK,

  // Operation is failed by security issue.
  SERVICE_WORKER_ERROR_SECURITY,

  // Event handling failed (event.waitUntil Promise rejected).
  SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED,

  // An error triggered by invalid worker state.
  SERVICE_WORKER_ERROR_STATE,

  // The Service Worker took too long to finish a task.
  SERVICE_WORKER_ERROR_TIMEOUT,

  // An error occurred during initial script evaluation.
  SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED,

  // Generic error to indicate failure to read/write the disk cache.
  SERVICE_WORKER_ERROR_DISK_CACHE,

  // The worker is in REDUNDANT state.
  SERVICE_WORKER_ERROR_REDUNDANT,

  // The worker was disallowed.
  SERVICE_WORKER_ERROR_DISALLOWED,

  SERVICE_WORKER_ERROR_MAX_VALUE
};

CONTENT_EXPORT const char* ServiceWorkerStatusToString(
    ServiceWorkerStatusCode code);

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_STATUS_CODE_H_
