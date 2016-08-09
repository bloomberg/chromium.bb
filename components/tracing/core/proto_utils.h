// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_CORE_PROTO_UTILS_H_
#define COMPONENTS_TRACING_CORE_PROTO_UTILS_H_

#include <inttypes.h>

#include <type_traits>

#include "base/logging.h"

namespace tracing {
namespace v2 {
namespace proto {

// TODO(kraynov): Change namespace to tracing::proto::internal.
// This is required in headers and it's too low-level to be exposed.

// See https://developers.google.com/protocol-buffers/docs/encoding wire types.

enum : uint32_t {
  kFieldTypeVarInt = 0,
  kFieldTypeFixed64 = 1,
  kFieldTypeLengthDelimited = 2,
  kFieldTypeFixed32 = 5,
};

// Maximum message size supported: 256 MiB (4 x 7-bit due to varint encoding).
constexpr size_t kMessageLengthFieldSize = 4;
constexpr size_t kMaxMessageLength = (1u << (kMessageLengthFieldSize * 7)) - 1;

// Field tag is encoded as 32-bit varint (5 bytes at most).
// Largest value of simple (not length-delimited) field is 64-bit varint
// (10 bytes at most). 15 bytes buffer is enough to store a simple field.
constexpr size_t kMaxTagEncodedSize = 5;
constexpr size_t kMaxSimpleFieldEncodedSize = 15;

// Proto types: (int|uint|sint)(32|64), bool, enum.
constexpr uint32_t MakeTagVarInt(uint32_t field_id) {
  return (field_id << 3) | kFieldTypeVarInt;
}

// Proto types: fixed64, sfixed64, fixed32, sfixed32, double, float.
template <typename T>
constexpr uint32_t MakeTagFixed(uint32_t field_id) {
  static_assert(sizeof(T) == 8 || sizeof(T) == 4, "Value must be 4 or 8 bytes");
  return (field_id << 3) |
         (sizeof(T) == 8 ? kFieldTypeFixed64 : kFieldTypeFixed32);
}

// Proto types: string, bytes, embedded messages.
constexpr uint32_t MakeTagLengthDelimited(uint32_t field_id) {
  return (field_id << 3) | kFieldTypeLengthDelimited;
}

// Proto tipes: sint64, sint32.
template <typename T>
inline typename std::make_unsigned<T>::type ZigZagEncode(T value) {
  return (value << 1) ^ (value >> (sizeof(T) * 8 - 1));
}

template <typename T>
inline uint8_t* WriteVarInt(T value, uint8_t* target) {
  // Avoid arithmetic (sign expanding) shifts.
  typedef typename std::make_unsigned<T>::type unsigned_T;
  unsigned_T unsigned_value = static_cast<unsigned_T>(value);

  while (unsigned_value >= 0x80) {
    *target++ = static_cast<uint8_t>(unsigned_value) | 0x80;
    unsigned_value >>= 7;
  }
  *target = static_cast<uint8_t>(unsigned_value);
  return target + 1;
}

// Writes a fixed-size redundant encoding of the given |value|. This is
// used to backfill fixed-size reservations for the length of field using a
// non-canonical varint encoding (e.g. \x81\x80\x80\x00 instead of \x01).
// See https://github.com/google/protobuf/issues/1530.
// In particular, this is used for nested messages. The size of a nested message
// is not known until all its field have been written. |kMessageLengthFieldSize|
// bytes are reserved to encode the size field and backfilled at the end.
inline void WriteRedundantLength(uint32_t value, uint8_t* buf) {
  DCHECK_LE(value, kMaxMessageLength) << "Message is too long";
  for (size_t i = 0; i < kMessageLengthFieldSize; ++i) {
    const uint8_t msb = (i < kMessageLengthFieldSize - 1) ? 0x80 : 0;
    buf[i] = static_cast<uint8_t>(value) | msb;
    value >>= 7;
  }
}

}  // namespace proto
}  // namespace v2
}  // namespace tracing

#endif  // COMPONENTS_TRACING_CORE_PROTO_UTILS_H_
