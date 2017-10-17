// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_CBOR_CBOR_VALUES_H_
#define CONTENT_BROWSER_WEBAUTH_CBOR_CBOR_VALUES_H_

#include <stdint.h>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
#include "content/common/content_export.h"

namespace content {

// A class for Concise Binary Object Representation (CBOR) values.
// This does not support:
//  * Negative integers.
//  * Floating-point numbers.
//  * Indefinite-length encodings.
class CONTENT_EXPORT CBORValue {
 public:
  struct CTAPLess {
    // Comparison predicate to order keys in a dictionary as required by the
    // Client-to-Authenticator Protocol (CTAP) spec 2.0.
    //
    // The sort order defined in CTAP is:
    //   • If the major types are different, the one with the lower value in
    //     numerical order sorts earlier. (Moot in this code because all keys
    //     are strings.)
    //   • If two keys have different lengths, the shorter one sorts earlier.
    //   • If two keys have the same length, the one with the lower value in
    //     (byte-wise) lexical order sorts earlier.
    //
    // See section 6 of https://fidoalliance.org/specs/fido-v2.0-rd-20170927/
    // fido-client-to-authenticator-protocol-v2.0-rd-20170927.html and
    // https://tools.ietf.org/html/rfc7049#section-3.9 also.
    bool operator()(const std::string& a, const std::string& b) const {
      const size_t a_size = a.size();
      const size_t b_size = b.size();
      return std::tie(a_size, a) < std::tie(b_size, b);
    }

    bool operator()(const char* a, const std::string& b) const {
      return operator()(std::string(a), b);
    }

    bool operator()(const std::string& a, const char* b) const {
      return operator()(a, std::string(b));
    }

    using is_transparent = void;
  };

  using BinaryValue = std::vector<uint8_t>;
  using ArrayValue = std::vector<CBORValue>;
  using MapValue = base::flat_map<std::string, CBORValue, CTAPLess>;

  enum class Type {
    NONE,
    UNSIGNED,
    BYTESTRING,
    STRING,
    ARRAY,
    MAP,
  };

  CBORValue(CBORValue&& that) noexcept;
  CBORValue() noexcept;  // A NONE value.

  explicit CBORValue(Type type);
  explicit CBORValue(uint64_t in_unsigned);

  explicit CBORValue(const BinaryValue& in_bytes);
  explicit CBORValue(BinaryValue&& in_bytes) noexcept;

  explicit CBORValue(const char* in_string);
  explicit CBORValue(std::string&& in_string) noexcept;
  explicit CBORValue(base::StringPiece in_string);

  explicit CBORValue(const ArrayValue& in_array);
  explicit CBORValue(ArrayValue&& in_array) noexcept;

  explicit CBORValue(const MapValue& in_map);
  explicit CBORValue(MapValue&& in_map) noexcept;

  CBORValue& operator=(CBORValue&& that) noexcept;

  ~CBORValue();

  // CBORValue's copy constructor and copy assignment operator are deleted.
  // Use this to obtain a deep copy explicitly.
  CBORValue Clone() const;

  // Returns the type of the value stored by the current Value object.
  Type type() const { return type_; }

  // Returns true if the current object represents a given type.
  bool is_type(Type type) const { return type == type_; }
  bool is_none() const { return type() == Type::NONE; }
  bool is_unsigned() const { return type() == Type::UNSIGNED; }
  bool is_bytestring() const { return type() == Type::BYTESTRING; }
  bool is_string() const { return type() == Type::STRING; }
  bool is_array() const { return type() == Type::ARRAY; }
  bool is_map() const { return type() == Type::MAP; }

  // These will all fatally assert if the type doesn't match.
  uint64_t GetUnsigned() const;
  const BinaryValue& GetBytestring() const;
  const std::string& GetString() const;
  const ArrayValue& GetArray() const;
  const MapValue& GetMap() const;

 private:
  Type type_;

  union {
    uint64_t unsigned_value_;
    BinaryValue bytestring_value_;
    std::string string_value_;
    ArrayValue array_value_;
    MapValue map_value_;
  };

  void InternalMoveConstructFrom(CBORValue&& that);
  void InternalCleanup();

  DISALLOW_COPY_AND_ASSIGN(CBORValue);
};
}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_CBOR_CBOR_VALUES_H_
