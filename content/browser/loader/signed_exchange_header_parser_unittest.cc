// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/signed_exchange_header_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class SignedExchangeHeaderParserTest : public ::testing::Test {
 protected:
  SignedExchangeHeaderParserTest() {}
};

TEST_F(SignedExchangeHeaderParserTest, ParseSignedHeaders) {
  const char hdr_string[] = "\"content-type\", \"digest\"";
  base::Optional<std::vector<std::string>> headers =
      SignedExchangeHeaderParser::ParseSignedHeaders(hdr_string);
  EXPECT_TRUE(headers.has_value());
  ASSERT_EQ(headers->size(), 2u);
  EXPECT_EQ(headers->at(0), "content-type");
  EXPECT_EQ(headers->at(1), "digest");
}

TEST_F(SignedExchangeHeaderParserTest, SignedHeadersNoQuotes) {
  const char hdr_string[] = "content-type, digest";
  base::Optional<std::vector<std::string>> headers =
      SignedExchangeHeaderParser::ParseSignedHeaders(hdr_string);
  EXPECT_FALSE(headers.has_value());
}

TEST_F(SignedExchangeHeaderParserTest, SignedHeadersParseError) {
  const char hdr_string[] = "\"content-type\", \"digest";
  base::Optional<std::vector<std::string>> headers =
      SignedExchangeHeaderParser::ParseSignedHeaders(hdr_string);
  EXPECT_FALSE(headers.has_value());
}

TEST_F(SignedExchangeHeaderParserTest, QuotedChar) {
  const char hdr_string[] = R"("\\o/")";
  base::Optional<std::vector<std::string>> headers =
      SignedExchangeHeaderParser::ParseSignedHeaders(hdr_string);
  EXPECT_TRUE(headers.has_value());
  ASSERT_EQ(headers->size(), 1u);
  EXPECT_EQ(headers->at(0), "\\o/");
}

