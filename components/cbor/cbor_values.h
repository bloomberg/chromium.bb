// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CBOR_CBOR_VALUES_H_
#define COMPONENTS_CBOR_CBOR_VALUES_H_

#include <stdint.h>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "components/cbor/cbor_export.h"

namespace cbor {

// A class for Concise Binary Object Representation (CBOR) values.
// This does not support:
//  * Floating-point numbers.
//  * Indefinite-length encodings.
class CBOR_EXPORT CBORValue {
 public:
  struct CTAPLess {
    // Comparison predicate to order keys in a dictionary as required by the
    // Client-to-Authenticator Protocol (CTAP) spec 2.0.
    //
    // The sort order defined in CTAP is:
    //   • If the major types are different, the one with the lower value in
    //     numerical order sorts earlier.
    //   • If two keys have different lengths, the shorter one sorts earlier.
    //   • If two keys have the same length, the one with the lower value in
    //     (byte-wise) lexical order sorts earlier.
    //
    // See section 6 of https://fidoalliance.org/specs/fido-v2.0-rd-20170927/
    // fido-client-to-authenticator-protocol-v2.0-rd-20170927.html.
    //
    // THE CTAP SORT ORDER IMPLEMENTED HERE DIFFERS FROM THE CANONICAL CBOR
    // ORDER defined in https://tools.ietf.org/html/rfc7049#section-3.9, in that
    // the latter sorts purely by serialised key and doesn't specify that major
    // types are compared first. Thus the shortest key sorts first by the RFC
    // rules (irrespective of the major type), but may not by CTAP rules.
    bool operator()(const CBORValue& a, const CBORValue& b) const {
      DCHECK((a.is_integer() || a.is_string()) &&
             (b.is_integer() || b.is_string()));
      if (a.type() != b.type())
        return a.type() < b.type();
      switch (a.type()) {
        case Type::UNSIGNED:
          return a.GetInteger() < b.GetInteger();
        case Type::NEGATIVE:
          return a.GetInteger() > b.GetInteger();
        case Type::STRING: {
          const auto& a_str = a.GetString();
          const size_t a_length = a_str.size();
          const auto& b_str = b.GetString();
          const size_t b_length = b_str.size();
          return std::tie(a_length, a_str) < std::tie(b_length, b_str);
        }
        default:
          break;
      }

      NOTREACHED();
      return false;
    }

    using is_transparent = void;
  };

  using BinaryValue = std::vector<uint8_t>;
  using ArrayValue = std::vector<CBORValue>;
  using MapValue = base::flat_map<CBORValue, CBORValue, CTAPLess>;

  enum class Type {
    UNSIGNED = 0,
    NEGATIVE = 1,
    BYTE_STRING = 2,
    STRING = 3,
    ARRAY = 4,
    MAP = 5,
    SIMPLE_VALUE = 7,
    NONE = -1,
  };

  enum class SimpleValue {
    FALSE_VALUE = 20,
    TRUE_VALUE = 21,
    NULL_VALUE = 22,
    UNDEFINED = 23,
  };

  CBORValue(CBORValue&& that) noexcept;
  CBORValue() noexcept;  // A NONE value.

  explicit CBORValue(Type type);
  explicit CBORValue(int integer_value);
  explicit CBORValue(int64_t integer_value);
  explicit CBORValue(uint64_t integer_value) = delete;

  explicit CBORValue(const BinaryValue& in_bytes);
  explicit CBORValue(BinaryValue&& in_bytes) noexcept;

  explicit CBORValue(const char* in_string);
  explicit CBORValue(std::string&& in_string) noexcept;
  explicit CBORValue(base::StringPiece in_string);

  explicit CBORValue(const ArrayValue& in_array);
  explicit CBORValue(ArrayValue&& in_array) noexcept;

  explicit CBORValue(const MapValue& in_map);
  explicit CBORValue(MapValue&& in_map) noexcept;

  explicit CBORValue(SimpleValue in_simple);

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
  bool is_negative() const { return type() == Type::NEGATIVE; }
  bool is_integer() const { return is_unsigned() || is_negative(); }
  bool is_bytestring() const { return type() == Type::BYTE_STRING; }
  bool is_string() const { return type() == Type::STRING; }
  bool is_array() const { return type() == Type::ARRAY; }
  bool is_map() const { return type() == Type::MAP; }
  bool is_simple() const { return type() == Type::SIMPLE_VALUE; }

  // These will all fatally assert if the type doesn't match.
  SimpleValue GetSimpleValue() const;
  const int64_t& GetInteger() const;
  const int64_t& GetUnsigned() const;
  const int64_t& GetNegative() const;
  const BinaryValue& GetBytestring() const;
  // Returned string may contain NUL characters.
  const std::string& GetString() const;
  const ArrayValue& GetArray() const;
  const MapValue& GetMap() const;

 private:
  Type type_;

  union {
    SimpleValue simple_value_;
    int64_t integer_value_;
    BinaryValue bytestring_value_;
    std::string string_value_;
    ArrayValue array_value_;
    MapValue map_value_;
  };

  void InternalMoveConstructFrom(CBORValue&& that);
  void InternalCleanup();

  DISALLOW_COPY_AND_ASSIGN(CBORValue);
};
}  // namespace cbor

#endif  // COMPONENTS_CBOR_CBOR_VALUES_H_
