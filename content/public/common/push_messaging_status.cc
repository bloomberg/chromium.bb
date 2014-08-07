// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/push_messaging_status.h"

#include "base/logging.h"

namespace content {

const char* PushMessagingStatusToString(PushMessagingStatus status) {
  switch (status) {
    case PUSH_MESSAGING_STATUS_OK:
      return "Operation has succeeded";

    case PUSH_MESSAGING_STATUS_REGISTRATION_FAILED_NO_SERVICE_WORKER:
      return "Registration failed - no Service Worker";

    case PUSH_MESSAGING_STATUS_REGISTRATION_FAILED_SERVICE_NOT_AVAILABLE:
      return "Registration failed - push service not available";

    case PUSH_MESSAGING_STATUS_REGISTRATION_FAILED_LIMIT_REACHED:
      return "Registration failed - registration limit has been reached";

    case PUSH_MESSAGING_STATUS_REGISTRATION_FAILED_PERMISSION_DENIED:
      return "Registration failed - permission denied";

    case PUSH_MESSAGING_STATUS_REGISTRATION_FAILED_SERVICE_ERROR:
      return "Registration failed - push service error";

    case PUSH_MESSAGING_STATUS_MESSAGE_DELIVERY_FAILED_NO_SERVICE_WORKER:
      return "Message delivery failed - no Service Worker";

    case PUSH_MESSAGING_STATUS_MESSAGE_DELIVERY_FAILED_SERVICE_WORKER_ERROR:
      return "Message delivery failed - Service Worker error";

    case PUSH_MESSAGING_STATUS_ERROR:
      return "Operation has failed (unspecified reason)";
  }
  NOTREACHED();
  return "";
}

}  // namespace content
