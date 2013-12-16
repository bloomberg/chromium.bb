// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration_status.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

namespace {
const char kInstallFailedErrorMessage[] = "ServiceWorker failed to install";
const char kActivateFailedErrorMessage[] = "ServiceWorker failed to activate";
}

namespace content {

using blink::WebServiceWorkerError;

void GetServiceWorkerRegistrationStatusResponse(
    ServiceWorkerRegistrationStatus status,
    blink::WebServiceWorkerError::ErrorType* error_type,
    base::string16* message) {
  switch (status) {
    case REGISTRATION_OK:
      NOTREACHED() << "Consumers should check registration status before "
                      "calling this function.";
      return;

    case REGISTRATION_INSTALL_FAILED:
      *error_type = WebServiceWorkerError::InstallError;
      *message = ASCIIToUTF16(kInstallFailedErrorMessage);
      return;

    case REGISTRATION_ACTIVATE_FAILED:
      *error_type = WebServiceWorkerError::ActivateError;
      *message = ASCIIToUTF16(kActivateFailedErrorMessage);
      return;
  }
  NOTREACHED();
}
}  // namespace content
