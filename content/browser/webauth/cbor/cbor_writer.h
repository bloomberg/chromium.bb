// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_CBOR_CBOR_WRITER_H_
#define CONTENT_BROWSER_WEBAUTH_CBOR_CBOR_WRITER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/numerics/safe_math.h"
#include "base/strings/string_piece.h"
#include "content/browser/webauth/cbor/cbor_values.h"
#include "content/common/content_export.h"

// A basic Concise Binary Object Representation (CBOR) encoder as defined by
// https://tools.ietf.org/html/rfc7049.
// This is a non-canonical, generic encoder that supplies well-formed
// CBOR values but does not guarantee their validity (see
// https://tools.ietf.org/html/rfc7049#section-3.2).
// Supported:
//  * Major types:
//     * 0: Unsigned integers, up to 64-bit.
//     * 2: Byte strings.
//     * 3: UTF-8 strings.
//     * 4: Arrays, with the number of elements known at the start.
//     * 5: Maps, with the number of elements known at the start
//              of the container.
// Unsupported:
//  * Negative integers.
//  * Floating-point numbers.
//  * Indefinite-length encodings.
//  * Parsing.
enum class CborMajorType {
  kUnsigned = 0,    // Unsigned integer.
  kNegative = 1,    // Negative integer. Unsupported by this implementation.
  kByteString = 2,  // Byte string.
  kString = 3,      // String.
  kArray = 4,       // Array.
  kMap = 5,         // Map.
};

namespace content {

namespace {
// Mask selecting the last 5 bits  of the "initial byte" where
// 'additional information is encoded.
constexpr uint8_t kAdditionalInformationDataMask = 0x1F;
// Indicates the integer is in the following byte.
constexpr uint8_t kAdditionalInformation1Byte = 24;
// Indicates the integer is in the next 2 bytes.
constexpr uint8_t kAdditionalInformation2Bytes = 25;
// Indicates the integer is in the next 4 bytes.
constexpr uint8_t kAdditionalInformation4Bytes = 26;
// Indicates the integer is in the next 8 bytes.
constexpr uint8_t kAdditionalInformation8Bytes = 27;
}  // namespace

class CONTENT_EXPORT CBORWriter {
 public:
  ~CBORWriter();

  // Generates a CBOR byte string.
  static std::vector<uint8_t> Write(const CBORValue& node);

 private:
  CBORWriter(std::vector<uint8_t>* cbor);

  // Called recursively to build the CBOR bytestring. When completed,
  // |encoded_cbor_| will contain the CBOR.
  void EncodeCBOR(const CBORValue& node);

  // Encodes the type and size of the data being added.
  void StartItem(CborMajorType type, uint64_t size);

  // Encodes the additional information for the data.
  void SetAdditionalInformation(uint8_t additional_information);

  // Encodes an unsigned integer value. This is used to both write
  // unsigned integers and to encode the lengths of other major types.
  void SetUint(uint64_t value);

  // Get the number of bytes needed to store the unsigned integer.
  size_t GetNumUintBytes(uint64_t value);

  // Holds the encoded CBOR data.
  std::vector<uint8_t>* encoded_cbor_;

  DISALLOW_COPY_AND_ASSIGN(CBORWriter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_CBOR_CBOR_WRITER_H_
