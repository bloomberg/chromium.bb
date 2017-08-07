// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DER_PARSE_VALUES_H_
#define NET_DER_PARSE_VALUES_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/der/input.h"

namespace net {

namespace der {

// Reads a DER-encoded ASN.1 BOOLEAN value from |in| and puts the resulting
// value in |out|. Returns whether the encoded value could successfully be
// read.
NET_EXPORT bool ParseBool(const Input& in, bool* out) WARN_UNUSED_RESULT;

// Like ParseBool, except it is more relaxed in what inputs it accepts: Any
// value that is a valid BER encoding will be parsed successfully.
NET_EXPORT bool ParseBoolRelaxed(const Input& in, bool* out) WARN_UNUSED_RESULT;

// Checks the validity of a DER-encoded ASN.1 INTEGER value from |in|, and
// determines the sign of the number. Returns true on success and
// fills |negative|. Otherwise returns false and does not modify the out
// parameter.
//
//    in: The value portion of an INTEGER.
//    negative: Out parameter that is set to true if the number is negative
//        and false otherwise (zero is non-negative).
NET_EXPORT bool IsValidInteger(const Input& in,
                               bool* negative) WARN_UNUSED_RESULT;

// Reads a DER-encoded ASN.1 INTEGER value from |in| and puts the resulting
// value in |out|. ASN.1 INTEGERs are arbitrary precision; this function is
// provided as a convenience when the caller knows that the value is unsigned
// and is between 0 and 2^64-1. This function returns false if the value is too
// big to fit in a uint64_t, is negative, or if there is an error reading the
// integer.
NET_EXPORT bool ParseUint64(const Input& in, uint64_t* out) WARN_UNUSED_RESULT;

// Same as ParseUint64() but for a uint8_t.
NET_EXPORT bool ParseUint8(const Input& in, uint8_t* out) WARN_UNUSED_RESULT;

// The BitString class is a helper for representing a valid parsed BIT STRING.
//
// * The bits are ordered within each octet of bytes() from most to least
//   significant, as in the DER encoding.
//
// * There may be at most 7 unused bits.
class NET_EXPORT BitString {
 public:
  BitString() : unused_bits_(0) {}

  // |unused_bits| represents the number of bits in the last octet of |bytes|,
  // starting from the least significant bit, that are unused. It MUST be < 8.
  // And if bytes is empty, then it MUST be 0.
  BitString(const Input& bytes, uint8_t unused_bits);

  const Input& bytes() const { return bytes_; }
  uint8_t unused_bits() const { return unused_bits_; }

  // Returns true if the bit string contains 1 at the specified position.
  // Otherwise returns false.
  //
  // A return value of false can mean either:
  //  * The bit value at |bit_index| is 0.
  //  * There is no bit at |bit_index| (index is beyond the end).
  bool AssertsBit(size_t bit_index) const WARN_UNUSED_RESULT;

 private:
  Input bytes_;
  uint8_t unused_bits_;

  // Default assignment and copy constructor are OK.
};

// Reads a DER-encoded ASN.1 BIT STRING value from |in| and puts the resulting
// octet string and number of unused bits into |bit_string|
//
// Returns true on success, otherwise returns false and does not modify the
// out-parameters.
NET_EXPORT bool ParseBitString(const Input& in,
                               BitString* bit_string) WARN_UNUSED_RESULT;

struct NET_EXPORT GeneralizedTime {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;

  // Returns true if the value is in UTCTime's range.
  bool InUTCTimeRange() const;
};

NET_EXPORT_PRIVATE bool operator<(const GeneralizedTime& lhs,
                                  const GeneralizedTime& rhs);
NET_EXPORT_PRIVATE bool operator<=(const GeneralizedTime& lhs,
                                   const GeneralizedTime& rhs);
NET_EXPORT_PRIVATE bool operator>(const GeneralizedTime& lhs,
                                  const GeneralizedTime& rhs);
NET_EXPORT_PRIVATE bool operator>=(const GeneralizedTime& lhs,
                                   const GeneralizedTime& rhs);

// Reads a DER-encoded ASN.1 UTCTime value from |in| and puts the resulting
// value in |out|, returning true if the UTCTime could be parsed successfully.
NET_EXPORT bool ParseUTCTime(const Input& in,
                             GeneralizedTime* out) WARN_UNUSED_RESULT;

// Like ParseUTCTime, but it is more lenient in what is accepted. DER requires
// a UTCTime to be in the format YYMMDDhhmmssZ; this function will accept both
// that and YYMMDDhhmmZ, which is a valid BER encoding of a UTCTime which
// sometimes incorrectly appears in X.509 certificates.
NET_EXPORT bool ParseUTCTimeRelaxed(const Input& in,
                                    GeneralizedTime* out) WARN_UNUSED_RESULT;

// Reads a DER-encoded ASN.1 GeneralizedTime value from |in| and puts the
// resulting value in |out|, returning true if the GeneralizedTime could
// be parsed successfully. This function is even more restrictive than the
// DER rules - it follows the rules from RFC5280, which does not allow for
// fractional seconds.
NET_EXPORT bool ParseGeneralizedTime(const Input& in,
                                     GeneralizedTime* out) WARN_UNUSED_RESULT;

}  // namespace der

}  // namespace net

#endif  // NET_DER_PARSE_VALUES_H_
