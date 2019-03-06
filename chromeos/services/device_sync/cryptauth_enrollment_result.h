// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_RESULT_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_RESULT_H_

#include <ostream>

#include "base/optional.h"
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
    // During SyncKeys API call, request could not be completed because the
    // device is offline or has issues sending the HTTP request.
    kErrorSyncKeysApiCallOffline,
    // During SyncKeys API call, server endpoint could not be found.
    kErrorSyncKeysApiCallEndpointNotFound,
    // During SyncKeys API call, authentication error contacting back-end.
    kErrorSyncKeysApiCallAuthenticationError,
    // During SyncKeys API call, network request was invalid.
    kErrorSyncKeysApiCallBadRequest,
    // During SyncKeys API call, the server responded but the response was not
    // formatted correctly.
    kErrorSyncKeysApiCallResponseMalformed,
    // During SyncKeys API call, internal server error.
    kErrorSyncKeysApiCallInternalServerError,
    // During SyncKeys API call, unknown network request error.
    kErrorSyncKeysApiCallUnknownError,
    // During EnrollKeys API call, request could not be completed because the
    // device is offline or has issues sending the HTTP request.
    kErrorEnrollKeysApiCallOffline,
    // During EnrollKeys API call, server endpoint could not be found.
    kErrorEnrollKeysApiCallEndpointNotFound,
    // During EnrollKeys API call, authentication error contacting back-end.
    kErrorEnrollKeysApiCallAuthenticationError,
    // During EnrollKeys API call, network request was invalid.
    kErrorEnrollKeysApiCallBadRequest,
    // During EnrollKeys API call, the server responded but the response was not
    // formatted correctly.
    kErrorEnrollKeysApiCallResponseMalformed,
    // During EnrollKeys API call, internal server error.
    kErrorEnrollKeysApiCallInternalServerError,
    // During EnrollKeys API call, unknown network request error.
    kErrorEnrollKeysApiCallUnknownError,
    // The CryptAuth server indicated via SyncKeysResponse::server_status that
    // it was overloaded and did not process the SyncKeysRequest.
    kErrorCryptAuthServerOverloaded,
    // The SyncKeysResponse from CryptAuth is missing a random_session_id.
    kErrorSyncKeysResponseMissingRandomSessionId,
    // The SyncKeysResponse from CryptAuth is missing a client_directive or the
    // parameters are invalid.
    kErrorSyncKeysResponseInvalidClientDirective,
    // The number of SyncSingleKeyResponses does not agree with the number of
    // SyncSingleKeyRequests.
    kErrorWrongNumberOfSyncSingleKeyResponses,
    // The size of a SyncSingleKeyResponse::key_actions list does not agree with
    // the size of the corresponding SyncSingleKeyRequest::key_handles.
    kErrorWrongNumberOfKeyActions,
    // An integer provided in SyncSingleKeyResponse::key_actions does not
    // correspond to a known KeyAction enum value.
    kErrorInvalidKeyActionEnumValue,
    // The SyncSingleKeyResponse::key_actions denote more than one active key.
    kErrorKeyActionsSpecifyMultipleActiveKeys,
    // The SyncSingleKeyResponse::key_actions fail to specify an active key.
    kErrorKeyActionsDoNotSpecifyAnActiveKey,
    // KeyCreation instructions specify an unsupported KeyType.
    kErrorKeyCreationKeyTypeNotSupported,
    // Invalid key-creation instructions for user key pair. It must be P256 and
    // active.
    kErrorUserKeyPairCreationInstructionsInvalid,
    // Cannot create a symmetric key without the server's Diffie-Hellman key.
    kErrorSymmetricKeyCreationMissingServerDiffieHellman,
    // Failed to compute at least one key proof.
    kErrorKeyProofComputationFailed,
    // The enroller timed out waiting for response from the SyncKeys API call.
    kErrorTimeoutWaitingForSyncKeysResponse,
    // The enroller timed out waiting for new keys to be created.
    kErrorTimeoutWaitingForKeyCreation,
    // The enroller timed out waiting for key proofs to be computed.
    kErrorTimeoutWaitingForEnrollKeysResponse,
    // Failed to register local device with GCM. This registration is required
    // in order for CryptAuth to send GCM messages to the local device,
    // requesting it to re-enroll or re-sync.
    kErrorGcmRegistrationFailed,
    // Could not retrieve ClientAppMetadata from ClientAppMetadataProvider.
    kErrorClientAppMetadataFetchFailed
  };

  CryptAuthEnrollmentResult(
      ResultCode result_code,
      const base::Optional<cryptauthv2::ClientDirective>& client_directive);
  CryptAuthEnrollmentResult(const CryptAuthEnrollmentResult& other);

  ~CryptAuthEnrollmentResult();

  ResultCode result_code() const { return result_code_; }

  const base::Optional<cryptauthv2::ClientDirective>& client_directive() const {
    return client_directive_;
  }

  bool IsSuccess() const;

  bool operator==(const CryptAuthEnrollmentResult& other) const;
  bool operator!=(const CryptAuthEnrollmentResult& other) const;

 private:
  ResultCode result_code_;
  base::Optional<cryptauthv2::ClientDirective> client_directive_;
};

std::ostream& operator<<(
    std::ostream& stream,
    const CryptAuthEnrollmentResult::ResultCode result_code);

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_ENROLLMENT_RESULT_H_
