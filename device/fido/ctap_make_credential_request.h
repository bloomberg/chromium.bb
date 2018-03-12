// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP_MAKE_CREDENTIAL_REQUEST_H_
#define DEVICE_FIDO_CTAP_MAKE_CREDENTIAL_REQUEST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace device {

// Object containing request parameters for AuthenticatorMakeCredential command
// as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html
class CtapMakeCredentialRequest {
 public:
  CtapMakeCredentialRequest(
      std::vector<uint8_t> client_data_hash,
      PublicKeyCredentialRpEntity rp,
      PublicKeyCredentialUserEntity user,
      PublicKeyCredentialParams public_key_credential_params);
  CtapMakeCredentialRequest(CtapMakeCredentialRequest&& that);
  CtapMakeCredentialRequest& operator=(CtapMakeCredentialRequest&& that);
  ~CtapMakeCredentialRequest();

  std::vector<uint8_t> EncodeAsCBOR() const;

  CtapMakeCredentialRequest& SetUserVerificationRequired(
      bool user_verfication_required);
  CtapMakeCredentialRequest& SetResidentKeySupported(bool resident_key);
  CtapMakeCredentialRequest& SetExcludeList(
      std::vector<PublicKeyCredentialDescriptor> exclude_list);
  CtapMakeCredentialRequest& SetPinAuth(std::vector<uint8_t> pin_auth);
  CtapMakeCredentialRequest& SetPinProtocol(uint8_t pin_protocol);

  const std::vector<uint8_t>& client_data_hash() const {
    return client_data_hash_;
  }
  const PublicKeyCredentialRpEntity& rp() const { return rp_; }
  const PublicKeyCredentialUserEntity user() const { return user_; }
  const PublicKeyCredentialParams& public_key_credential_params() const {
    return public_key_credential_params_;
  }
  bool user_verification_required() const {
    return user_verification_required_;
  }
  bool resident_key_supported() const { return resident_key_supported_; }

 private:
  std::vector<uint8_t> client_data_hash_;
  PublicKeyCredentialRpEntity rp_;
  PublicKeyCredentialUserEntity user_;
  PublicKeyCredentialParams public_key_credential_params_;
  bool user_verification_required_ = false;
  bool resident_key_supported_ = false;

  base::Optional<std::vector<PublicKeyCredentialDescriptor>> exclude_list_;
  base::Optional<std::vector<uint8_t>> pin_auth_;
  base::Optional<uint8_t> pin_protocol_;

  DISALLOW_COPY_AND_ASSIGN(CtapMakeCredentialRequest);
};

}  // namespace device

#endif  // DEVICE_FIDO_CTAP_MAKE_CREDENTIAL_REQUEST_H_
