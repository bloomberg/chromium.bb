// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/parse_certificate.h"

#include "base/strings/stringprintf.h"
#include "net/cert/internal/test_helpers.h"
#include "net/der/input.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Pretty-prints a GeneralizedTime as a human-readable string for use in test
// expectations (it is more readable to specify the expected results as a
// string).
std::string ToString(const der::GeneralizedTime& time) {
  return base::StringPrintf(
      "year=%d, month=%d, day=%d, hours=%d, minutes=%d, seconds=%d", time.year,
      time.month, time.day, time.hours, time.minutes, time.seconds);
}

std::string GetFilePath(const std::string& file_name) {
  return std::string("net/data/parse_certificate_unittest/") + file_name;
}

// Loads certificate data and expectations from the PEM file |file_name|.
// Verifies that parsing the Certificate succeeds, and each parsed field matches
// the expectations.
void EnsureParsingCertificateSucceeds(const std::string& file_name) {
  std::string data;
  std::string expected_tbs_certificate;
  std::string expected_signature_algorithm;
  std::string expected_signature;

  // Read the certificate data and test expectations from a single PEM file.
  const PemBlockMapping mappings[] = {
      {"CERTIFICATE", &data},
      {"SIGNATURE", &expected_signature},
      {"SIGNATURE ALGORITHM", &expected_signature_algorithm},
      {"TBS CERTIFICATE", &expected_tbs_certificate},
  };
  ASSERT_TRUE(ReadTestDataFromPemFile(GetFilePath(file_name), mappings));

  // Parsing the certificate should succeed.
  ParsedCertificate parsed;
  ASSERT_TRUE(ParseCertificate(InputFromString(&data), &parsed));

  // Ensure that the ParsedCertificate matches expectations.
  EXPECT_EQ(0, parsed.signature_value.unused_bits());
  EXPECT_EQ(InputFromString(&expected_signature),
            parsed.signature_value.bytes());
  EXPECT_EQ(InputFromString(&expected_signature_algorithm),
            parsed.signature_algorithm_tlv);
  EXPECT_EQ(InputFromString(&expected_tbs_certificate),
            parsed.tbs_certificate_tlv);
}

// Loads certificate data from the PEM file |file_name| and verifies that the
// Certificate parsing fails.
void EnsureParsingCertificateFails(const std::string& file_name) {
  std::string data;

  const PemBlockMapping mappings[] = {
      {"CERTIFICATE", &data},
  };

  ASSERT_TRUE(ReadTestDataFromPemFile(GetFilePath(file_name), mappings));

  // Parsing the Certificate should fail.
  ParsedCertificate parsed;
  ASSERT_FALSE(ParseCertificate(InputFromString(&data), &parsed));
}

// Tests parsing a Certificate.
TEST(ParseCertificateTest, Version3) {
  EnsureParsingCertificateSucceeds("cert_version3.pem");
}

// Tests parsing a simplified Certificate-like structure (the sub-fields for
// algorithm and tbsCertificate are not actually valid, but ParseCertificate()
// doesn't check them)
TEST(ParseCertificateTest, Skeleton) {
  EnsureParsingCertificateSucceeds("cert_skeleton.pem");
}

// Tests parsing a Certificate that is not a sequence fails.
TEST(ParseCertificateTest, NotSequence) {
  EnsureParsingCertificateFails("cert_not_sequence.pem");
}

// Tests that uncomsumed data is not allowed after the main SEQUENCE.
TEST(ParseCertificateTest, DataAfterSignature) {
  EnsureParsingCertificateFails("cert_data_after_signature.pem");
}

// Tests that parsing fails if the signature BIT STRING is missing.
TEST(ParseCertificateTest, MissingSignature) {
  EnsureParsingCertificateFails("cert_missing_signature.pem");
}

// Tests that parsing fails if the signature is present but not a BIT STRING.
TEST(ParseCertificateTest, SignatureNotBitString) {
  EnsureParsingCertificateFails("cert_signature_not_bit_string.pem");
}

// Tests that parsing fails if the main SEQUENCE is empty (missing all the
// fields).
TEST(ParseCertificateTest, EmptySequence) {
  EnsureParsingCertificateFails("cert_empty_sequence.pem");
}

// Tests what happens when the signature algorithm is present, but has the wrong
// tag.
TEST(ParseCertificateTest, AlgorithmNotSequence) {
  EnsureParsingCertificateFails("cert_algorithm_not_sequence.pem");
}

