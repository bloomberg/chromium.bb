// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_PARSE_CERTIFICATE_H_
#define NET_CERT_INTERNAL_PARSE_CERTIFICATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"

namespace net {

struct ParsedCertificate;

// Parses a DER-encoded "Certificate" as specified by RFC 5280. Returns true on
// success and sets the results in |out|.
//
// Note that on success |out| aliases data from the input |certificate_tlv|.
// Hence the fields of the ParsedCertificate are only valid as long as
// |certificate_tlv| remains valid.
//
// On failure |out| has an undefined state. Some of its fields may have been
// updated during parsing, whereas others were not changed.
//
// Refer to the per-field documention of the ParsedCertificate structure for
// details on what validity checks parsing performs.
//
//       Certificate  ::=  SEQUENCE  {
//            tbsCertificate       TBSCertificate,
//            signatureAlgorithm   AlgorithmIdentifier,
//            signatureValue       BIT STRING  }
NET_EXPORT bool ParseCertificate(const der::Input& certificate_tlv,
                                 ParsedCertificate* out) WARN_UNUSED_RESULT;

// ParsedCertificate contains pointers to the main fields of a DER-encoded RFC
// 5280 "Certificate".
//
// ParsedCertificate is expected to be filled by ParseCertificate(), so
// subsequent field descriptions are in terms of what ParseCertificate() sets.
struct NET_EXPORT ParsedCertificate {
  // Corresponds with "tbsCertificate" from RFC 5280:
  //         tbsCertificate       TBSCertificate,
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE.
  der::Input tbs_certificate_tlv;

  // Corresponds with "signatureAlgorithm" from RFC 5280:
  //         signatureAlgorithm   AlgorithmIdentifier,
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE.
  //
  // This can be further parsed using SignatureValue::CreateFromDer().
  der::Input signature_algorithm_tlv;

  // Corresponds with "signatureValue" from RFC 5280:
  //         signatureValue       BIT STRING  }
  //
  // Parsing guarantees that this is a valid BIT STRING.
  der::BitString signature_value;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_PARSE_CERTIFICATE_H_
