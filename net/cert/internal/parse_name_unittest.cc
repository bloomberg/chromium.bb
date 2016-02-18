// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/parse_name.h"

#include "net/cert/internal/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {
// Loads test data from file. The filename is constructed from the parameters:
// |prefix| describes the type of data being tested, e.g. "ascii",
// "unicode_bmp", "unicode_supplementary", and "invalid".
// |value_type| indicates what ASN.1 type is used to encode the data.
// |suffix| indicates any additional modifications, such as caseswapping,
// whitespace adding, etc.
::testing::AssertionResult LoadTestData(const std::string& prefix,
                                        const std::string& value_type,
                                        const std::string& suffix,
                                        std::string* result) {
  std::string path = "net/data/verify_name_match_unittest/names/" + prefix +
                     "-" + value_type + "-" + suffix + ".pem";

  const PemBlockMapping mappings[] = {
      {"NAME", result},
  };

  return ReadTestDataFromPemFile(path, mappings);
}
}

TEST(ParseNameTest, ConvertBmpString) {
  const uint8_t der[] = {
      0x00, 0x66, 0x00, 0x6f, 0x00, 0x6f, 0x00, 0x62, 0x00, 0x61, 0x00, 0x72,
  };
  X509NameAttribute value(der::Input(), der::kBmpString, der::Input(der));
  std::string result;
  ASSERT_TRUE(value.ValueAsStringUnsafe(&result));
  ASSERT_EQ("foobar", result);
}

// BmpString must encode characters in pairs of 2 bytes.
TEST(ParseNameTest, ConvertInvalidBmpString) {
  const uint8_t der[] = {0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72, 0x72};
  X509NameAttribute value(der::Input(), der::kBmpString, der::Input(der));
  std::string result;
  ASSERT_FALSE(value.ValueAsStringUnsafe(&result));
}

TEST(ParseNameTest, ConvertUniversalString) {
  const uint8_t der[] = {0x00, 0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 0x6f,
                         0x00, 0x00, 0x00, 0x6f, 0x00, 0x00, 0x00, 0x62,
                         0x00, 0x00, 0x00, 0x61, 0x00, 0x00, 0x00, 0x72};
  X509NameAttribute value(der::Input(), der::kUniversalString, der::Input(der));
  std::string result;
  ASSERT_TRUE(value.ValueAsStringUnsafe(&result));
}

// UniversalString must encode characters in pairs of 4 bytes.
TEST(ParseNameTest, ConvertInvalidUniversalString) {
  const uint8_t der[] = {0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72};
  X509NameAttribute value(der::Input(), der::kUniversalString, der::Input(der));
  std::string result;
  ASSERT_FALSE(value.ValueAsStringUnsafe(&result));
}

TEST(ParseNameTest, EmptyName) {
  const uint8_t der[] = {0x30, 0x00};
  der::Input rdn(der);
  std::vector<X509NameAttribute> atv;
  ASSERT_TRUE(ParseName(rdn, &atv));
  ASSERT_EQ(0u, atv.size());
}

TEST(ParseNameTest, ValidName) {
  const uint8_t der[] = {0x30, 0x3c, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55,
                         0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x14, 0x30,
                         0x12, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x13, 0x0b, 0x47,
                         0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x20, 0x49, 0x6e, 0x63,
                         0x2e, 0x31, 0x17, 0x30, 0x15, 0x06, 0x03, 0x55, 0x04,
                         0x03, 0x13, 0x0e, 0x47, 0x6f, 0x6f, 0x67, 0x6c, 0x65,
                         0x20, 0x54, 0x65, 0x73, 0x74, 0x20, 0x43, 0x41};
  der::Input rdn(der);
  std::vector<X509NameAttribute> atv;
  ASSERT_TRUE(ParseName(rdn, &atv));
  ASSERT_EQ(3u, atv.size());
  ASSERT_TRUE(atv[0].type == TypeCountryNameOid());
  ASSERT_EQ("US", atv[0].value.AsString());
  ASSERT_TRUE(atv[1].type == TypeOrganizationNameOid());
  ASSERT_EQ("Google Inc.", atv[1].value.AsString());
  ASSERT_TRUE(atv[2].type == TypeCommonNameOid());
  ASSERT_EQ("Google Test CA", atv[2].value.AsString());
}

TEST(ParseNameTest, InvalidNameExtraData) {
  std::string invalid;
  ASSERT_TRUE(
      LoadTestData("invalid", "AttributeTypeAndValue", "extradata", &invalid));
  std::vector<X509NameAttribute> atv;
  ASSERT_FALSE(ParseName(SequenceValueFromString(&invalid), &atv));
}

TEST(ParseNameTest, InvalidNameEmpty) {
  std::string invalid;
  ASSERT_TRUE(
      LoadTestData("invalid", "AttributeTypeAndValue", "empty", &invalid));
  std::vector<X509NameAttribute> atv;
  ASSERT_FALSE(ParseName(SequenceValueFromString(&invalid), &atv));
}

TEST(ParseNameTest, InvalidNameBadType) {
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "AttributeTypeAndValue",
                           "badAttributeType", &invalid));
  std::vector<X509NameAttribute> atv;
  ASSERT_FALSE(ParseName(SequenceValueFromString(&invalid), &atv));
}

TEST(ParseNameTest, InvalidNameNotSequence) {
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "AttributeTypeAndValue", "setNotSequence",
                           &invalid));
  std::vector<X509NameAttribute> atv;
  ASSERT_FALSE(ParseName(SequenceValueFromString(&invalid), &atv));
}

TEST(ParseNameTest, InvalidNameNotSet) {
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "RDN", "sequenceInsteadOfSet", &invalid));
  std::vector<X509NameAttribute> atv;
  ASSERT_FALSE(ParseName(SequenceValueFromString(&invalid), &atv));
}

TEST(ParseNameTest, InvalidNameEmptyRdn) {
  std::string invalid;
  ASSERT_TRUE(LoadTestData("invalid", "RDN", "empty", &invalid));
  std::vector<X509NameAttribute> atv;
  ASSERT_FALSE(ParseName(SequenceValueFromString(&invalid), &atv));
}

}  // namespace net