// Loads tbsCertificate data and expectations from the PEM file |file_name|.
// Verifies that parsing the TBSCertificate succeeds, and each parsed field
// matches the expectations.
void EnsureParsingTbsSucceeds(const std::string& file_name,
                              CertificateVersion expected_version) {
  std::string data;
  std::string expected_serial_number;
  std::string expected_signature_algorithm;
  std::string expected_issuer;
  std::string expected_validity_not_before;
  std::string expected_validity_not_after;
  std::string expected_subject;
  std::string expected_spki;
  std::string expected_issuer_unique_id;
  std::string expected_subject_unique_id;
  std::string expected_extensions;

  // Read the certificate data and test expectations from a single PEM file.
  const PemBlockMapping mappings[] = {
      {"TBS CERTIFICATE", &data},
      {"SIGNATURE ALGORITHM", &expected_signature_algorithm},
      {"SERIAL NUMBER", &expected_serial_number},
      {"ISSUER", &expected_issuer},
      {"VALIDITY NOTBEFORE", &expected_validity_not_before},
      {"VALIDITY NOTAFTER", &expected_validity_not_after},
      {"SUBJECT", &expected_subject},
      {"SPKI", &expected_spki},
      {"ISSUER UNIQUE ID", &expected_issuer_unique_id, true},
      {"SUBJECT UNIQUE ID", &expected_subject_unique_id, true},
      {"EXTENSIONS", &expected_extensions, true},
  };
  ASSERT_TRUE(ReadTestDataFromPemFile(GetFilePath(file_name), mappings));

  // Parsing the TBSCertificate should succeed.
  ParsedTbsCertificate parsed;
  ASSERT_TRUE(ParseTbsCertificate(InputFromString(&data), &parsed));

  // Ensure that the ParsedTbsCertificate matches expectations.
  EXPECT_EQ(expected_version, parsed.version);

  EXPECT_EQ(InputFromString(&expected_serial_number), parsed.serial_number);
  EXPECT_EQ(InputFromString(&expected_signature_algorithm),
            parsed.signature_algorithm_tlv);

  EXPECT_EQ(InputFromString(&expected_issuer), parsed.issuer_tlv);

  // In the test expectations PEM file, validity is described as a
  // textual string of the parsed value (rather than as DER).
  EXPECT_EQ(expected_validity_not_before, ToString(parsed.validity_not_before));
  EXPECT_EQ(expected_validity_not_after, ToString(parsed.validity_not_after));

  EXPECT_EQ(InputFromString(&expected_subject), parsed.subject_tlv);
  EXPECT_EQ(InputFromString(&expected_spki), parsed.spki_tlv);

  EXPECT_EQ(InputFromString(&expected_issuer_unique_id),
            parsed.issuer_unique_id.bytes());
  EXPECT_EQ(!expected_issuer_unique_id.empty(), parsed.has_issuer_unique_id);
  EXPECT_EQ(InputFromString(&expected_subject_unique_id),
            parsed.subject_unique_id.bytes());
  EXPECT_EQ(!expected_subject_unique_id.empty(), parsed.has_subject_unique_id);

  EXPECT_EQ(InputFromString(&expected_extensions), parsed.extensions_tlv);
  EXPECT_EQ(!expected_extensions.empty(), parsed.has_extensions);
}

// Loads certificate data from the PEM file |file_name| and verifies that the
// Certificate parsing succeed, however the TBSCertificate parsing fails.
void EnsureParsingTbsFails(const std::string& file_name) {
  std::string data;

  const PemBlockMapping mappings[] = {
      {"TBS CERTIFICATE", &data},
  };

  ASSERT_TRUE(ReadTestDataFromPemFile(GetFilePath(file_name), mappings));

  // Parsing the TBSCertificate should fail.
  ParsedTbsCertificate parsed;
  ASSERT_FALSE(ParseTbsCertificate(InputFromString(&data), &parsed));
}

// Tests parsing a TBSCertificate for v3 that contains no optional fields.
TEST(ParseTbsCertificateTest, Version3NoOptionals) {
  EnsureParsingTbsSucceeds("tbs_v3_no_optionals.pem", CertificateVersion::V3);
}

// Tests parsing a TBSCertificate for v3 that contains extensions.
TEST(ParseTbsCertificateTest, Version3WithExtensions) {
  EnsureParsingTbsSucceeds("tbs_v3_extensions.pem", CertificateVersion::V3);
}

// Tests parsing a TBSCertificate for v3 that contains no optional fields, and
// has a negative serial number.
//
// CAs are not supposed to include negative serial numbers, however RFC 5280
// expects consumers to deal with it anyway).
TEST(ParseTbsCertificateTest, NegativeSerialNumber) {
  EnsureParsingTbsSucceeds("tbs_negative_serial_number.pem",
                           CertificateVersion::V3);
}

// Tests parsing a TBSCertificate with a serial number that is 21 octets long
// (and the first byte is 0).
TEST(ParseTbCertificateTest, SerialNumber21OctetsLeading0) {
  EnsureParsingTbsFails("tbs_serial_number_21_octets_leading_0.pem");
}

