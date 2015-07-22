// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_type_converters.h"

#include "base/logging.h"

namespace mojo {

// TODO(iclelland): Make these enums equivalent so that conversion can be a
// static cast.
content::ServiceWorkerStatusCode
TypeConverter<content::ServiceWorkerStatusCode,
              content::ServiceWorkerEventStatus>::
    Convert(content::ServiceWorkerEventStatus status) {
  content::ServiceWorkerStatusCode status_code;
  if (status == content::SERVICE_WORKER_EVENT_STATUS_COMPLETED) {
    status_code = content::SERVICE_WORKER_OK;
  } else if (status == content::SERVICE_WORKER_EVENT_STATUS_REJECTED) {
    status_code = content::SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED;
  } else if (status == content::SERVICE_WORKER_EVENT_STATUS_ABORTED) {
    status_code = content::SERVICE_WORKER_ERROR_ABORT;
  } else {
    // We received an unexpected value back. This can theoretically happen as
    // mojo doesn't validate enum values.
    status_code = content::SERVICE_WORKER_ERROR_IPC_FAILED;
  }
  return status_code;
}

}  // namespace
