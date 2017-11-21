// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/ec_public_key.h"

#include <utility>

#include "content/browser/webauth/authenticator_utils.h"
#include "content/browser/webauth/cbor/cbor_writer.h"

namespace content {

// static
std::unique_ptr<ECPublicKey> ECPublicKey::ExtractFromU2fRegistrationResponse(
    std::string algorithm,
    const std::vector<uint8_t>& u2f_data) {
  // Extract the key, which is located after the first byte of the response
  // (which is a reserved byte).
  // The uncompressed form consists of 65 bytes:
  // - a constant 0x04 prefix
  // - the 32-byte x coordinate
  // - the 32-byte y coordinate.
  int start = 2;  // Account for reserved byte and 0x04 prefix.
  std::vector<uint8_t> x = authenticator_utils::Extract(u2f_data, start, 32);
  std::vector<uint8_t> y =
      authenticator_utils::Extract(u2f_data, start + 32, 32);
  return std::make_unique<ECPublicKey>(std::move(algorithm), std::move(x),
                                       std::move(y));
}

ECPublicKey::ECPublicKey(std::string algorithm,
                         std::vector<uint8_t> x,
                         std::vector<uint8_t> y)
    : PublicKey(std::move(algorithm)),
      x_coordinate_(std::move(x)),
      y_coordinate_(std::move(y)) {
  DCHECK_EQ(x_coordinate_.size(), 32u);
  DCHECK_EQ(y_coordinate_.size(), 32u);
}

std::vector<uint8_t> ECPublicKey::EncodeAsCBOR() {
  CBORValue::MapValue map;
  map["alg"] = CBORValue(algorithm_.c_str());
  map["x"] = CBORValue(x_coordinate_);
  map["y"] = CBORValue(y_coordinate_);
  auto cbor = CBORWriter::Write(CBORValue(map));
  DCHECK(cbor.has_value());
  return cbor.value();
}

ECPublicKey::~ECPublicKey() {}

}  // namespace content