// Tests parsing a TBSCertificate with a serial number that is 26 octets long
// (and does not contain a leading 0).
TEST(ParseTbsCertificateTest, SerialNumber26Octets) {
  EnsureParsingTbsFails("tbs_serial_number_26_octets.pem");
}

// Tests parsing a TBSCertificate which lacks a version number (causing it to
// default to v1).
TEST(ParseTbsCertificateTest, Version1) {
  EnsureParsingTbsSucceeds("tbs_v1.pem", CertificateVersion::V1);
}

// The version was set to v1 explicitly rather than omitting the version field.
TEST(ParseTbsCertificateTest, ExplicitVersion1) {
  EnsureParsingTbsFails("tbs_explicit_v1.pem");
}

// Extensions are not defined in version 1.
TEST(ParseTbsCertificateTest, Version1WithExtensions) {
  EnsureParsingTbsFails("tbs_v1_extensions.pem");
}

// Extensions are not defined in version 2.
TEST(ParseTbsCertificateTest, Version2WithExtensions) {
  EnsureParsingTbsFails("tbs_v2_extensions.pem");
}

// A boring version 2 certificate with none of the optional fields.
TEST(ParseTbsCertificateTest, Version2NoOptionals) {
  EnsureParsingTbsSucceeds("tbs_v2_no_optionals.pem", CertificateVersion::V2);
}

// A version 2 certificate with an issuer unique ID field.
TEST(ParseTbsCertificateTest, Version2IssuerUniqueId) {
  EnsureParsingTbsSucceeds("tbs_v2_issuer_unique_id.pem",
                           CertificateVersion::V2);
}

// A version 2 certificate with both a issuer and subject unique ID field.
TEST(ParseTbsCertificateTest, Version2IssuerAndSubjectUniqueId) {
  EnsureParsingTbsSucceeds("tbs_v2_issuer_and_subject_unique_id.pem",
                           CertificateVersion::V2);
}

// A version 3 certificate with all of the optional fields (issuer unique id,
// subject unique id, and extensions).
TEST(ParseTbsCertificateTest, Version3AllOptionals) {
  EnsureParsingTbsSucceeds("tbs_v3_all_optionals.pem", CertificateVersion::V3);
}

// The version was set to v4, which is unrecognized.
TEST(ParseTbsCertificateTest, Version4) {
  EnsureParsingTbsFails("tbs_v4.pem");
}

// Tests that extraneous data after extensions in a v3 is rejected.
TEST(ParseTbsCertificateTest, Version3DataAfterExtensions) {
  EnsureParsingTbsFails("tbs_v3_data_after_extensions.pem");
}

// Tests using a real-world certificate (whereas the other tests are fabricated
// (and in fact invalid) data.
TEST(ParseTbsCertificateTest, Version3Real) {
  EnsureParsingTbsSucceeds("tbs_v3_real.pem", CertificateVersion::V3);
}

// Parses a TBSCertificate whose "validity" field expresses both notBefore
// and notAfter using UTCTime.
TEST(ParseTbsCertificateTest, ValidityBothUtcTime) {
  EnsureParsingTbsSucceeds("tbs_validity_both_utc_time.pem",
                           CertificateVersion::V3);
}

// Parses a TBSCertificate whose "validity" field expresses both notBefore
// and notAfter using GeneralizedTime.
TEST(ParseTbsCertificateTest, ValidityBothGeneralizedTime) {
  EnsureParsingTbsSucceeds("tbs_validity_both_generalized_time.pem",
                           CertificateVersion::V3);
}

// Parses a TBSCertificate whose "validity" field expresses notBefore using
// UTCTime and notAfter using GeneralizedTime.
TEST(ParseTbsCertificateTest, ValidityUTCTimeAndGeneralizedTime) {
  EnsureParsingTbsSucceeds("tbs_validity_utc_time_and_generalized_time.pem",
                           CertificateVersion::V3);
}

// Parses a TBSCertificate whose validity" field expresses notBefore using
// GeneralizedTime and notAfter using UTCTime. Also of interest, notBefore >
// notAfter. Parsing will succeed, however no time can satisfy this constraint.
TEST(ParseTbsCertificateTest, ValidityGeneralizedTimeAndUTCTime) {
  EnsureParsingTbsSucceeds("tbs_validity_generalized_time_and_utc_time.pem",
                           CertificateVersion::V3);
}

// Parses a TBSCertificate whose "validity" field does not strictly follow
// the DER rules (and fails to be parsed).
TEST(ParseTbsCertificateTest, ValidityRelaxed) {
  EnsureParsingTbsFails("tbs_validity_relaxed.pem");
}

}  // namespace

}  // namespace net
