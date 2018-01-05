// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_CTAP_CTAP_MAKE_CREDENTIAL_REQUEST_PARAM_H_
#define DEVICE_CTAP_CTAP_MAKE_CREDENTIAL_REQUEST_PARAM_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/optional.h"
#include "device/ctap/ctap_request_param.h"
#include "device/ctap/public_key_credential_descriptor.h"
#include "device/ctap/public_key_credential_params.h"
#include "device/ctap/public_key_credential_rp_entity.h"
#include "device/ctap/public_key_credential_user_entity.h"

namespace device {

// Object containing request parameters for AuthenticatorMakeCredential command
// as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html
class CTAPMakeCredentialRequestParam : public CTAPRequestParam {
 public:
  CTAPMakeCredentialRequestParam(
      std::vector<uint8_t> client_data_hash,
      PublicKeyCredentialRPEntity rp,
      PublicKeyCredentialUserEntity user,
      PublicKeyCredentialParams public_key_credential_params);
  CTAPMakeCredentialRequestParam(CTAPMakeCredentialRequestParam&& that);
  CTAPMakeCredentialRequestParam& operator=(
      CTAPMakeCredentialRequestParam&& that);
  ~CTAPMakeCredentialRequestParam() override;

  base::Optional<std::vector<uint8_t>> SerializeToCBOR() const override;
  CTAPMakeCredentialRequestParam& SetExcludeList(
      std::vector<PublicKeyCredentialDescriptor> exclude_list);
  CTAPMakeCredentialRequestParam& SetPinAuth(std::vector<uint8_t> pin_auth);
  CTAPMakeCredentialRequestParam& SetPinProtocol(uint8_t pin_protocol);

 private:
  std::vector<uint8_t> client_data_hash_;
  PublicKeyCredentialRPEntity rp_;
  PublicKeyCredentialUserEntity user_;
  PublicKeyCredentialParams public_key_credentials_;
  base::Optional<std::vector<PublicKeyCredentialDescriptor>> exclude_list_;
  base::Optional<std::vector<uint8_t>> pin_auth_;
  base::Optional<uint8_t> pin_protocol_;

  DISALLOW_COPY_AND_ASSIGN(CTAPMakeCredentialRequestParam);
};

}  // namespace device

#endif  // DEVICE_CTAP_CTAP_MAKE_CREDENTIAL_REQUEST_PARAM_H_
