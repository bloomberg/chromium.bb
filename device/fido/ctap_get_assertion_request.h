// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP_GET_ASSERTION_REQUEST_H_
#define DEVICE_FIDO_CTAP_GET_ASSERTION_REQUEST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "device/fido/public_key_credential_descriptor.h"

namespace device {

// Object that encapsulates request parameters for AuthenticatorGetAssertion as
// specified in the CTAP spec.
// https://fidoalliance.org/specs/fido-v2.0-rd-20161004/fido-client-to-authenticator-protocol-v2.0-rd-20161004.html#authenticatorgetassertion
class CtapGetAssertionRequest {
 public:
  CtapGetAssertionRequest(std::string rp_id,
                          std::vector<uint8_t> client_data_hash);
  CtapGetAssertionRequest(CtapGetAssertionRequest&& that);
  CtapGetAssertionRequest& operator=(CtapGetAssertionRequest&& other);
  ~CtapGetAssertionRequest();

  std::vector<uint8_t> EncodeAsCBOR() const;

  CtapGetAssertionRequest& SetUserVerificationRequired(
      bool user_verfication_required);
  CtapGetAssertionRequest& SetUserPresenceRequired(bool user_presence_required);
  CtapGetAssertionRequest& SetAllowList(
      std::vector<PublicKeyCredentialDescriptor> allow_list);
  CtapGetAssertionRequest& SetPinAuth(std::vector<uint8_t> pin_auth);
  CtapGetAssertionRequest& SetPinProtocol(uint8_t pin_protocol);

  const std::string& rp_id() const { return rp_id_; }
  const std::vector<uint8_t>& client_data_hash() const {
    return client_data_hash_;
  }

  bool user_verification_required() const {
    return user_verification_required_;
  }

  bool user_presence_required() const { return user_presence_required_; }
  const base::Optional<std::vector<PublicKeyCredentialDescriptor>>& allow_list()
      const {
    return allow_list_;
  }

  const base::Optional<std::vector<uint8_t>>& pin_auth() const {
    return pin_auth_;
  }

  const base::Optional<uint8_t>& pin_protocol() const { return pin_protocol_; }

 private:
  std::string rp_id_;
  std::vector<uint8_t> client_data_hash_;
  bool user_verification_required_ = false;
  bool user_presence_required_ = true;

  base::Optional<std::vector<PublicKeyCredentialDescriptor>> allow_list_;
  base::Optional<std::vector<uint8_t>> pin_auth_;
  base::Optional<uint8_t> pin_protocol_;

  DISALLOW_COPY_AND_ASSIGN(CtapGetAssertionRequest);
};

}  // namespace device

#endif  // DEVICE_FIDO_CTAP_GET_ASSERTION_REQUEST_H_
