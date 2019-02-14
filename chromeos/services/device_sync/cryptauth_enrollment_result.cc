// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_enrollment_result.h"

namespace chromeos {

namespace device_sync {

// static
CryptAuthEnrollmentResult::ResultCode
CryptAuthEnrollmentResult::NetworkRequestErrorToResultCode(
    NetworkRequestError error) {
  switch (error) {
    case NetworkRequestError::kOffline:
      return CryptAuthEnrollmentResult::ResultCode::kNetworkRequestErrorOffline;
    case NetworkRequestError::kEndpointNotFound:
      return CryptAuthEnrollmentResult::ResultCode::
          kNetworkRequestErrorEndpointNotFound;
    case NetworkRequestError::kAuthenticationError:
      return CryptAuthEnrollmentResult::ResultCode::
          kNetworkRequestErrorAuthenticationError;
    case NetworkRequestError::kBadRequest:
      return CryptAuthEnrollmentResult::ResultCode::
          kNetworkRequestErrorBadRequest;
    case NetworkRequestError::kResponseMalformed:
      return CryptAuthEnrollmentResult::ResultCode::
          kNetworkRequestErrorResponseMalformed;
    case NetworkRequestError::kInternalServerError:
      return CryptAuthEnrollmentResult::ResultCode::
          kNetworkRequestErrorInternalServerError;
    case NetworkRequestError::kUnknown:
      return CryptAuthEnrollmentResult::ResultCode::kNetworkRequestErrorUnknown;
  }
}

CryptAuthEnrollmentResult::CryptAuthEnrollmentResult(
    ResultCode result_code,
    const base::Optional<cryptauthv2::ClientDirective>& client_directive)
    : result_code_(result_code), client_directive_(client_directive) {}

CryptAuthEnrollmentResult::CryptAuthEnrollmentResult(
    const CryptAuthEnrollmentResult& other) = default;

CryptAuthEnrollmentResult::~CryptAuthEnrollmentResult() = default;

bool CryptAuthEnrollmentResult::IsSuccess() const {
  return result_code_ == ResultCode::kSuccessNewKeysEnrolled ||
         result_code_ == ResultCode::kSuccessNoNewKeysNeeded;
}

}  // namespace device_sync

}  // namespace chromeos
