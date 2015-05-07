// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DER_PARSE_VALUES_H_
#define NET_DER_PARSE_VALUES_H_

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

// Reads a DER-encoded ASN.1 INTEGER value from |in| and puts the resulting
// value in |out|. ASN.1 INTEGERs are arbitrary precision; this function is
// provided as a convenience when the caller knows that the value is unsigned
// and is between 0 and 2^63-1. This function does not support the full range of
// uint64_t. This function returns false if the value is too big to fit in a
// uint64_t, is negative, or if there is an error reading the integer.
NET_EXPORT bool ParseUint64(const Input& in, uint64_t* out) WARN_UNUSED_RESULT;

struct GeneralizedTime {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
};

NET_EXPORT_PRIVATE bool operator<(const GeneralizedTime& lhs,
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
// be parsed sucessfully. This function is even more restrictive than the
// DER rules - it follows the rules from RFC5280, which does not allow for
// fractional seconds.
NET_EXPORT bool ParseGeneralizedTime(const Input& in,
                                     GeneralizedTime* out) WARN_UNUSED_RESULT;

}  // namespace der

}  // namespace net

#endif  // NET_DER_PARSE_VALUES_H_
