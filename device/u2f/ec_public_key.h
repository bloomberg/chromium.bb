// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_EC_PUBLIC_KEY_H_
#define DEVICE_U2F_EC_PUBLIC_KEY_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/macros.h"
#include "device/u2f/public_key.h"

namespace device {

// An uncompressed ECPublicKey consisting of 64 bytes:
// - the 32-byte x coordinate
// - the 32-byte y coordinate.
class ECPublicKey : public PublicKey {
 public:
  static std::unique_ptr<ECPublicKey> ExtractFromU2fRegistrationResponse(
      std::string algorithm,
      base::span<const uint8_t> u2f_data);

  ECPublicKey(std::string algorithm,
              std::vector<uint8_t> x,
              std::vector<uint8_t> y);

  ~ECPublicKey() override;

  // Produces a CBOR-encoded public key encoded in the following format:
  // { alg: eccAlgName, x: biguint, y: biguint }
  // where eccAlgName = "ES256" / "ES384" / "ES512"
  std::vector<uint8_t> EncodeAsCBOR() const override;

 private:
  const std::vector<uint8_t> x_coordinate_;
  const std::vector<uint8_t> y_coordinate_;

  DISALLOW_COPY_AND_ASSIGN(ECPublicKey);
};

}  // namespace device

#endif  // DEVICE_U2F_EC_PUBLIC_KEY_H_
