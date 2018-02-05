// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP_AUTHENTICATOR_REQUEST_PARAM_H_
#define DEVICE_FIDO_CTAP_AUTHENTICATOR_REQUEST_PARAM_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/optional.h"
#include "device/fido/ctap_constants.h"
#include "device/fido/ctap_request_param.h"

namespace device {

// Represents CTAP requests with empty parameters, including
// AuthenticatorGetInfo, AuthenticatorCancel, AuthenticatorReset and
// AuthenticatorGetNextAssertion commands.
class CTAPAuthenticatorRequestParam : public CTAPRequestParam {
 public:
  static CTAPAuthenticatorRequestParam CreateGetInfoParam();
  static CTAPAuthenticatorRequestParam CreateGetNextAssertionParam();
  static CTAPAuthenticatorRequestParam CreateResetParam();
  static CTAPAuthenticatorRequestParam CreateCancelParam();

  CTAPAuthenticatorRequestParam(CTAPAuthenticatorRequestParam&& that);
  CTAPAuthenticatorRequestParam& operator=(
      CTAPAuthenticatorRequestParam&& that);
  ~CTAPAuthenticatorRequestParam() override;

  base::Optional<std::vector<uint8_t>> Encode() const override;

 private:
  explicit CTAPAuthenticatorRequestParam(CTAPRequestCommand cmd);

  CTAPRequestCommand cmd_;

  DISALLOW_COPY_AND_ASSIGN(CTAPAuthenticatorRequestParam);
};

}  // namespace device

#endif  // DEVICE_FIDO_CTAP_AUTHENTICATOR_REQUEST_PARAM_H_
