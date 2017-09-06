// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_STATUS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_STATUS_H_

#include "base/strings/string16.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"

namespace content {

// TODO(leonhsl): Eliminate this function once all its users changed to use
// GetServiceWorkerErrorTypeForRegistration() which gets out a std::string
// |message| rather than a base::string16. The legacy IPC used base::string16 as
// an old WebKit convention. This should only be called for errors, where status
// != OK.
void GetServiceWorkerRegistrationStatusResponse(
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    blink::mojom::ServiceWorkerErrorType* error_type,
    base::string16* message);

// This should only be called for errors, where status != OK.
void GetServiceWorkerErrorTypeForRegistration(
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    blink::mojom::ServiceWorkerErrorType* out_error,
    std::string* out_message);

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_STATUS_H_
