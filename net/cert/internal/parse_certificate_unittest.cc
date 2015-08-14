// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/parse_certificate.h"

#include "net/cert/internal/test_helpers.h"
#include "net/der/input.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

std::string GetFilePath(const std::string file_name) {
  return std::string("net/data/parse_certificate_unittest/") + file_name;
}

// Loads certificate data and expectations from the PEM file |file_name|.
// Verifies that parsing the Certificate succeeds, and each parsed field matches
// the expectations.
void EnsureParsingCertificateSucceds(const std::string& file_name) {
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
  EnsureParsingCertificateSucceds("cert_version3.pem");
}

// Tests parsing a simplified Certificate-like structure (the sub-fields for
// algorithm and tbsCertificate are not actually valid, but ParseCertificate()
// doesn't check them)
TEST(ParseCertificateTest, Skeleton) {
  EnsureParsingCertificateSucceds("cert_skeleton.pem");
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

}  // namespace

}  // namespace net
