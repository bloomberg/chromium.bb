// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_COMMON_EXPANDED_VALUE_BASE_H_
#define MEDIA_CAST_COMMON_EXPANDED_VALUE_BASE_H_

#include <stdint.h>

#include <limits>

namespace media {
namespace cast {

// Abstract base template class for common "sequence value" data types such as
// RtpTimeTicks, FrameId, or PacketId which generally increment/decrement in
// predictable amounts as media is streamed, and which often need to be reliably
// truncated and re-expanded for over-the-wire transmission.
//
// FullPrecisionInteger should be a signed integer POD type that is of
// sufficiently high precision to never wrap-around in the system.  Subclass is
// the class inheriting the common functionality provided in this template, and
// is used to provide operator overloads.  The Subclass must friend this class
// to enable these operator overloads.
//
// Please see RtpTimeTicks and unit test code for examples of how to define
// Subclasses and add features specific to their concrete data type, and how to
// use data types derived from ExpandedValueBase.  For example, a RtpTimeTicks
// adds math operators consisting of the meaningful and valid set of operations
// allowed for doing "time math."  On the other hand, FrameId only adds math
// operators for incrementing/decrementing since multiplication and division are
// meaningless.
template <typename FullPrecisionInteger, class Subclass>
class ExpandedValueBase {
  static_assert(std::numeric_limits<FullPrecisionInteger>::is_signed,
                "FullPrecisionInteger must be a signed integer.");
  static_assert(std::numeric_limits<FullPrecisionInteger>::is_integer,
                "FullPrecisionInteger must be a signed integer.");

 public:
  // Methods that return the lower bits of this value.  This should only be used
  // for serializing/wire-formatting, and not to subvert the restricted set of
  // operators allowed on this data type.
  uint8_t lower_8_bits() const { return static_cast<uint8_t>(value_); }
  uint16_t lower_16_bits() const { return static_cast<uint16_t>(value_); }
  uint32_t lower_32_bits() const { return static_cast<uint32_t>(value_); }

  // Compute the value closest to |this| value whose lower bits are those of
  // |x|.  The result is always within |max_distance_for_expansion()| of |this|
  // value.
  //
  // The purpose of this method is to re-instantiate an original value from its
  // truncated form, usually when deserializing off-the-wire.  Therefore, it is
  // always important to call this method on an instance known to be close in
  // distance to |x|.
  template <typename ShortUnsigned>
  Subclass Expand(ShortUnsigned x) const {
    static_assert(!std::numeric_limits<ShortUnsigned>::is_signed,
                  "|x| must be an unsigned integer.");
    static_assert(std::numeric_limits<ShortUnsigned>::is_integer,
                  "|x| must be an unsigned integer.");
    static_assert(sizeof(ShortUnsigned) <= sizeof(FullPrecisionInteger),
                  "|x| must fit within the FullPrecisionInteger.");

    if (sizeof(ShortUnsigned) < sizeof(FullPrecisionInteger)) {
      // Initially, the |result| is composed of upper bits from |value_| and
      // lower bits from |x|.
      const FullPrecisionInteger short_max =
          std::numeric_limits<ShortUnsigned>::max();
      FullPrecisionInteger result = (value_ & ~short_max) | x;

      // Determine whether the shorter integer type encountered wrap-around, and
      // increment/decrement the upper bits by one to account for that.
      const FullPrecisionInteger diff = result - value_;
      const FullPrecisionInteger pivot =
          max_distance_for_expansion<ShortUnsigned>();
      if (diff > pivot)
        result -= short_max + 1;
      else if (diff < -(pivot + 1))
        result += short_max + 1;
      return Subclass(result);
    } else {
      return Subclass(x);
    }
  }

  // Comparison operators.
  bool operator==(Subclass rhs) const { return value_ == rhs.value_; }
  bool operator!=(Subclass rhs) const { return value_ != rhs.value_; }
  bool operator<(Subclass rhs) const { return value_ < rhs.value_; }
  bool operator>(Subclass rhs) const { return value_ > rhs.value_; }
  bool operator<=(Subclass rhs) const { return value_ <= rhs.value_; }
  bool operator>=(Subclass rhs) const { return value_ >= rhs.value_; }

  // (De)Serialize for transmission over IPC.  Do not use these to subvert the
  // valid set of operators allowed by this class or its Subclass.
  uint64_t SerializeForIPC() const {
    static_assert(sizeof(uint64_t) >= sizeof(FullPrecisionInteger),
                  "Cannot serialize FullPrecisionInteger into an uint64_t.");
    return static_cast<uint64_t>(value_);
  }
  static Subclass DeserializeForIPC(uint64_t serialized) {
    return Subclass(static_cast<FullPrecisionInteger>(serialized));
  }

  // Design limit: Values that are truncated to the ShortUnsigned type must be
  // no more than this maximum distance from each other in order to ensure the
  // original value can be determined correctly.
  template <typename ShortUnsigned>
  static FullPrecisionInteger max_distance_for_expansion() {
    return std::numeric_limits<ShortUnsigned>::max() / 2;
  }

 protected:
  // Only subclasses are permitted to instantiate directly.
  explicit ExpandedValueBase(FullPrecisionInteger value) : value_(value) {}

  FullPrecisionInteger value_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_COMMON_EXPANDED_VALUE_BASE_H_
