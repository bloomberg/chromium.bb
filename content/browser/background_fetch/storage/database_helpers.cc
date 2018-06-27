// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/database_helpers.h"

#include "base/strings/string_number_conversions.h"

namespace content {

namespace background_fetch {

std::string ActiveRegistrationUniqueIdKey(const std::string& developer_id) {
  // Allows looking up the active registration's |unique_id| by |developer_id|.
  // Registrations are active from creation up until completed/failed/aborted.
  // These database entries correspond to the active background fetches map:
  // https://wicg.github.io/background-fetch/#service-worker-registration-active-background-fetches
  return kActiveRegistrationUniqueIdKeyPrefix + developer_id;
}

std::string RegistrationKey(const std::string& unique_id) {
  // Allows looking up a registration by |unique_id|.
  return kRegistrationKeyPrefix + unique_id;
}

std::string TitleKey(const std::string& unique_id) {
  return kTitleKeyPrefix + unique_id;
}

std::string PendingRequestKeyPrefix(const std::string& unique_id) {
  return kPendingRequestKeyPrefix + unique_id + kSeparator;
}

std::string PendingRequestKey(const std::string& unique_id, int request_index) {
  return PendingRequestKeyPrefix(unique_id) + std::to_string(request_index);
}

std::string ActiveRequestKeyPrefix(const std::string& unique_id) {
  return kActiveRequestKeyPrefix + unique_id + kSeparator;
}

std::string ActiveRequestKey(const std::string& unique_id, int request_index) {
  return ActiveRequestKeyPrefix(unique_id) + std::to_string(request_index);
}

std::string CompletedRequestKeyPrefix(const std::string& unique_id) {
  return kCompletedRequestKeyPrefix + unique_id + kSeparator;
}

std::string CompletedRequestKey(const std::string& unique_id,
                                int request_index) {
  return CompletedRequestKeyPrefix(unique_id) + std::to_string(request_index);
}

DatabaseStatus ToDatabaseStatus(blink::ServiceWorkerStatusCode status) {
  switch (status) {
    case blink::SERVICE_WORKER_OK:
      return DatabaseStatus::kOk;
    case blink::SERVICE_WORKER_ERROR_FAILED:
    case blink::SERVICE_WORKER_ERROR_ABORT:
      // FAILED is for invalid arguments (e.g. empty key) or database errors.
      // ABORT is for unexpected failures, e.g. because shutdown is in progress.
      // BackgroundFetchDataManager handles both of these the same way.
      return DatabaseStatus::kFailed;
    case blink::SERVICE_WORKER_ERROR_NOT_FOUND:
      // This can also happen for writes, if the ServiceWorkerRegistration has
      // been deleted.
      return DatabaseStatus::kNotFound;
    case blink::SERVICE_WORKER_ERROR_START_WORKER_FAILED:
    case blink::SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND:
    case blink::SERVICE_WORKER_ERROR_EXISTS:
    case blink::SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED:
    case blink::SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED:
    case blink::SERVICE_WORKER_ERROR_IPC_FAILED:
    case blink::SERVICE_WORKER_ERROR_NETWORK:
    case blink::SERVICE_WORKER_ERROR_SECURITY:
    case blink::SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED:
    case blink::SERVICE_WORKER_ERROR_STATE:
    case blink::SERVICE_WORKER_ERROR_TIMEOUT:
    case blink::SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED:
    case blink::SERVICE_WORKER_ERROR_DISK_CACHE:
    case blink::SERVICE_WORKER_ERROR_REDUNDANT:
    case blink::SERVICE_WORKER_ERROR_DISALLOWED:
    case blink::SERVICE_WORKER_ERROR_MAX_VALUE:
      break;
  }
  NOTREACHED();
  return DatabaseStatus::kFailed;
}

}  // namespace background_fetch

}  // namespace content
