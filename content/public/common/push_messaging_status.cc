// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/push_messaging_status.h"

#include "base/logging.h"

namespace content {

const char* PushRegistrationStatusToString(PushRegistrationStatus status) {
  switch (status) {
    case PUSH_REGISTRATION_STATUS_SUCCESS:
      return "Registration successful";

    case PUSH_REGISTRATION_STATUS_NO_SERVICE_WORKER:
      return "Registration failed - no Service Worker";

    case PUSH_REGISTRATION_STATUS_SERVICE_NOT_AVAILABLE:
      return "Registration failed - push service not available";

    case PUSH_REGISTRATION_STATUS_LIMIT_REACHED:
      return "Registration failed - registration limit has been reached";

    case PUSH_REGISTRATION_STATUS_PERMISSION_DENIED:
      return "Registration failed - permission denied";

    case PUSH_REGISTRATION_STATUS_SERVICE_ERROR:
      return "Registration failed - push service error";

    case PUSH_REGISTRATION_STATUS_NO_SENDER_ID:
      return "Registration failed - no sender id provided";
  }
  NOTREACHED();
  return "";
}

}  // namespace content
