// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PUSH_MESSAGING_STATUS_H_
#define CONTENT_PUBLIC_COMMON_PUSH_MESSAGING_STATUS_H_

namespace content {

enum PushRegistrationStatus {
  // Registration was successful.
  PUSH_REGISTRATION_STATUS_SUCCESS,

  // Registration failed because there is no Service Worker.
  PUSH_REGISTRATION_STATUS_NO_SERVICE_WORKER,

  // Registration failed because the push service is not available.
  PUSH_REGISTRATION_STATUS_SERVICE_NOT_AVAILABLE,

  // Registration failed because the maximum number of registratons has been
  // reached.
  PUSH_REGISTRATION_STATUS_LIMIT_REACHED,

  // Registration failed because permission was denied.
  PUSH_REGISTRATION_STATUS_PERMISSION_DENIED,

  // Registration failed in the push service implemented by the embedder.
  PUSH_REGISTRATION_STATUS_SERVICE_ERROR,

  // Used for IPC message range checks.
  PUSH_REGISTRATION_STATUS_LAST = PUSH_REGISTRATION_STATUS_SERVICE_ERROR
};

enum PushDeliveryStatus {
  // The message was successfully delivered.
  PUSH_DELIVERY_STATUS_SUCCESS,

  // The message could not be delivered because no service worker was found.
  PUSH_DELIVERY_STATUS_NO_SERVICE_WORKER,

  // The message could not be delivered because of a service worker error.
  PUSH_DELIVERY_STATUS_SERVICE_WORKER_ERROR,

  // Used for IPC message range checks.
  PUSH_DELIVERY_STATUS_LAST = PUSH_DELIVERY_STATUS_SERVICE_WORKER_ERROR
};

const char* PushRegistrationStatusToString(PushRegistrationStatus status);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PUSH_MESSAGING_STATUS_H_