TEST_F(SignedExchangeHeaderParserTest, ParseSignature) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY;"
      " integrity=\"mi\";"
      " validityUrl=\"https://example.com/resource.validity.1511128380\";"
      " certUrl=\"https://example.com/oldcerts\";"
      " certSha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI;"
      " date=1511128380; expires=1511733180,"
      "srisig;"
      " sig=*lGZVaJJM5f2oGczFlLmBdKTDL+QADza4BgeO494ggACYJOvrof6uh5OJCcwKrk7DK+"
      "LBch0jssDYPp5CLc1SDA;"
      " integrity=\"mi\";"
      " validityUrl=\"https://example.com/resource.validity.1511128380\";"
      " ed25519Key=*zsSevyFsxyZHiUluVBDd4eypdRLTqyWRVOJuuKUz+A8;"
      " date=1511128380; expires=1511733180";
  const uint8_t decoded_sig1[] = {
      0x30, 0x45, 0x02, 0x21, 0x00, 0xd7, 0x94, 0x8d, 0xa0, 0x37, 0x74, 0x4d,
      0x06, 0x58, 0x05, 0x8a, 0xe4, 0x4d, 0x16, 0x96, 0x57, 0x70, 0x32, 0x1a,
      0x52, 0x95, 0xfa, 0x1c, 0x81, 0x30, 0x71, 0x91, 0x1c, 0xd1, 0xc6, 0x58,
      0x2c, 0x02, 0x20, 0x6b, 0xd0, 0xec, 0x54, 0xe3, 0x0c, 0xfa, 0x0e, 0x58,
      0xa7, 0x01, 0x01, 0x74, 0x65, 0xb7, 0xb1, 0x2f, 0x9b, 0xbe, 0x79, 0x80,
      0x24, 0x98, 0x92, 0x33, 0x08, 0x6e, 0x05, 0xda, 0xa9, 0xe5, 0x46};
  const uint8_t decoded_certSha256[] = {
      0x5b, 0xbb, 0x81, 0xf7, 0xaf, 0x5d, 0x15, 0x6d, 0xcc, 0x6f, 0x96,
      0x5e, 0x7c, 0xf4, 0xbd, 0x4e, 0xae, 0x59, 0x6c, 0x7e, 0x62, 0x4a,
      0x63, 0x88, 0x2e, 0x98, 0xef, 0xda, 0xa1, 0x00, 0xae, 0x62};
  const uint8_t decoded_sig2[] = {
      0x94, 0x66, 0x55, 0x68, 0x92, 0x4c, 0xe5, 0xfd, 0xa8, 0x19, 0xcc,
      0xc5, 0x94, 0xb9, 0x81, 0x74, 0xa4, 0xc3, 0x2f, 0xe4, 0x00, 0x0f,
      0x36, 0xb8, 0x06, 0x07, 0x8e, 0xe3, 0xde, 0x20, 0x80, 0x00, 0x98,
      0x24, 0xeb, 0xeb, 0xa1, 0xfe, 0xae, 0x87, 0x93, 0x89, 0x09, 0xcc,
      0x0a, 0xae, 0x4e, 0xc3, 0x2b, 0xe2, 0xc1, 0x72, 0x1d, 0x23, 0xb2,
      0xc0, 0xd8, 0x3e, 0x9e, 0x42, 0x2d, 0xcd, 0x52, 0x0c};
  const uint8_t decoded_ed25519Key[] = {
      0xce, 0xc4, 0x9e, 0xbf, 0x21, 0x6c, 0xc7, 0x26, 0x47, 0x89, 0x49,
      0x6e, 0x54, 0x10, 0xdd, 0xe1, 0xec, 0xa9, 0x75, 0x12, 0xd3, 0xab,
      0x25, 0x91, 0x54, 0xe2, 0x6e, 0xb8, 0xa5, 0x33, 0xf8, 0x0f};

  auto signatures = SignedExchangeHeaderParser::ParseSignature(hdr_string);
  EXPECT_TRUE(signatures.has_value());
  ASSERT_EQ(signatures->size(), 2u);

  EXPECT_EQ(signatures->at(0).label, "sig1");
  EXPECT_EQ(signatures->at(0).sig,
            std::string(reinterpret_cast<const char*>(decoded_sig1),
                        sizeof(decoded_sig1)));
  EXPECT_EQ(signatures->at(0).integrity, "mi");
  EXPECT_EQ(signatures->at(0).validityUrl,
            "https://example.com/resource.validity.1511128380");
  EXPECT_EQ(signatures->at(0).certUrl, "https://example.com/oldcerts");
  EXPECT_EQ(signatures->at(0).certSha256,
            std::string(reinterpret_cast<const char*>(decoded_certSha256),
                        sizeof(decoded_certSha256)));
  EXPECT_EQ(signatures->at(0).date, 1511128380ul);
  EXPECT_EQ(signatures->at(0).expires, 1511733180ul);

  EXPECT_EQ(signatures->at(1).label, "srisig");
  EXPECT_EQ(signatures->at(1).sig,
            std::string(reinterpret_cast<const char*>(decoded_sig2),
                        sizeof(decoded_sig2)));
  EXPECT_EQ(signatures->at(1).integrity, "mi");
  EXPECT_EQ(signatures->at(1).validityUrl,
            "https://example.com/resource.validity.1511128380");
  EXPECT_EQ(signatures->at(1).ed25519Key,
            std::string(reinterpret_cast<const char*>(decoded_ed25519Key),
                        sizeof(decoded_ed25519Key)));
  EXPECT_EQ(signatures->at(1).date, 1511128380ul);
  EXPECT_EQ(signatures->at(1).expires, 1511733180ul);
}

TEST_F(SignedExchangeHeaderParserTest, IncompleteSignature) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY;"
      // no integrity= param
      " validityUrl=\"https://example.com/resource.validity.1511128380\";"
      " certUrl=\"https://example.com/oldcerts\";"
      " certSha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeHeaderParser::ParseSignature(hdr_string);
  EXPECT_FALSE(signatures.has_value());
}

TEST_F(SignedExchangeHeaderParserTest, HasBothCertAndEd25519Key) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY;"
      " integrity=\"mi\";"
      " validityUrl=\"https://example.com/resource.validity.1511128380\";"
      " certUrl=\"https://example.com/oldcerts\";"
      " certSha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI;"
      " ed25519Key=*zsSevyFsxyZHiUluVBDd4eypdRLTqyWRVOJuuKUz+A8;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeHeaderParser::ParseSignature(hdr_string);
  EXPECT_FALSE(signatures.has_value());
}

TEST_F(SignedExchangeHeaderParserTest, DuplicatedParam) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY;"
      " integrity=\"mi\";"
      " validityUrl=\"https://example.com/resource.validity.1511128380\";"
      " certUrl=\"https://example.com/oldcerts\";"
      " certUrl=\"https://example.com/oldcerts\";"
      " certSha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeHeaderParser::ParseSignature(hdr_string);
  EXPECT_FALSE(signatures.has_value());
}

}  // namespace content
