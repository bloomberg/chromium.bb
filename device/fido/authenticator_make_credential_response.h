// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AUTHENTICATOR_MAKE_CREDENTIAL_RESPONSE_H_
#define DEVICE_FIDO_AUTHENTICATOR_MAKE_CREDENTIAL_RESPONSE_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "device/fido/fido_constants.h"

namespace device {

// Attestation object which includes attestation format, authentication
// data, and attestation statement returned by the authenticator as a response
// to MakeCredential request.
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#authenticatorMakeCredential
class COMPONENT_EXPORT(DEVICE_FIDO) AuthenticatorMakeCredentialResponse {
 public:
  AuthenticatorMakeCredentialResponse(CtapDeviceResponseCode response_code,
                                      std::vector<uint8_t> attestation_object);

  AuthenticatorMakeCredentialResponse(
      AuthenticatorMakeCredentialResponse&& that);
  AuthenticatorMakeCredentialResponse& operator=(
      AuthenticatorMakeCredentialResponse&& other);
  ~AuthenticatorMakeCredentialResponse();

  CtapDeviceResponseCode response_code() const { return response_code_; }
  const std::vector<uint8_t>& attestation_object() const {
    return attestation_object_;
  }

 private:
  CtapDeviceResponseCode response_code_;
  std::vector<uint8_t> attestation_object_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorMakeCredentialResponse);
};

}  // namespace device

#endif  // DEVICE_FIDO_AUTHENTICATOR_MAKE_CREDENTIAL_RESPONSE_H_
