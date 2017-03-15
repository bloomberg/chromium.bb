// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/utility/payment_manifest_parser.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

void ExpectUnableToParse(const std::string& input) {
  std::vector<mojom::PaymentManifestSectionPtr> actual_output =
      PaymentManifestParser::ParseIntoVector(input);
  EXPECT_TRUE(actual_output.empty());
}

void ExpectParsed(
    const std::string& input,
    const std::string& expected_package_name,
    int expected_version,
    const std::vector<std::vector<uint8_t>>& expected_fingerprints = {}) {
  std::vector<mojom::PaymentManifestSectionPtr> actual_output =
      PaymentManifestParser::ParseIntoVector(input);
  ASSERT_EQ(1U, actual_output.size());
  EXPECT_EQ(expected_package_name, actual_output.front()->package_name);
  EXPECT_EQ(expected_version, actual_output.front()->version);
  EXPECT_EQ(expected_fingerprints,
            actual_output.front()->sha256_cert_fingerprints);
}

TEST(PaymentManifestParserTest, NullContentIsMalformed) {
  ExpectUnableToParse(std::string());
}

TEST(PaymentManifestParserTest, NonJsonContentIsMalformed) {
  ExpectUnableToParse("this is not json");
}

TEST(PaymentManifestParserTest, StringContentIsMalformed) {
  ExpectUnableToParse("\"this is a string\"");
}

TEST(PaymentManifestParserTest, EmptyDictionaryIsMalformed) {
  ExpectUnableToParse("{}");
}

TEST(PaymentManifestParserTest, NullAndroidSectionIsMalformed) {
  ExpectUnableToParse("{\"android\": null}");
}

TEST(PaymentManifestParserTest, NumberAndroidSectionIsMalformed) {
  ExpectUnableToParse("{\"android\": 0}");
}

TEST(PaymentManifestParserTest, ListOfNumbersAndroidSectionIsMalformed) {
  ExpectUnableToParse("{\"android\": [0]}");
}

TEST(PaymentManifestParserTest,
     ListOfEmptyDictionariesAndroidSectionIsMalformed) {
  ExpectUnableToParse("{\"android\": [{}]}");
}

TEST(PaymentManifestParserTest, NoPackageNameIsMalformed) {
  ExpectUnableToParse("{\"android\": [{\"version\": 3}]}");
}

TEST(PaymentManifestParserTest, OnlyPackageNameIsWellFormed) {
  ExpectParsed("{\"android\": [{\"package\": \"*\"}]}", "*", 0);
}

TEST(PaymentManifestParserTest, WellFormed) {
  ExpectParsed(
      "{\"android\": [{"
      "\"package\": \"com.bobpay.app\","
      "\"version\":  3,"
      "\"sha256_cert_fingerprints\": "
      "[\"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7:A8:A9:B0:B1:B2:"
      "B3:B4:B5:B6:B7:B8:B9:C0:C1\"]}]}",
      "com.bobpay.app", 3,
      {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xA0,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0, 0xB1,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xC0, 0xC1}});
}

TEST(PaymentManifestParserTest, ValuesShouldNotHaveNulCharacters) {
  ExpectUnableToParse(
      "{\"android\": [{"
      "\"package\": \"com.bob\0pay.app\","
      "\"version\":  3,"
      "\"sha256_cert_fingerprints\": "
      "[\"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7:A8:A9:B0:B1:B2:"
      "B3:B4:B5:B6:B7:B8:B9:C0:C1\"]}]}");
}

TEST(PaymentManifestParserTest, StarPackageShouldBeTheOnlySection) {
  ExpectUnableToParse(
      "{\"android\": [{"
      "\"package\": \"com.bobpay.app\","
      "\"version\":  3,"
      "\"sha256_cert_fingerprints\": "
      "[\"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7:A8:A9:B0:B1:B2:"
      "B3:B4:B5:B6:B7:B8:B9:C0:C1\"]}, {"
      "\"package\": \"*\"}]}");
}

TEST(PaymentManifestParserTest, DuplicateSignaturesWellFormed) {
  ExpectParsed(
      "{\"android\": [{"
      "\"package\": \"com.bobpay.app\","
      "\"version\":  3,"
      "\"sha256_cert_fingerprints\": "
      "[\"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7:A8:A9:B0:B1:B2:"
      "B3:B4:B5:B6:B7:B8:B9:C0:C1\","
      "\"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7:A8:A9:B0:B1:B2:"
      "B3:B4:B5:B6:B7:B8:B9:C0:C1\"]}]}",
      "com.bobpay.app", 3,
      {{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xA0,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0, 0xB1,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xC0, 0xC1},
       {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xA0,
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0, 0xB1,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xC0, 0xC1}});
}

TEST(PaymentManifestParserTest, KeysShouldBeLowerCase) {
  ExpectUnableToParse(
      "{\"android\": [{"
      "\"package\": \"com.bobpay.app\","
      "\"version\":  3,"
      "\"sha256_CERT_fingerprints\": "
      "[\"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7:A8:A9:B0:B1:B2:"
      "B3:B4:B5:B6:B7:B8:B9:C0:C1\"]}]}");
}

TEST(PaymentManifestParserTest, FingerprintsShouldBeUpperCase) {
  ExpectUnableToParse(
      "{\"android\": [{"
      "\"package\": \"com.bobpay.app\","
      "\"version\":  3,"
      "\"sha256_cert_fingerprints\": "
      "[\"00:01:02:03:04:05:06:07:08:09:a0:a1:a2:a3:a4:a5:a6:a7:a8:a9:b0:b1:b2:"
      "b3:b4:b5:b6:b7:b8:b9:c0:c1\"]}]}");
}

TEST(PaymentManifestParserTest, FingerprintBytesShouldBeColonSeparated) {
  ExpectUnableToParse(
      "{\"android\": [{"
      "\"package\": \"com.bobpay.app\","
      "\"version\":  3,"
      "\"sha256_cert_fingerprints\":"
      "[\"00010203040506070809A0A1A2A3A4A5A6A7A8A9B0B1B2B3B4B5B6B7B8B9C0C1\"]}]"
      "}");
}

TEST(PaymentManifestParserTest, FingerprintsShouldBeHex) {
  ExpectUnableToParse(
      "{\"android\": [{"
      "\"package\": \"com.bobpay.app\","
      "\"version\":  3,"
      "\"sha256_cert_fingerprints\": "
      "[\"GG:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7:A8:A9:B0:B1:B2:"
      "B3:B4:B5:B6:B7:B8:B9:C0:C1\"]}]}");
}

TEST(PaymentManifestParserTest, FingerprintsShouldContainsThirtyTwoBytes) {
  ExpectUnableToParse(
      "{\"android\": [{"
      "\"package\": \"com.bobpay.app\","
      "\"version\":  3,"
      "\"sha256_cert_fingerprints\": "
      "[\"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7:A8:A9:B0:B1:B2:"
      "B3:B4:B5:B6:B7:B8:B9:C0\"]}]}");
  ExpectUnableToParse(
      "{\"android\": [{"
      "\"package\": \"com.bobpay.app\","
      "\"version\":  3,"
      "\"sha256_cert_fingerprints\": "
      "[\"00:01:02:03:04:05:06:07:08:09:A0:A1:A2:A3:A4:A5:A6:A7:A8:A9:B0:B1:B2:"
      "B3:B4:B5:B6:B7:B8:B9:C0:C1:C2\"]}]}");
}

}  // namespace
}  // namespace payments
