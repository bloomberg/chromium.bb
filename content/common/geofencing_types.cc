// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/geofencing_types.h"

#include "base/logging.h"

namespace content {

const char* GeofencingStatusToString(GeofencingStatus status) {
  switch (status) {
    case GEOFENCING_STATUS_OK:
      return "Operation has succeeded";

    case GEOFENCING_STATUS_OPERATION_FAILED_NO_SERVICE_WORKER:
      return "Operation failed - no Service Worker";

    case GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE:
      return "Operation failed - geofencing not available";

    case GEOFENCING_STATUS_UNREGISTRATION_FAILED_NOT_REGISTERED:
      return "Unregistration failed - no region registered with given ID";

    case GEOFENCING_STATUS_ERROR:
      return "Operation has failed (unspecified reason)";
  }
  NOTREACHED();
  return "";
}

}  // namespace content
