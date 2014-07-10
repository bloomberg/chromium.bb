// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PUSH_MESSAGING_STATUS_STATUS_H_
#define CONTENT_PUBLIC_COMMON_PUSH_MESSAGING_STATUS_STATUS_H_

namespace content {

enum PushMessagingStatus {
  // Everything is ok.
  PUSH_MESSAGING_STATUS_OK,

  // Registration failed because there is no Service Worker.
  PUSH_MESSAGING_STATUS_REGISTRATION_FAILED_NO_SERVICE_WORKER,

  // Registration failed because the push service is not available.
  PUSH_MESSAGING_STATUS_REGISTRATION_FAILED_SERVICE_NOT_AVAILABLE,

  // Registration failed because the maximum number of registratons has been
  // reached.
  PUSH_MESSAGING_STATUS_REGISTRATION_FAILED_LIMIT_REACHED,

  // Registration failed because permission was denied.
  PUSH_MESSAGING_STATUS_REGISTRATION_FAILED_PERMISSION_DENIED,

  // Registration failed in the push service implemented by the embedder.
  PUSH_MESSAGING_STATUS_REGISTRATION_FAILED_SERVICE_ERROR,

  // Generic error (a more specific error should be used whenever possible).
  PUSH_MESSAGING_STATUS_ERROR,

  // Used for IPC message range checks.
  PUSH_MESSAGING_STATUS_LAST = PUSH_MESSAGING_STATUS_ERROR
};

const char* PushMessagingStatusToString(PushMessagingStatus status);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PUSH_MESSAGING_STATUS_STATUS_H_
