// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_CORE_PROTO_UTILS_H_
#define COMPONENTS_TRACING_CORE_PROTO_UTILS_H_

#include <inttypes.h>

#include <type_traits>

#include "base/logging.h"
#include "base/macros.h"
#include "components/tracing/tracing_export.h"

namespace tracing {
namespace v2 {
namespace proto {

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

// Returns the number of bytes sufficient to encode the largest
// |int_size_in_bits|-bits integer using a non-redundant varint encoding.
template <typename T>
constexpr size_t GetMaxVarIntEncodedSize() {
  return (sizeof(T) * 8 + 6) / 7;
}

// Variable-length field types: (s)int32, (s)int64, bool, enum.
inline uint32_t MakeTagVarInt(uint32_t field_id) {
  return (field_id << 3) | kFieldTypeVarInt;
}

// Length-limited field types: string, bytes, embedded messages.
inline uint32_t MakeTagLengthDelimited(uint32_t field_id) {
  return (field_id << 3) | kFieldTypeLengthDelimited;
}

// 32-bit fixed-length field types: fixed32, sfixed32, float.
inline uint32_t MakeTagFixed32(uint32_t field_id) {
  return (field_id << 3) | kFieldTypeFixed32;
}

// 64-bit fixed-length field types: fixed64, sfixed64, double.
inline uint32_t MakeTagFixed64(uint32_t field_id) {
  return (field_id << 3) | kFieldTypeFixed64;
}

template <typename T>
inline uint8_t* WriteVarIntInternal(T value, uint8_t* target) {
  static_assert(std::is_unsigned<T>::value, "value must be unsigned");
  while (value >= 0x80) {
    *target++ = static_cast<uint8_t>(value | 0x80);
    value >>= 7;
  }
  *target = static_cast<uint8_t>(value);
  return target + 1;
}

inline uint8_t* WriteVarIntU32(uint32_t value, uint8_t* target) {
  return WriteVarIntInternal<uint32_t>(value, target);
}

inline uint8_t* WriteVarIntU64(uint64_t value, uint8_t* target) {
  return WriteVarIntInternal<uint64_t>(value, target);
}

// TODO(kraynov): add support for signed integers and zig-zag encoding.

// Writes a fixed-size redundant encoding of the given |value|. This is
// used to backfill fixed-size reservations for the length field using a
// non-canonical varint encoding (e.g. \x81\x80\x80\x00 instead of \x01).
// See https://github.com/google/protobuf/issues/1530.
// In particular, this is used for nested messages. The size of a nested message
// is not known until all its field have been written. A fixed amount of bytes
// is reserved to encode the size field and backfilled at the end.
template <size_t LENGTH>
void WriteRedundantVarIntU32(uint32_t value, uint8_t* buf) {
  for (size_t i = 0; i < LENGTH; ++i) {
    const uint8_t msb = (i < LENGTH - 1) ? 0x80 : 0;
    buf[i] = static_cast<uint8_t>((value & 0x7F) | msb);
    value >>= 7;
  }
  DCHECK_EQ(0u, value) << "Buffer too short to encode the given value";
}

}  // namespace proto
}  // namespace v2
}  // namespace tracing

#endif  // COMPONENTS_TRACING_CORE_PROTO_UTILS_H_
