// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP_MAKE_CREDENTIAL_REQUEST_H_
#define DEVICE_FIDO_CTAP_MAKE_CREDENTIAL_REQUEST_H_

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/fido/fido_constants.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace device {

// Object containing request parameters for AuthenticatorMakeCredential command
// as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html
class COMPONENT_EXPORT(DEVICE_FIDO) CtapMakeCredentialRequest {
 public:
  using ClientDataHash = std::array<uint8_t, kClientDataHashLength>;

  CtapMakeCredentialRequest(
      std::string client_data_json,
      PublicKeyCredentialRpEntity rp,
      PublicKeyCredentialUserEntity user,
      PublicKeyCredentialParams public_key_credential_params);
  CtapMakeCredentialRequest(const CtapMakeCredentialRequest& that);
  CtapMakeCredentialRequest(CtapMakeCredentialRequest&& that);
  CtapMakeCredentialRequest& operator=(const CtapMakeCredentialRequest& that);
  CtapMakeCredentialRequest& operator=(CtapMakeCredentialRequest&& that);
  ~CtapMakeCredentialRequest();

  // Serializes MakeCredential request parameter into CBOR encoded map with
  // integer keys and CBOR encoded values as defined by the CTAP spec.
  // https://drafts.fidoalliance.org/fido-2/latest/fido-client-to-authenticator-protocol-v2.0-wd-20180305.html#authenticatorMakeCredential
  std::vector<uint8_t> EncodeAsCBOR() const;

  CtapMakeCredentialRequest& SetAuthenticatorAttachment(
      AuthenticatorAttachment authenticator_attachment);
  CtapMakeCredentialRequest& SetUserVerification(
      UserVerificationRequirement user_verification);
  CtapMakeCredentialRequest& SetResidentKeyRequired(bool resident_key);
  CtapMakeCredentialRequest& SetExcludeList(
      std::vector<PublicKeyCredentialDescriptor> exclude_list);
  CtapMakeCredentialRequest& SetPinAuth(std::vector<uint8_t> pin_auth);
  CtapMakeCredentialRequest& SetPinProtocol(uint8_t pin_protocol);
  CtapMakeCredentialRequest& SetIsIndividualAttestation(
      bool is_individual_attestation);
  CtapMakeCredentialRequest& SetHmacSecret(bool hmac_secret);

  const std::string& client_data_json() const { return client_data_json_; }
  const std::array<uint8_t, kClientDataHashLength>& client_data_hash() const {
    return client_data_hash_;
  }
  const PublicKeyCredentialRpEntity& rp() const { return rp_; }
  const PublicKeyCredentialUserEntity user() const { return user_; }
  const PublicKeyCredentialParams& public_key_credential_params() const {
    return public_key_credential_params_;
  }
  UserVerificationRequirement user_verification() const {
    return user_verification_;
  }
  AuthenticatorAttachment authenticator_attachment() const {
    return authenticator_attachment_;
  }
  bool resident_key_required() const { return resident_key_required_; }
  bool is_individual_attestation() const { return is_individual_attestation_; }
  bool hmac_secret() const { return hmac_secret_; }
  const base::Optional<std::vector<PublicKeyCredentialDescriptor>>&
  exclude_list() const {
    return exclude_list_;
  }
  const base::Optional<std::vector<uint8_t>>& pin_auth() const {
    return pin_auth_;
  }

  void set_is_u2f_only(bool is_u2f_only) { is_u2f_only_ = is_u2f_only; }
  bool is_u2f_only() { return is_u2f_only_; }

 private:
  std::string client_data_json_;
  std::array<uint8_t, kClientDataHashLength> client_data_hash_;
  PublicKeyCredentialRpEntity rp_;
  PublicKeyCredentialUserEntity user_;
  PublicKeyCredentialParams public_key_credential_params_;
  UserVerificationRequirement user_verification_ =
      UserVerificationRequirement::kPreferred;
  AuthenticatorAttachment authenticator_attachment_ =
      AuthenticatorAttachment::kAny;
  bool resident_key_required_ = false;
  bool is_individual_attestation_ = false;
  // hmac_secret_ indicates whether the "hmac-secret" extension should be
  // asserted to CTAP2 authenticators.
  bool hmac_secret_ = false;

  // If true, instruct the request handler only to dispatch this request via
  // U2F.
  bool is_u2f_only_ = false;

  base::Optional<std::vector<PublicKeyCredentialDescriptor>> exclude_list_;
  base::Optional<std::vector<uint8_t>> pin_auth_;
  base::Optional<uint8_t> pin_protocol_;
};

}  // namespace device

#endif  // DEVICE_FIDO_CTAP_MAKE_CREDENTIAL_REQUEST_H_
