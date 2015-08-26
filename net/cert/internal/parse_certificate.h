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
struct ParsedTbsCertificate;

// Parses a DER-encoded "Certificate" as specified by RFC 5280. Returns true on
// success and sets the results in |out|.
//
// Note that on success |out| aliases data from the input |certificate_tlv|.
// Hence the fields of the ParsedCertificate are only valid as long as
// |certificate_tlv| remains valid.
//
// On failure |out| has an undefined state. Some of its fields may have been
// updated during parsing, whereas others may not have been changed.
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

// Parses a DER-encoded "TBSCertificate" as specified by RFC 5280. Returns true
// on success and sets the results in |out|.
//
// Note that on success |out| aliases data from the input |tbs_tlv|.
// Hence the fields of the ParsedTbsCertificate are only valid as long as
// |tbs_tlv| remains valid.
//
// On failure |out| has an undefined state. Some of its fields may have been
// updated during parsing, whereas others may not have been changed.
//
// Refer to the per-field documentation of ParsedTbsCertificate for details on
// what validity checks parsing performs.
//
//       TBSCertificate  ::=  SEQUENCE  {
//            version         [0]  EXPLICIT Version DEFAULT v1,
//            serialNumber         CertificateSerialNumber,
//            signature            AlgorithmIdentifier,
//            issuer               Name,
//            validity             Validity,
//            subject              Name,
//            subjectPublicKeyInfo SubjectPublicKeyInfo,
//            issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
//                                 -- If present, version MUST be v2 or v3
//            subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
//                                 -- If present, version MUST be v2 or v3
//            extensions      [3]  EXPLICIT Extensions OPTIONAL
//                                 -- If present, version MUST be v3
//            }
NET_EXPORT bool ParseTbsCertificate(const der::Input& tbs_tlv,
                                    ParsedTbsCertificate* out)
    WARN_UNUSED_RESULT;

// Represents a "Version" from RFC 5280:
//         Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
enum class CertificateVersion {
  V1,
  V2,
  V3,
};

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
  //
  // This can be further parsed using ParseTbsCertificate().
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

// ParsedTbsCertificate contains pointers to the main fields of a DER-encoded
// RFC 5280 "TBSCertificate".
//
// ParsedTbsCertificate is expected to be filled by ParseTbsCertificate(), so
// subsequent field descriptions are in terms of what ParseTbsCertificate()
// sets.
struct NET_EXPORT ParsedTbsCertificate {
  ParsedTbsCertificate();
  ~ParsedTbsCertificate();

  // Corresponds with "version" from RFC 5280:
  //         version         [0]  EXPLICIT Version DEFAULT v1,
  //
  // Parsing guarantees that the version is one of v1, v2, or v3.
  CertificateVersion version = CertificateVersion::V1;

  // Corresponds with "serialNumber" from RFC 5280:
  //         serialNumber         CertificateSerialNumber,
  //
  // This field specifically contains the content bytes of the INTEGER. So for
  // instance if the serial number was 1000 then this would contain bytes
  // {0x03, 0xE8}.
  //
  // In addition to being a valid DER-encoded INTEGER, parsing guarantees that
  // the serial number is at most 20 bytes long. Parsing does NOT guarantee
  // that the integer is positive (might be zero or negative).
  der::Input serial_number;

  // Corresponds with "signatureAlgorithm" from RFC 5280:
  //         signatureAlgorithm   AlgorithmIdentifier,
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE.
  //
  // This can be further parsed using SignatureValue::CreateFromDer().
  der::Input signature_algorithm_tlv;

  // Corresponds with "issuer" from RFC 5280:
  //         issuer               Name,
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE.
  der::Input issuer_tlv;

  // Corresponds with "validity" from RFC 5280:
  //         validity             Validity,
  //
  // Where Validity is defined as:
  //
  //   Validity ::= SEQUENCE {
  //        notBefore      Time,
  //        notAfter       Time }
  //
  // Parsing guarantees that notBefore (validity_not_before) and notAfter
  // (validity_not_after) are valid DER-encoded dates, however it DOES NOT
  // gurantee anything about their values. For instance notAfter could be
  // before notBefore, or the dates could indicate an expired certificate.
  // Consumers are responsible for testing expiration.
  der::GeneralizedTime validity_not_before;
  der::GeneralizedTime validity_not_after;

  // Corresponds with "subject" from RFC 5280:
  //         subject              Name,
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE.
  der::Input subject_tlv;

  // Corresponds with "subjectPublicKeyInfo" from RFC 5280:
  //         subjectPublicKeyInfo SubjectPublicKeyInfo,
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE.
  der::Input spki_tlv;

  // Corresponds with "issuerUniqueID" from RFC 5280:
  //         issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                              -- If present, version MUST be v2 or v3
  //
  // Parsing guarantees that if issuer_unique_id is present it is a valid BIT
  // STRING, and that the version is either v2 or v3
  bool has_issuer_unique_id = false;
  der::BitString issuer_unique_id;

  // Corresponds with "subjectUniqueID" from RFC 5280:
  //         subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                              -- If present, version MUST be v2 or v3
  //
  // Parsing guarantees that if subject_unique_id is present it is a valid BIT
  // STRING, and that the version is either v2 or v3
  bool has_subject_unique_id = false;
  der::BitString subject_unique_id;

  // Corresponds with "extensions" from RFC 5280:
  //         extensions      [3]  EXPLICIT Extensions OPTIONAL
  //                              -- If present, version MUST be v3
  //
  //
  // This contains the full (unverified) Tag-Length-Value for a SEQUENCE. No
  // guarantees are made regarding the value of this SEQUENCE. (Note that the
  // EXPLICIT outer tag is stripped.)
  //
  // Parsing guarantees that if extensions is present the version is v3.
  bool has_extensions = false;
  der::Input extensions_tlv;
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_PARSE_CERTIFICATE_H_
