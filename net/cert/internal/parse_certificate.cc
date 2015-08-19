// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/parse_certificate.h"

#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"

namespace net {

namespace {

// Returns true if |input| is a SEQUENCE and nothing else.
WARN_UNUSED_RESULT bool IsSequenceTLV(const der::Input& input) {
  der::Parser parser(input);
  der::Parser unused_sequence_parser;
  if (!parser.ReadSequence(&unused_sequence_parser))
    return false;
  // Should by a single SEQUENCE by definition of the function.
  return !parser.HasMore();
}

// Reads a SEQUENCE from |parser| and writes the full tag-length-value into
// |out|. On failure |parser| may or may not have been advanced.
WARN_UNUSED_RESULT bool ReadSequenceTLV(der::Parser* parser, der::Input* out) {
  return parser->ReadRawTLV(out) && IsSequenceTLV(*out);
}

// Parses a Version according to RFC 5280:
//
//     Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
//
// No value other that v1, v2, or v3 is allowed (and if given will fail). RFC
// 5280 minimally requires the handling of v3 (and overwhelmingly these are the
// certificate versions in use today):
//
//     Implementations SHOULD be prepared to accept any version certificate.
//     At a minimum, conforming implementations MUST recognize version 3
//     certificates.
WARN_UNUSED_RESULT bool ParseVersion(const der::Input& in,
                                     CertificateVersion* version) {
  der::Parser parser(in);
  uint64_t version64;
  if (!parser.ReadUint64(&version64))
    return false;

  switch (version64) {
    case 0:
      *version = CertificateVersion::V1;
      break;
    case 1:
      *version = CertificateVersion::V2;
      break;
    case 2:
      *version = CertificateVersion::V3;
      break;
    default:
      // Don't allow any other version identifier.
      return false;
  }

  // By definition the input to this function was a single INTEGER, so there
  // shouldn't be anything else after it.
  return !parser.HasMore();
}

// Returns true if the given serial number (CertificateSerialNumber in RFC 5280)
// is valid:
//
//    CertificateSerialNumber  ::=  INTEGER
//
// The input to this function is the (unverified) value octets of the INTEGER.
// This function will verify that:
//
//   * The octets are a valid DER-encoding of an INTEGER (for instance, minimal
//     encoding length).
//
//   * No more than 20 octets are used.
//
// Note that it DOES NOT reject non-positive values (zero or negative).
//
// For reference, here is what RFC 5280 section 4.1.2.2 says:
//
//     Given the uniqueness requirements above, serial numbers can be
//     expected to contain long integers.  Certificate users MUST be able to
//     handle serialNumber values up to 20 octets.  Conforming CAs MUST NOT
//     use serialNumber values longer than 20 octets.
//
//     Note: Non-conforming CAs may issue certificates with serial numbers
//     that are negative or zero.  Certificate users SHOULD be prepared to
//     gracefully handle such certificates.
WARN_UNUSED_RESULT bool VerifySerialNumber(const der::Input& value) {
  bool unused_negative;
  if (!der::IsValidInteger(value, &unused_negative))
    return false;

  // Check if the serial number is too long per RFC 5280.
  if (value.Length() > 20)
    return false;

  return true;
}

}  // namespace

ParsedTbsCertificate::ParsedTbsCertificate() {}

ParsedTbsCertificate::~ParsedTbsCertificate() {}

bool ParseCertificate(const der::Input& certificate_tlv,
                      ParsedCertificate* out) {
  der::Parser parser(certificate_tlv);

  //   Certificate  ::=  SEQUENCE  {
  der::Parser certificate_parser;
  if (!parser.ReadSequence(&certificate_parser))
    return false;

  //        tbsCertificate       TBSCertificate,
  if (!ReadSequenceTLV(&certificate_parser, &out->tbs_certificate_tlv))
    return false;

  //        signatureAlgorithm   AlgorithmIdentifier,
  if (!ReadSequenceTLV(&certificate_parser, &out->signature_algorithm_tlv))
    return false;

  //        signatureValue       BIT STRING  }
  if (!certificate_parser.ReadBitString(&out->signature_value))
    return false;

  // There isn't an extension point at the end of Certificate.
  if (certificate_parser.HasMore())
    return false;

  // By definition the input was a single Certificate, so there shouldn't be
  // unconsumed data.
  if (parser.HasMore())
    return false;

  return true;
}

// From RFC 5280 section 4.1:
//
//   TBSCertificate  ::=  SEQUENCE  {
//        version         [0]  EXPLICIT Version DEFAULT v1,
//        serialNumber         CertificateSerialNumber,
//        signature            AlgorithmIdentifier,
//        issuer               Name,
//        validity             Validity,
//        subject              Name,
//        subjectPublicKeyInfo SubjectPublicKeyInfo,
//        issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
//                             -- If present, version MUST be v2 or v3
//        subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
//                             -- If present, version MUST be v2 or v3
//        extensions      [3]  EXPLICIT Extensions OPTIONAL
//                             -- If present, version MUST be v3
//        }
bool ParseTbsCertificate(const der::Input& tbs_tlv, ParsedTbsCertificate* out) {
  der::Parser parser(tbs_tlv);

  //   Certificate  ::=  SEQUENCE  {
  der::Parser tbs_parser;
  if (!parser.ReadSequence(&tbs_parser))
    return false;

  //        version         [0]  EXPLICIT Version DEFAULT v1,
  der::Input version;
  bool has_version;
  if (!tbs_parser.ReadOptionalTag(der::ContextSpecificConstructed(0), &version,
                                  &has_version)) {
    return false;
  }
  if (has_version) {
    if (!ParseVersion(version, &out->version))
      return false;
    if (out->version == CertificateVersion::V1) {
      // The correct way to specify v1 is to omit the version field since v1 is
      // the DEFAULT.
      return false;
    }
  } else {
    out->version = CertificateVersion::V1;
  }

  //        serialNumber         CertificateSerialNumber,
  if (!tbs_parser.ReadTag(der::kInteger, &out->serial_number))
    return false;
  if (!VerifySerialNumber(out->serial_number))
    return false;

  //        signature            AlgorithmIdentifier,
  if (!ReadSequenceTLV(&tbs_parser, &out->signature_algorithm_tlv))
    return false;

  //        issuer               Name,
  if (!ReadSequenceTLV(&tbs_parser, &out->issuer_tlv))
    return false;

  //        validity             Validity,
  if (!ReadSequenceTLV(&tbs_parser, &out->validity_tlv))
    return false;

  //        subject              Name,
  if (!ReadSequenceTLV(&tbs_parser, &out->subject_tlv))
    return false;

  //        subjectPublicKeyInfo SubjectPublicKeyInfo,
  if (!ReadSequenceTLV(&tbs_parser, &out->spki_tlv))
    return false;

  //        issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                             -- If present, version MUST be v2 or v3
  der::Input issuer_unique_id;
  if (!tbs_parser.ReadOptionalTag(der::ContextSpecificPrimitive(1),
                                  &issuer_unique_id,
                                  &out->has_issuer_unique_id)) {
    return false;
  }
  if (out->has_issuer_unique_id) {
    if (!der::ParseBitString(issuer_unique_id, &out->issuer_unique_id))
      return false;
    if (out->version != CertificateVersion::V2 &&
        out->version != CertificateVersion::V3) {
      return false;
    }
  }

  //        subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                             -- If present, version MUST be v2 or v3
  der::Input subject_unique_id;
  if (!tbs_parser.ReadOptionalTag(der::ContextSpecificPrimitive(2),
                                  &subject_unique_id,
                                  &out->has_subject_unique_id)) {
    return false;
  }
  if (out->has_subject_unique_id) {
    if (!der::ParseBitString(subject_unique_id, &out->subject_unique_id))
      return false;
    if (out->version != CertificateVersion::V2 &&
        out->version != CertificateVersion::V3) {
      return false;
    }
  }

  //        extensions      [3]  EXPLICIT Extensions OPTIONAL
  //                             -- If present, version MUST be v3
  if (!tbs_parser.ReadOptionalTag(der::ContextSpecificConstructed(3),
                                  &out->extensions_tlv, &out->has_extensions)) {
    return false;
  }
  if (out->has_extensions) {
    // extensions_tlv must be a single element. Also check that it is a
    // SEQUENCE.
    if (!IsSequenceTLV(out->extensions_tlv))
      return false;
    if (out->version != CertificateVersion::V3)
      return false;
  }

  // Note that there IS an extension point at the end of TBSCertificate
  // (according to RFC 5912), so from that interpretation, unconsumed data would
  // be allowed in |tbs_parser|.
  //
  // However because only v1, v2, and v3 certificates are supported by the
  // parsing, there shouldn't be any subsequent data in those versions, so
  // reject.
  if (tbs_parser.HasMore())
    return false;

  // By definition the input was a single TBSCertificate, so there shouldn't be
  // unconsumed data.
  if (parser.HasMore())
    return false;

  return true;
}

}  // namespace net
