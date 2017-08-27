// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration_status.h"

#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_error_type.mojom.h"

namespace content {

using blink::WebServiceWorkerError;

void GetServiceWorkerRegistrationStatusResponse(
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    blink::mojom::ServiceWorkerErrorType* error_type,
    base::string16* message) {
  *error_type = blink::mojom::ServiceWorkerErrorType::kUnknown;
  if (!status_message.empty())
    *message = base::UTF8ToUTF16(status_message);
  else
    *message = base::ASCIIToUTF16(ServiceWorkerStatusToString(status));
  switch (status) {
    case SERVICE_WORKER_OK:
      NOTREACHED() << "Calling this when status == OK is not allowed";
      return;

    case SERVICE_WORKER_ERROR_START_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND:
    case SERVICE_WORKER_ERROR_REDUNDANT:
    case SERVICE_WORKER_ERROR_DISALLOWED:
      *error_type = blink::mojom::ServiceWorkerErrorType::kInstall;
      return;

    case SERVICE_WORKER_ERROR_NOT_FOUND:
      *error_type = blink::mojom::ServiceWorkerErrorType::kNotFound;
      return;

    case SERVICE_WORKER_ERROR_NETWORK:
      *error_type = blink::mojom::ServiceWorkerErrorType::kNetwork;
      return;

    case SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED:
      *error_type = blink::mojom::ServiceWorkerErrorType::kScriptEvaluateFailed;
      return;

    case SERVICE_WORKER_ERROR_SECURITY:
      *error_type = blink::mojom::ServiceWorkerErrorType::kSecurity;
      return;

    case SERVICE_WORKER_ERROR_TIMEOUT:
      *error_type = blink::mojom::ServiceWorkerErrorType::kTimeout;
      return;

    case SERVICE_WORKER_ERROR_ABORT:
      *error_type = blink::mojom::ServiceWorkerErrorType::kAbort;
      return;

    case SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_IPC_FAILED:
    case SERVICE_WORKER_ERROR_FAILED:
    case SERVICE_WORKER_ERROR_EXISTS:
    case SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED:
    case SERVICE_WORKER_ERROR_STATE:
    case SERVICE_WORKER_ERROR_DISK_CACHE:
    case SERVICE_WORKER_ERROR_MAX_VALUE:
      // Unexpected, or should have bailed out before calling this, or we don't
      // have a corresponding blink error code yet.
      break;  // Fall through to NOTREACHED().
  }
  NOTREACHED() << "Got unexpected error code: "
               << status << " " << ServiceWorkerStatusToString(status);
}

}  // namespace content
