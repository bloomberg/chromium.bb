// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_REGISTER_RESPONSE_DATA_H_
#define DEVICE_U2F_REGISTER_RESPONSE_DATA_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/u2f/response_data.h"

namespace device {

class AttestationObject;

// See figure 2: https://goo.gl/rsgvXk
class RegisterResponseData : public ResponseData {
 public:
  static base::Optional<RegisterResponseData> CreateFromU2fRegisterResponse(
      std::string relying_party_id,
      base::span<const uint8_t> u2f_data);

  RegisterResponseData();

  RegisterResponseData(std::vector<uint8_t> credential_id,
                       std::unique_ptr<AttestationObject> object);

  // Moveable.
  RegisterResponseData(RegisterResponseData&& other);
  RegisterResponseData& operator=(RegisterResponseData&& other);

  ~RegisterResponseData();

  std::vector<uint8_t> GetCBOREncodedAttestationObject() const;

 private:
  std::unique_ptr<AttestationObject> attestation_object_;

  DISALLOW_COPY_AND_ASSIGN(RegisterResponseData);
};

}  // namespace device

#endif  // DEVICE_U2F_REGISTER_RESPONSE_DATA_H_
