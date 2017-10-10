// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/cbor/cbor_values.h"

#include <new>
#include <utility>

#include "base/strings/string_util.h"

namespace content {

CBORValue::CBORValue() noexcept : type_(Type::NONE) {}

CBORValue::CBORValue(CBORValue&& that) noexcept {
  InternalMoveConstructFrom(std::move(that));
}

CBORValue::CBORValue(Type type) : type_(type) {
  // Initialize with the default value.
  switch (type_) {
    case Type::NONE:
      return;
    case Type::UNSIGNED:
      unsigned_value_ = 0;
      return;
    case Type::BYTESTRING:
      new (&bytestring_value_) BinaryValue();
      return;
    case Type::STRING:
      new (&string_value_) std::string();
      return;
    case Type::ARRAY:
      new (&array_value_) ArrayValue();
      return;
    case Type::MAP:
      new (&map_value_) MapValue();
      return;
  }
}

CBORValue::CBORValue(uint64_t in_unsigned)
    : type_(Type::UNSIGNED), unsigned_value_(in_unsigned) {}

CBORValue::CBORValue(const BinaryValue& in_bytes)
    : type_(Type::BYTESTRING), bytestring_value_(in_bytes) {}

CBORValue::CBORValue(BinaryValue&& in_bytes) noexcept
    : type_(Type::BYTESTRING), bytestring_value_(std::move(in_bytes)) {}

CBORValue::CBORValue(const char* in_string)
    : CBORValue(std::string(in_string)) {}

CBORValue::CBORValue(std::string&& in_string) noexcept
    : type_(Type::STRING), string_value_(std::move(in_string)) {
  DCHECK(base::IsStringUTF8(string_value_));
}

CBORValue::CBORValue(base::StringPiece in_string)
    : CBORValue(in_string.as_string()) {}

CBORValue::CBORValue(const ArrayValue& in_array)
    : type_(Type::ARRAY), array_value_() {
  array_value_.reserve(in_array.size());
  for (const auto& val : in_array)
    array_value_.emplace_back(val.Clone());
}

CBORValue::CBORValue(ArrayValue&& in_array) noexcept
    : type_(Type::ARRAY), array_value_(std::move(in_array)) {}

CBORValue::CBORValue(const MapValue& in_map) : type_(Type::MAP), map_value_() {
  map_value_.reserve(in_map.size());
  for (const auto& it : in_map)
    map_value_.emplace_hint(map_value_.end(), it.first, it.second.Clone());
}

CBORValue::CBORValue(MapValue&& in_map) noexcept
    : type_(Type::MAP), map_value_(std::move(in_map)) {}

CBORValue& CBORValue::operator=(CBORValue&& that) noexcept {
  InternalCleanup();
  InternalMoveConstructFrom(std::move(that));

  return *this;
}

CBORValue::~CBORValue() {
  InternalCleanup();
}

CBORValue CBORValue::Clone() const {
  switch (type_) {
    case Type::NONE:
      return CBORValue();
    case Type::UNSIGNED:
      return CBORValue(unsigned_value_);
    case Type::BYTESTRING:
      return CBORValue(bytestring_value_);
    case Type::STRING:
      return CBORValue(string_value_);
    case Type::ARRAY:
      return CBORValue(array_value_);
    case Type::MAP:
      return CBORValue(map_value_);
  }

  NOTREACHED();
  return CBORValue();
}

uint64_t CBORValue::GetUnsigned() const {
  CHECK(is_unsigned());
  return unsigned_value_;
}

const std::string& CBORValue::GetString() const {
  CHECK(is_string());
  return string_value_;
}

const CBORValue::BinaryValue& CBORValue::GetBytestring() const {
  CHECK(is_bytestring());
  return bytestring_value_;
}

const CBORValue::ArrayValue& CBORValue::GetArray() const {
  CHECK(is_array());
  return array_value_;
}

const CBORValue::MapValue& CBORValue::GetMap() const {
  CHECK(is_map());
  return map_value_;
}

void CBORValue::InternalMoveConstructFrom(CBORValue&& that) {
  type_ = that.type_;

  switch (type_) {
    case Type::NONE:
      return;
    case Type::UNSIGNED:
      unsigned_value_ = that.unsigned_value_;
      return;
    case Type::BYTESTRING:
      new (&bytestring_value_) BinaryValue(std::move(that.bytestring_value_));
      return;
    case Type::STRING:
      new (&string_value_) std::string(std::move(that.string_value_));
      return;
    case Type::ARRAY:
      new (&array_value_) ArrayValue(std::move(that.array_value_));
      return;
    case Type::MAP:
      new (&map_value_) MapValue(std::move(that.map_value_));
      return;
  }
}

void CBORValue::InternalCleanup() {
  switch (type_) {
    case Type::NONE:
    case Type::UNSIGNED:
      // Nothing to do
      break;
      ;
    case Type::BYTESTRING:
      bytestring_value_.~BinaryValue();
      break;
    case Type::STRING:
      string_value_.~basic_string();
      break;
    case Type::ARRAY:
      array_value_.~ArrayValue();
      break;
    case Type::MAP:
      map_value_.~MapValue();
      break;
  }
  type_ = Type::NONE;
}

}  // namespace content
