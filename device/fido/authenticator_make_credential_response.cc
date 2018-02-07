// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_make_credential_response.h"

#include <utility>

namespace device {

AuthenticatorMakeCredentialResponse::AuthenticatorMakeCredentialResponse(
    CtapDeviceResponseCode response_code,
    std::vector<uint8_t> attestation_object)
    : response_code_(response_code),
      attestation_object_(std::move(attestation_object)) {}

AuthenticatorMakeCredentialResponse::AuthenticatorMakeCredentialResponse(
    AuthenticatorMakeCredentialResponse&& that) = default;

AuthenticatorMakeCredentialResponse& AuthenticatorMakeCredentialResponse::
operator=(AuthenticatorMakeCredentialResponse&& other) = default;

AuthenticatorMakeCredentialResponse::~AuthenticatorMakeCredentialResponse() =
    default;

}  // namespace device
