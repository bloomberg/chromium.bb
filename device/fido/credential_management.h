// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CREDENTIAL_MANAGEMENT_H_
#define DEVICE_FIDO_CREDENTIAL_MANAGEMENT_H_

#include "base/component_export.h"
#include "base/optional.h"
#include "device/fido/fido_constants.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace cbor {
class Value;
}

namespace device {

namespace pin {
struct EmptyResponse;
}

// https://drafts.fidoalliance.org/fido-2/latest/fido-client-to-authenticator-protocol-v2.0-wd-20190409.html#authenticatorCredentialManagement

enum class CredentialManagementRequestKey : uint8_t {
  kSubCommand = 0x01,
  kSubCommandParams = 0x02,
  kPinProtocol = 0x03,
  kPinAuth = 0x04,
};

enum class CredentialManagementRequestParamKey : uint8_t {
  kRPIDHash = 0x01,
  kCredentialID = 0x02,
};

enum class CredentialManagementResponseKey : uint8_t {
  kExistingResidentCredentialsCount = 0x01,
  kMaxPossibleRemainingResidentCredentialsCount = 0x02,
  kRP = 0x03,
  kRPIDHash = 0x04,
  kTotalRPs = 0x05,
  kUser = 0x06,
  kCredentialID = 0x07,
  kPublicKey = 0x08,
  kTotalCredentials = 0x09,
  kCredProtect = 0x0a,
};

enum class CredentialManagementSubCommand : uint8_t {
  kGetCredsMetadata = 0x01,
  kEnumerateRPsBegin = 0x02,
  kEnumerateRPsGetNextRP = 0x03,
  kEnumerateCredentialsBegin = 0x04,
  kEnumerateCredentialsGetNextCredential = 0x05,
  kDeleteCredential = 0x06,
};

// CredentialManagementPreviewRequestAdapter wraps any credential management
// request struct in order to replace the authenticatorCredentialManagement
// command byte returned by the static EncodeAsCBOR() method (0x0a) with its
// vendor-specific preview equivalent (0x41).
template <class T>
class CredentialManagementPreviewRequestAdapter {
 public:
  static std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
  EncodeAsCBOR(const CredentialManagementPreviewRequestAdapter<T>& request) {
    auto result = T::EncodeAsCBOR(request.wrapped_request_);
    DCHECK_EQ(result.first,
              CtapRequestCommand::kAuthenticatorCredentialManagement);
    result.first =
        CtapRequestCommand::kAuthenticatorCredentialManagementPreview;
    return result;
  }

  CredentialManagementPreviewRequestAdapter(T request)
      : wrapped_request_(std::move(request)) {}

 private:
  T wrapped_request_;
};

struct CredentialsMetadataRequest {
  static std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
  EncodeAsCBOR(const CredentialsMetadataRequest&);

  explicit CredentialsMetadataRequest(std::vector<uint8_t> pin_token);
  CredentialsMetadataRequest(CredentialsMetadataRequest&&);
  CredentialsMetadataRequest& operator=(CredentialsMetadataRequest&&);
  ~CredentialsMetadataRequest();

  std::vector<uint8_t> pin_token;

 private:
  CredentialsMetadataRequest(const CredentialsMetadataRequest&) = delete;
  CredentialsMetadataRequest& operator=(const CredentialsMetadataRequest&) =
      delete;
};

struct CredentialsMetadataResponse {
  static base::Optional<CredentialsMetadataResponse> Parse(
      const base::Optional<cbor::Value>& cbor_response);

  size_t num_existing_credentials;
  size_t num_estimated_remaining_credentials;

 private:
  CredentialsMetadataResponse() = default;
};

struct EnumerateRPsResponse {
  static base::Optional<EnumerateRPsResponse> Parse(
      const base::Optional<cbor::Value>& cbor_response,
      bool expect_rp_count);

  EnumerateRPsResponse(EnumerateRPsResponse&&);
  EnumerateRPsResponse& operator=(EnumerateRPsResponse&&);
  ~EnumerateRPsResponse();

  PublicKeyCredentialRpEntity rp;
  std::array<uint8_t, kRpIdHashLength> rp_id_hash;
  size_t rp_count;

 private:
  EnumerateRPsResponse(PublicKeyCredentialRpEntity rp,
                       std::array<uint8_t, kRpIdHashLength> rp_id_hash,
                       size_t rp_count);
  EnumerateRPsResponse(const EnumerateRPsResponse&) = delete;
  EnumerateRPsResponse& operator=(const EnumerateRPsResponse&) = delete;
};

struct EnumerateCredentialsResponse {
  static base::Optional<EnumerateCredentialsResponse> Parse(
      const base::Optional<cbor::Value>& cbor_response,
      bool expect_credential_count);

  EnumerateCredentialsResponse(EnumerateCredentialsResponse&&);
  EnumerateCredentialsResponse& operator=(EnumerateCredentialsResponse&&);
  ~EnumerateCredentialsResponse();

  PublicKeyCredentialUserEntity user;
  std::vector<uint8_t> credential_id;
  size_t credential_count;

 private:
  EnumerateCredentialsResponse(PublicKeyCredentialUserEntity user,
                               std::vector<uint8_t> credential_id,
                               size_t credential_count);
  EnumerateCredentialsResponse(const EnumerateCredentialsResponse&) = delete;
  EnumerateCredentialsResponse& operator=(EnumerateCredentialsResponse&) =
      delete;
};

struct COMPONENT_EXPORT(DEVICE_FIDO) AggregatedEnumerateCredentialsResponse {
  AggregatedEnumerateCredentialsResponse(PublicKeyCredentialRpEntity rp);
  ~AggregatedEnumerateCredentialsResponse();

  PublicKeyCredentialRpEntity rp;
  std::list<EnumerateCredentialsResponse> credentials;

 private:
  AggregatedEnumerateCredentialsResponse(
      const AggregatedEnumerateCredentialsResponse&) = delete;
};

using DeleteCredentialResponse = pin::EmptyResponse;

}  // namespace device

#endif  // DEVICE_FIDO_CREDENTIAL_MANAGEMENT_H_
