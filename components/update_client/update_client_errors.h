// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_ERRORS_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_ERRORS_H_

namespace update_client {

// Errors generated as a result of calling UpdateClient member functions.
// These errors are not sent in pings.
enum class Error {
  INVALID_ARGUMENT = -1,
  NONE = 0,
  UPDATE_IN_PROGRESS = 1,
  UPDATE_CANCELED = 2,
  RETRY_LATER = 3,
  SERVICE_ERROR = 4,
  UPDATE_CHECK_ERROR = 5,
};

// These errors are sent in pings. Add new values only to the bottom of
// the enums below; the order must be kept stable.
enum class ErrorCategory {
  kErrorNone = 0,
  kNetworkError,
  kUnpackError,
  kInstallError,
  kServiceError,  // Runtime errors which occur in the service itself.
};

// These errors are returned with the |kNetworkError| error category. This
// category could include other errors such as the errors defined by
// the Chrome net stack.
enum class CrxDownloaderError {
  NONE = 0,
  NO_URL = 10,
  NO_HASH = 11,
  BAD_HASH = 12,  // The downloaded file fails the hash verification.
  GENERIC_ERROR = -1
};

// These errors are returned with the |kUnpackError| error category and
// indicate unpacker, patcher, or install errors.
enum class UnpackerError {
  kNone = 0,
  kInvalidParams,
  kInvalidFile,
  kUnzipPathError,
  kUnzipFailed,
  kNoManifest,
  kBadManifest,
  kBadExtension,
  kInvalidId,
  kInstallerError,
  kIoError,
  kDeltaVerificationFailure,
  kDeltaBadCommands,
  kDeltaUnsupportedCommand,
  kDeltaOperationFailure,
  kDeltaPatchProcessFailure,
  kDeltaMissingExistingFile,
  kFingerprintWriteFailed,
};

// These errors are returned with the |kServiceError| error category and
// indicate critical or configuration errors in the update service.
enum class ServiceError {
  NONE = 0,
  SERVICE_WAIT_FAILED = 1,
  UPDATE_DISABLED = 2,
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_ERRORS_H_
