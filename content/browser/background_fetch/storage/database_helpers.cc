// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/database_helpers.h"

#include "base/strings/string_number_conversions.h"
#include "content/browser/background_fetch/background_fetch.pb.h"

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

std::string UIOptionsKey(const std::string& unique_id) {
  return kUIOptionsKeyPrefix + unique_id;
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
    case blink::ServiceWorkerStatusCode::kOk:
      return DatabaseStatus::kOk;
    case blink::ServiceWorkerStatusCode::kErrorFailed:
    case blink::ServiceWorkerStatusCode::kErrorAbort:
      // FAILED is for invalid arguments (e.g. empty key) or database errors.
      // ABORT is for unexpected failures, e.g. because shutdown is in progress.
      // BackgroundFetchDataManager handles both of these the same way.
      return DatabaseStatus::kFailed;
    case blink::ServiceWorkerStatusCode::kErrorNotFound:
      // This can also happen for writes, if the ServiceWorkerRegistration has
      // been deleted.
      return DatabaseStatus::kNotFound;
    case blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorProcessNotFound:
    case blink::ServiceWorkerStatusCode::kErrorExists:
    case blink::ServiceWorkerStatusCode::kErrorInstallWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorActivateWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorIpcFailed:
    case blink::ServiceWorkerStatusCode::kErrorNetwork:
    case blink::ServiceWorkerStatusCode::kErrorSecurity:
    case blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected:
    case blink::ServiceWorkerStatusCode::kErrorState:
    case blink::ServiceWorkerStatusCode::kErrorTimeout:
    case blink::ServiceWorkerStatusCode::kErrorScriptEvaluateFailed:
    case blink::ServiceWorkerStatusCode::kErrorDiskCache:
    case blink::ServiceWorkerStatusCode::kErrorRedundant:
    case blink::ServiceWorkerStatusCode::kErrorDisallowed:
      break;
  }
  NOTREACHED();
  return DatabaseStatus::kFailed;
}

BackgroundFetchRegistration ToBackgroundFetchRegistration(
    const proto::BackgroundFetchMetadata& metadata_proto) {
  const auto& registration_proto = metadata_proto.registration();

  BackgroundFetchRegistration registration;
  registration.developer_id = registration_proto.developer_id();
  registration.unique_id = registration_proto.unique_id();
  registration.upload_total = registration_proto.upload_total();
  registration.uploaded = registration_proto.uploaded();
  registration.download_total = registration_proto.download_total();
  registration.downloaded = registration_proto.downloaded();

  return registration;
}

}  // namespace background_fetch

}  // namespace content
