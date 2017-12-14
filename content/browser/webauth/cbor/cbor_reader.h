// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_CBOR_CBOR_READER_H_
#define CONTENT_BROWSER_WEBAUTH_CBOR_CBOR_READER_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "base/optional.h"
#include "content/browser/webauth/cbor/cbor_values.h"
#include "content/common/content_export.h"

// Concise Binary Object Representation (CBOR) decoder as defined by
// https://tools.ietf.org/html/rfc7049. This decoder only accepts canonical
// CBOR as defined by section 3.9.
// Supported:
//  * Major types:
//     * 0: Unsigned integers, up to 64-bit.
//     * 2: Byte strings.
//     * 3: UTF-8 strings.
//     * 4: Definite-length arrays.
//     * 5: Definite-length maps.
//
// Requirements for canonical CBOR representation:
//  - Duplicate keys for map are not allowed.
//  - Keys for map must be sorted first by length and then by byte-wise
//    lexical order.
//
// Known limitations and interpretations of the RFC:
//  - Does not support negative integers, floating point numbers, indefinite
//    data streams and tagging.
//  - Non-character codepoint are not supported for Major type 3.
//  - Incomplete CBOR data items are treated as syntax errors.
//  - Trailing data bytes are treated as errors.
//  - Unknown additional information formats are treated as syntax errors.
//  - Callers can decode CBOR values with at most 16 nested depth layer. More
//    strict restrictions on nesting layer size of CBOR values can be enforced
//    by setting |max_nesting_level|.
//  - Only CBOR maps with integer or string type keys are supported due to the
//    cost of serialization when sorting map keys.

namespace content {

class CONTENT_EXPORT CBORReader {
 public:
  using Bytes = std::vector<uint8_t>;

  enum class DecoderError {
    CBOR_NO_ERROR = 0,
    UNSUPPORTED_MAJOR_TYPE,
    UNKNOWN_ADDITIONAL_INFO,
    INCOMPLETE_CBOR_DATA,
    INCORRECT_MAP_KEY_TYPE,
    TOO_MUCH_NESTING,
    INVALID_UTF8,
    EXTRANEOUS_DATA,
    DUPLICATE_KEY,
    OUT_OF_ORDER_KEY,
    NON_MINIMAL_CBOR_ENCODING,
  };

  // CBOR nested depth sufficient for most use cases.
  static const int kCBORMaxDepth = 16;

  ~CBORReader();

  // Reads and parses |input_data| into a CBORValue. If any one of the syntax
  // formats is violated -including unknown additional info and incomplete
  // CBOR data- then an empty optional is returned. Optional |error_code_out|
  // can be provided by the caller to obtain additional information about
  // decoding failures.
  static base::Optional<CBORValue> Read(const Bytes& input_data,
                                        DecoderError* error_code_out = nullptr,
                                        int max_nesting_level = kCBORMaxDepth);

  // Translates errors to human-readable error messages.
  static const char* ErrorCodeToString(DecoderError error_code);

 private:
  CBORReader(Bytes::const_iterator it, const Bytes::const_iterator end);
  base::Optional<CBORValue> DecodeCBOR(int max_nesting_level);
  bool ReadUnsignedInt(int additional_info, uint64_t* length);
  base::Optional<CBORValue> ReadBytes(uint64_t num_bytes);
  base::Optional<CBORValue> ReadString(uint64_t num_bytes);
  base::Optional<CBORValue> ReadCBORArray(uint64_t length,
                                          int max_nesting_level);
  base::Optional<CBORValue> ReadCBORMap(uint64_t length, int max_nesting_level);
  bool CanConsume(uint64_t bytes);
  void CheckExtraneousData();
  bool CheckDuplicateKey(const CBORValue& new_key, CBORValue::MapValue* map);
  bool HasValidUTF8Format(const std::string& string_data);
  bool CheckOutOfOrderKey(const CBORValue& new_key, CBORValue::MapValue* map);
  bool CheckUintEncodedByteLength(uint8_t additional_bytes, uint64_t uint_data);

  DecoderError GetErrorCode();

  Bytes::const_iterator it_;
  const Bytes::const_iterator end_;
  DecoderError error_code_;

  DISALLOW_COPY_AND_ASSIGN(CBORReader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_CBOR_CBOR_READER_H_
