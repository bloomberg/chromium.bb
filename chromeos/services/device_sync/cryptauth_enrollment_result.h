// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_RESULT_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_RESULT_H_

#include "base/optional.h"
#include "chromeos/services/device_sync/network_request_error.h"
#include "chromeos/services/device_sync/proto/cryptauth_directive.pb.h"

namespace chromeos {

namespace device_sync {

// Information about the result of a CryptAuth v2 Enrollment attempt.
class CryptAuthEnrollmentResult {
 public:
  // Enum class to denote the result of a CryptAuth v2 Enrollment attempt
  enum class ResultCode {
    // Successfully synced but no new keys were requested by CryptAuth, so no
    // EnrollKeysRequest was made.
    kSuccessNoNewKeysNeeded,
    // Successfully synced and enrolled new key(s) with CryptAuth.
    kSuccessNewKeysEnrolled,
    // Request could not be completed because the device is offline or has
    // issues sending the HTTP request.
    kNetworkRequestErrorOffline,
    // Server endpoint could not be found.
    kNetworkRequestErrorEndpointNotFound,
    // Authentication error contacting back-end.
    kNetworkRequestErrorAuthenticationError,
    // Network request was invalid.
    kNetworkRequestErrorBadRequest,
    // The server responded, but the response was not formatted correctly.
    kNetworkRequestErrorResponseMalformed,
    // Internal server error.
    kNetworkRequestErrorInternalServerError,
    // Unknown network request error.
    kNetworkRequestErrorUnknown,
    // The CryptAuth server indicated via SyncKeysResponse::server_status that
    // it was overloaded and did not process the SyncKeysRequest.
    kErrorCryptAuthServerOverloaded,
    // The data sent in the SyncKeysResponse is not self-consistent or is not
    // consistent with the SyncKeysRequest data.
    kErrorSyncKeysResponseMalformed,
  };

  static ResultCode NetworkRequestErrorToResultCode(NetworkRequestError error);

  CryptAuthEnrollmentResult(
      ResultCode result_code,
      const base::Optional<cryptauthv2::ClientDirective>& client_directive);
  CryptAuthEnrollmentResult(const CryptAuthEnrollmentResult& other);

  ~CryptAuthEnrollmentResult();

  ResultCode result_code() const { return result_code_; }

  const base::Optional<cryptauthv2::ClientDirective>& client_directive() const {
    return client_directive_;
  };

  bool IsSuccess() const;

 private:
  ResultCode result_code_;
  base::Optional<cryptauthv2::ClientDirective> client_directive_;
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_RESULT_H_
