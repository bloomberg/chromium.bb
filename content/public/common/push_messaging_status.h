// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PUSH_MESSAGING_STATUS_H_
#define CONTENT_PUBLIC_COMMON_PUSH_MESSAGING_STATUS_H_

namespace content {

// Push registration success / error codes for internal use & reporting in UMA.
enum PushRegistrationStatus {
  // New successful registration (there was not yet a registration cached in
  // Service Worker storage, so the browser successfully registered with the
  // push service. This is likely to be a new push registration, though it's
  // possible that the push service had its own cache (for example if Chrome's
  // app data was cleared, we might have forgotten about a registration that the
  // push service still stores).
  PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE = 0,

  // Registration failed because there is no Service Worker.
  PUSH_REGISTRATION_STATUS_NO_SERVICE_WORKER = 1,

  // Registration failed because the push service is not available.
  PUSH_REGISTRATION_STATUS_SERVICE_NOT_AVAILABLE = 2,

  // Registration failed because the maximum number of registratons has been
  // reached.
  PUSH_REGISTRATION_STATUS_LIMIT_REACHED = 3,

  // Registration failed because permission was denied.
  PUSH_REGISTRATION_STATUS_PERMISSION_DENIED = 4,

  // Registration failed in the push service implemented by the embedder.
  PUSH_REGISTRATION_STATUS_SERVICE_ERROR = 5,

  // Registration failed because no sender id was provided by the page.
  PUSH_REGISTRATION_STATUS_NO_SENDER_ID = 6,

  // Registration succeeded, but we failed to persist it.
  PUSH_REGISTRATION_STATUS_STORAGE_ERROR = 7,

  // A successful registration was already cached in Service Worker storage.
  PUSH_REGISTRATION_STATUS_SUCCESS_FROM_CACHE = 8,

  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync, and
  // update PUSH_REGISTRATION_STATUS_LAST below.

  // Used for IPC message range checks.
  PUSH_REGISTRATION_STATUS_LAST = PUSH_REGISTRATION_STATUS_SUCCESS_FROM_CACHE
};

// Push message delivery success / error codes for internal use.
enum PushDeliveryStatus {
  // The message was successfully delivered.
  PUSH_DELIVERY_STATUS_SUCCESS,

  // The message could not be delivered because no service worker was found.
  PUSH_DELIVERY_STATUS_NO_SERVICE_WORKER,

  // The message could not be delivered because of a service worker error.
  PUSH_DELIVERY_STATUS_SERVICE_WORKER_ERROR,

  // The message was delivered, but the Service Worker passed a Promise to
  // event.waitUntil that got rejected.
  PUSH_DELIVERY_STATUS_EVENT_WAITUNTIL_REJECTED,

  // When making changes, update PUSH_DELIVERY_STATUS_LAST below.

  // Used for IPC message range checks.
  PUSH_DELIVERY_STATUS_LAST = PUSH_DELIVERY_STATUS_EVENT_WAITUNTIL_REJECTED
};

const char* PushRegistrationStatusToString(PushRegistrationStatus status);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PUSH_MESSAGING_STATUS_H_
