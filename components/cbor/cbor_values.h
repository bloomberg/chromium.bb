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
  struct Less {
    // Comparison predicate to order keys in a dictionary as required by the
    // canonical CBOR order defined in
    // https://tools.ietf.org/html/rfc7049#section-3.9
    // TODO(808022): Clarify where this stands.
    bool operator()(const CBORValue& a, const CBORValue& b) const {
      // The current implementation only supports integer, text string,
      // and byte string keys.
      DCHECK((a.is_integer() || a.is_string() || a.is_bytestring()) &&
             (b.is_integer() || b.is_string() || b.is_bytestring()));

      // Below text from https://tools.ietf.org/html/rfc7049 errata 4409:
      // *  If the major types are different, the one with the lower value
      //    in numerical order sorts earlier.
      if (a.type() != b.type())
        return a.type() < b.type();

      // *  If two keys have different lengths, the shorter one sorts
      //    earlier;
      // *  If two keys have the same length, the one with the lower value
      //    in (byte-wise) lexical order sorts earlier.
      switch (a.type()) {
        case Type::UNSIGNED:
          // For unsigned integers, the smaller value has shorter length,
          // and (byte-wise) lexical representation.
          return a.GetInteger() < b.GetInteger();
        case Type::NEGATIVE:
          // For negative integers, the value closer to zero has shorter length,
          // and (byte-wise) lexical representation.
          return a.GetInteger() > b.GetInteger();
        case Type::STRING: {
          const auto& a_str = a.GetString();
          const size_t a_length = a_str.size();
          const auto& b_str = b.GetString();
          const size_t b_length = b_str.size();
          return std::tie(a_length, a_str) < std::tie(b_length, b_str);
        }
        case Type::BYTE_STRING: {
          const auto& a_str = a.GetBytestring();
          const size_t a_length = a_str.size();
          const auto& b_str = b.GetBytestring();
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
  using MapValue = base::flat_map<CBORValue, CBORValue, Less>;

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

  explicit CBORValue(SimpleValue in_simple);
  explicit CBORValue(bool boolean_value);

  explicit CBORValue(int integer_value);
  explicit CBORValue(int64_t integer_value);
  explicit CBORValue(uint64_t integer_value) = delete;

  explicit CBORValue(const BinaryValue& in_bytes);
  explicit CBORValue(BinaryValue&& in_bytes) noexcept;

  explicit CBORValue(const char* in_string, Type type = Type::STRING);
  explicit CBORValue(std::string&& in_string,
                     Type type = Type::STRING) noexcept;
  explicit CBORValue(base::StringPiece in_string, Type type = Type::STRING);

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
  bool is_simple() const { return type() == Type::SIMPLE_VALUE; }
  bool is_bool() const {
    return is_simple() && (simple_value_ == SimpleValue::TRUE_VALUE ||
                           simple_value_ == SimpleValue::FALSE_VALUE);
  }
  bool is_unsigned() const { return type() == Type::UNSIGNED; }
  bool is_negative() const { return type() == Type::NEGATIVE; }
  bool is_integer() const { return is_unsigned() || is_negative(); }
  bool is_bytestring() const { return type() == Type::BYTE_STRING; }
  bool is_string() const { return type() == Type::STRING; }
  bool is_array() const { return type() == Type::ARRAY; }
  bool is_map() const { return type() == Type::MAP; }

  // These will all fatally assert if the type doesn't match.
  SimpleValue GetSimpleValue() const;
  bool GetBool() const;
  const int64_t& GetInteger() const;
  const int64_t& GetUnsigned() const;
  const int64_t& GetNegative() const;
  const BinaryValue& GetBytestring() const;
  base::StringPiece GetBytestringAsString() const;
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
