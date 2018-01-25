// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_CTAP_AUTHENTICATOR_GET_ASSERTION_RESPONSE_H_
#define DEVICE_CTAP_AUTHENTICATOR_GET_ASSERTION_RESPONSE_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "device/ctap/ctap_constants.h"
#include "device/ctap/public_key_credential_descriptor.h"
#include "device/ctap/public_key_credential_user_entity.h"

namespace device {

// Represents response from authenticators for AuthenticatorGetAssertion and
// AuthenticatorGetNextAssertion requests.
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#authenticatorGetAssertion
class AuthenticatorGetAssertionResponse {
 public:
  AuthenticatorGetAssertionResponse(CTAPDeviceResponseCode response_code,
                                    std::vector<uint8_t> auth_data,
                                    std::vector<uint8_t> signature,
                                    PublicKeyCredentialUserEntity user);
  AuthenticatorGetAssertionResponse(AuthenticatorGetAssertionResponse&& that);
  AuthenticatorGetAssertionResponse& operator=(
      AuthenticatorGetAssertionResponse&& other);
  ~AuthenticatorGetAssertionResponse();

  AuthenticatorGetAssertionResponse& SetNumCredentials(uint8_t num_credentials);
  AuthenticatorGetAssertionResponse& SetCredential(
      PublicKeyCredentialDescriptor credential);

  CTAPDeviceResponseCode response_code() const { return response_code_; }
  const std::vector<uint8_t>& auth_data() const { return auth_data_; }
  const std::vector<uint8_t>& signature() const { return signature_; }
  const PublicKeyCredentialUserEntity& user() const { return user_; }
  const base::Optional<uint8_t>& num_credentials() const {
    return num_credentials_;
  }
  const base::Optional<PublicKeyCredentialDescriptor>& credential() const {
    return credential_;
  }

 private:
  CTAPDeviceResponseCode response_code_;
  std::vector<uint8_t> auth_data_;
  std::vector<uint8_t> signature_;
  PublicKeyCredentialUserEntity user_;
  base::Optional<uint8_t> num_credentials_;
  base::Optional<PublicKeyCredentialDescriptor> credential_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorGetAssertionResponse);
};

}  // namespace device

#endif  // DEVICE_CTAP_AUTHENTICATOR_GET_ASSERTION_RESPONSE_H_
