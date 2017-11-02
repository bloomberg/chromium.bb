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

std::string RequestKeyPrefix(const std::string& unique_id) {
  // Allows looking up all requests within a registration.
  return kRequestKeyPrefix + unique_id + kSeparator;
}

std::string PendingRequestKeyPrefix(
    int64_t registration_creation_microseconds_since_unix_epoch,
    const std::string& unique_id) {
  // These keys are ordered by the registration's creation time rather than by
  // its |unique_id|, so that the highest priority pending requests in FIFO
  // order can be looked up by fetching the lexicographically smallest keys.
  // https://crbug.com/741609 may introduce more advanced prioritisation.
  //
  // Since the ordering must survive restarts, wall clock time is used, but that
  // is not monotonically increasing, so the ordering is not exact, and the
  // |unique_id| is appended to break ties in case the wall clock returns the
  // same values more than once.
  //
  // On Nov 20 2286 17:46:39 the microseconds will transition from 9999999999999
  // to 10000000000000 and pending requests will briefly sort incorrectly.
  return kPendingRequestKeyPrefix +
         base::Int64ToString(
             registration_creation_microseconds_since_unix_epoch) +
         kSeparator + unique_id + kSeparator;
}

std::string PendingRequestKey(
    int64_t registration_creation_microseconds_since_unix_epoch,
    const std::string& unique_id,
    int request_index) {
  // In addition to the ordering from PendingRequestKeyPrefix, the requests
  // within each registration should be prioritized according to their index.
  return PendingRequestKeyPrefix(
             registration_creation_microseconds_since_unix_epoch, unique_id) +
         base::IntToString(request_index);
}

DatabaseStatus ToDatabaseStatus(ServiceWorkerStatusCode status) {
  switch (status) {
    case SERVICE_WORKER_OK:
      return DatabaseStatus::kOk;
    case SERVICE_WORKER_ERROR_FAILED:
    case SERVICE_WORKER_ERROR_ABORT:
      // FAILED is for invalid arguments (e.g. empty key) or database errors.
      // ABORT is for unexpected failures, e.g. because shutdown is in progress.
      // BackgroundFetchDataManager handles both of these the same way.
      return DatabaseStatus::kFailed;
    case SERVICE_WORKER_ERROR_NOT_FOUND:
      // This can also happen for writes, if the ServiceWorkerRegistration has
      // been deleted.
      return DatabaseStatus::kNotFound;
    case SERVICE_WORKER_ERROR_START_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND:
    case SERVICE_WORKER_ERROR_EXISTS:
    case SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED:
    case SERVICE_WORKER_ERROR_IPC_FAILED:
    case SERVICE_WORKER_ERROR_NETWORK:
    case SERVICE_WORKER_ERROR_SECURITY:
    case SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED:
    case SERVICE_WORKER_ERROR_STATE:
    case SERVICE_WORKER_ERROR_TIMEOUT:
    case SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED:
    case SERVICE_WORKER_ERROR_DISK_CACHE:
    case SERVICE_WORKER_ERROR_REDUNDANT:
    case SERVICE_WORKER_ERROR_DISALLOWED:
    case SERVICE_WORKER_ERROR_MAX_VALUE:
      break;
  }
  NOTREACHED();
  return DatabaseStatus::kFailed;
}

}  // namespace background_fetch

}  // namespace content
