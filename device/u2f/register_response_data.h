// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_REGISTER_RESPONSE_DATA_H_
#define DEVICE_U2F_REGISTER_RESPONSE_DATA_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"

namespace device {

class AttestationObject;

// See figure 2:
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/ \
// fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#using-the- \
// ctap2-authenticatormakecredential-command-with-ctap1-u2f-authenticators
class RegisterResponseData {
 public:
  static RegisterResponseData CreateFromU2fRegisterResponse(
      std::string relying_party_id,
      std::vector<uint8_t> u2f_data);

  RegisterResponseData();

  RegisterResponseData(std::vector<uint8_t> credential_id,
                       std::unique_ptr<AttestationObject> object);

  // Moveable.
  RegisterResponseData(RegisterResponseData&& other);
  RegisterResponseData& operator=(RegisterResponseData&& other);

  ~RegisterResponseData();

  std::string GetId() const;
  std::vector<uint8_t> GetCBOREncodedAttestationObject() const;
  const std::vector<uint8_t>& raw_id() const { return raw_id_; }

 private:
  std::vector<uint8_t> raw_id_;
  std::unique_ptr<AttestationObject> attestation_object_;

  DISALLOW_COPY_AND_ASSIGN(RegisterResponseData);
};

}  // namespace device

#endif  // DEVICE_U2F_REGISTER_RESPONSE_DATA_H_
