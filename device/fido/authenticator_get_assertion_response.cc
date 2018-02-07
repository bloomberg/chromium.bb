// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_get_assertion_response.h"

#include <utility>

namespace device {

AuthenticatorGetAssertionResponse::AuthenticatorGetAssertionResponse(
    CtapDeviceResponseCode response_code,
    std::vector<uint8_t> auth_data,
    std::vector<uint8_t> signature,
    PublicKeyCredentialUserEntity user)
    : response_code_(response_code),
      auth_data_(std::move(auth_data)),
      signature_(std::move(signature)),
      user_(std::move(user)) {}

AuthenticatorGetAssertionResponse::AuthenticatorGetAssertionResponse(
    AuthenticatorGetAssertionResponse&& that) = default;

AuthenticatorGetAssertionResponse& AuthenticatorGetAssertionResponse::operator=(
    AuthenticatorGetAssertionResponse&& other) = default;

AuthenticatorGetAssertionResponse::~AuthenticatorGetAssertionResponse() =
    default;

AuthenticatorGetAssertionResponse&
AuthenticatorGetAssertionResponse::SetNumCredentials(uint8_t num_credentials) {
  num_credentials_ = num_credentials;
  return *this;
}

AuthenticatorGetAssertionResponse&
AuthenticatorGetAssertionResponse::SetCredential(
    PublicKeyCredentialDescriptor credential) {
  credential_ = std::move(credential);
  return *this;
}

}  // namespace device
