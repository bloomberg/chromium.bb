// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP_EMPTY_AUTHENTICATOR_REQUEST_H_
#define DEVICE_FIDO_CTAP_EMPTY_AUTHENTICATOR_REQUEST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "device/fido/ctap_constants.h"

namespace device {

namespace internal {

// Represents CTAP requests with empty parameters, including
// AuthenticatorGetInfo, AuthenticatorCancel, AuthenticatorReset and
// AuthenticatorGetNextAssertion commands.
class CtapEmptyAuthenticatorRequest {
 public:
  CtapRequestCommand cmd() const { return cmd_; }
  std::vector<uint8_t> Serialize() const;

 protected:
  explicit CtapEmptyAuthenticatorRequest(CtapRequestCommand cmd) : cmd_(cmd) {}

 private:
  CtapRequestCommand cmd_;
};

}  // namespace internal

class AuthenticatorGetNextAssertionRequest
    : public internal::CtapEmptyAuthenticatorRequest {
 public:
  AuthenticatorGetNextAssertionRequest()
      : CtapEmptyAuthenticatorRequest(
            CtapRequestCommand::kAuthenticatorGetNextAssertion) {}
};

class AuthenticatorGetInfoRequest
    : public internal::CtapEmptyAuthenticatorRequest {
 public:
  AuthenticatorGetInfoRequest()
      : CtapEmptyAuthenticatorRequest(
            CtapRequestCommand::kAuthenticatorGetInfo) {}
};

class AuthenticatorResetRequest
    : public internal::CtapEmptyAuthenticatorRequest {
 public:
  AuthenticatorResetRequest()
      : CtapEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorReset) {
  }
};

class AuthenticatorCancelRequest
    : public internal::CtapEmptyAuthenticatorRequest {
 public:
  AuthenticatorCancelRequest()
      : CtapEmptyAuthenticatorRequest(
            CtapRequestCommand::kAuthenticatorCancel) {}
};

}  // namespace device

#endif  // DEVICE_FIDO_CTAP_EMPTY_AUTHENTICATOR_REQUEST_H_
