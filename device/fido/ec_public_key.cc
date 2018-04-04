// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ec_public_key.h"

#include <utility>

#include "components/cbor/cbor_writer.h"
#include "device/fido/u2f_parsing_utils.h"

namespace device {

namespace {
// The key is located after the first byte of the response
// (which is a reserved byte). It's in X9.62 format:
// - a constant 0x04 prefix to indicate an uncompressed key
// - the 32-byte x coordinate
// - the 32-byte y coordinate.
constexpr size_t kKeyCompressionTypeOffset = 1;
constexpr size_t kUncompressedKey = 0x04;
constexpr size_t kHeaderLength = 2;  // Account for reserved byte and prefix.
constexpr size_t kKeyLength = 32;
}  // namespace

// static
std::unique_ptr<ECPublicKey> ECPublicKey::ExtractFromU2fRegistrationResponse(
    std::string algorithm,
    base::span<const uint8_t> u2f_data) {
  if (u2f_data.size() < kHeaderLength ||
      u2f_data[kKeyCompressionTypeOffset] != kUncompressedKey)
    return nullptr;

  std::vector<uint8_t> x =
      u2f_parsing_utils::Extract(u2f_data, kHeaderLength, kKeyLength);

  if (x.empty())
    return nullptr;

  std::vector<uint8_t> y = u2f_parsing_utils::Extract(
      u2f_data, kHeaderLength + kKeyLength, kKeyLength);

  if (y.empty())
    return nullptr;

  return std::make_unique<ECPublicKey>(std::move(algorithm), std::move(x),
                                       std::move(y));
}

ECPublicKey::ECPublicKey(std::string algorithm,
                         std::vector<uint8_t> x,
                         std::vector<uint8_t> y)
    : PublicKey(std::move(algorithm)),
      x_coordinate_(std::move(x)),
      y_coordinate_(std::move(y)) {
  DCHECK_EQ(x_coordinate_.size(), kKeyLength);
  DCHECK_EQ(y_coordinate_.size(), kKeyLength);
}

ECPublicKey::~ECPublicKey() = default;

std::vector<uint8_t> ECPublicKey::EncodeAsCOSEKey() const {
  cbor::CBORValue::MapValue map;
  map[cbor::CBORValue(1)] = cbor::CBORValue(2);
  map[cbor::CBORValue(3)] = cbor::CBORValue(-7);
  map[cbor::CBORValue(-1)] = cbor::CBORValue(1);
  map[cbor::CBORValue(-2)] = cbor::CBORValue(x_coordinate_);
  map[cbor::CBORValue(-3)] = cbor::CBORValue(y_coordinate_);
  return *cbor::CBORWriter::Write(cbor::CBORValue(std::move(map)));
}

}  // namespace device
