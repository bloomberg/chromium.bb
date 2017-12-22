// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CBOR_CBOR_WRITER_H_
#define COMPONENTS_CBOR_CBOR_WRITER_H_

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "base/optional.h"
#include "components/cbor/cbor_export.h"
#include "components/cbor/cbor_values.h"

// A basic Concise Binary Object Representation (CBOR) encoder as defined by
// https://tools.ietf.org/html/rfc7049. This is a generic encoder that supplies
// canonical, well-formed CBOR values but does not guarantee their validity
// (see https://tools.ietf.org/html/rfc7049#section-3.2).
// Supported:
//  * Major types:
//     * 0: Unsigned integers, up to INT64_MAX.
//     * 1: Negative integers, to INT64_MIN.
//     * 2: Byte strings.
//     * 3: UTF-8 strings.
//     * 4: Arrays, with the number of elements known at the start.
//     * 5: Maps, with the number of elements known at the start
//              of the container.
//     * 7: Simple values.
//
// Unsupported:
//  * Floating-point numbers.
//  * Indefinite-length encodings.
//  * Parsing.
//
// Requirements for canonical CBOR as suggested by RFC 7049 are:
//  1) All major data types for the CBOR values must be as short as possible.
//      * Unsigned integer between 0 to 23 must be expressed in same byte as
//            the major type.
//      * 24 to 255 must be expressed only with an additional uint8_t.
//      * 256 to 65535 must be expressed only with an additional uint16_t.
//      * 65536 to 4294967295 must be expressed only with an additional
//            uint32_t. * The rules for expression of length in major types
//            2 to 5 follow the above rule for integers.
//  2) Keys in every map must be sorted (first by major type, then by key
//         length, then by value in byte-wise lexical order).
//  3) Indefinite length items must be converted to definite length items.
//  4) All maps must not have duplicate keys.
//
// Current implementation of CBORWriter encoder meets all the requirements of
// canonical CBOR.

namespace cbor {

class CBOR_EXPORT CBORWriter {
 public:
  // Default that should be sufficiently large for most use cases.
  static constexpr size_t kDefaultMaxNestingDepth = 16;

  ~CBORWriter();

  // Returns the CBOR byte string representation of |node|, unless its nesting
  // depth is greater than |max_nesting_depth|, in which case an empty optional
  // value is returned. The nesting depth of |node| is defined as the number of
  // arrays/maps that has to be traversed to reach the most nested CBORValue
  // contained in |node|. Primitive values and empty containers have nesting
  // depths of 0.
  static base::Optional<std::vector<uint8_t>> Write(
      const CBORValue& node,
      size_t max_nesting_level = kDefaultMaxNestingDepth);

 private:
  explicit CBORWriter(std::vector<uint8_t>* cbor);

  // Called recursively to build the CBOR bytestring. When completed,
  // |encoded_cbor_| will contain the CBOR.
  bool EncodeCBOR(const CBORValue& node, int max_nesting_level);

  // Encodes the type and size of the data being added.
  void StartItem(CBORValue::Type type, uint64_t size);

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

}  // namespace cbor

#endif  // COMPONENTS_CBOR_CBOR_WRITER_H_
