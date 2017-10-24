// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/physical_web/eddystone/eddystone_encoder.h"

#include <algorithm>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

using std::string;
using std::vector;
using std::equal;

class EddystoneEncoderTest : public testing::Test {
 public:
  EddystoneEncoderTest() {}
  ~EddystoneEncoderTest() override {}

  void SetUp() override {}
  void TearDown() override {}

  bool ByteVectorEquals(const vector<uint8_t>& a, const vector<uint8_t>& b);
};

bool EddystoneEncoderTest::ByteVectorEquals(const vector<uint8_t>& a,
                                            const vector<uint8_t>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  return equal(a.begin(), a.end(), b.begin());
}

TEST_F(EddystoneEncoderTest, NullVector) {
  string valid_url = "https://www.aurl.com/";

  size_t expected_result = physical_web::kNullEncodedUrl;
  size_t actual_result;

  actual_result = physical_web::EncodeUrl(valid_url, nullptr);

  EXPECT_TRUE(expected_result == actual_result);
}

TEST_F(EddystoneEncoderTest, EmptyUrl) {
  string empty_url = "";

  size_t expected_result = physical_web::kEmptyUrl;
  size_t actual_result;
  vector<uint8_t> expected_vector;
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(empty_url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testTotallyInvalidUrl) {
  string invalid_url = "InValidURL.duh";

  size_t expected_result = physical_web::kInvalidUrl;
  size_t actual_result;
  vector<uint8_t> expected_vector;
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(invalid_url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testAlmostvalidUrl) {
  string invalid_url = "https;//.com";

  size_t expected_result = physical_web::kInvalidUrl;
  size_t actual_result;
  vector<uint8_t> expected_vector;
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(invalid_url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testIPAddressUrl) {
  string invalid_url = "https://8.8.8.8/";

  size_t expected_result = physical_web::kInvalidFormat;
  size_t actual_result;
  vector<uint8_t> expected_vector;
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(invalid_url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testInvalidProtocol) {
  string invalid_url = "file://data/foo.com/it";

  size_t expected_result = physical_web::kInvalidFormat;
  size_t actual_result;
  vector<uint8_t> expected_vector;
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(invalid_url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testShortUrl) {
  string url = "http://a.com";
  uint8_t expected_array[] = {0x02,   // "http://"
                              0x61,   // "a"
                              0x07};  // ".com"
  size_t expected_array_length = sizeof(expected_array) / sizeof(uint8_t);

  size_t expected_result = expected_array_length;
  size_t actual_result;

  vector<uint8_t> expected_vector(expected_array,
                                  expected_array + expected_array_length);
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testStandardUrl) {
  string url = "https://www.example.com/";
  uint8_t expected_array[] = {0x01,  // "https://www."
                              0x65, 0x78, 0x61, 0x6d,
                              0x70, 0x6c, 0x65,  // "example"
                              0x00};             // ".com/"
  size_t expected_array_length = sizeof(expected_array) / sizeof(uint8_t);

  size_t expected_result = expected_array_length;
  size_t actual_result;

  vector<uint8_t> expected_vector(expected_array,
                                  expected_array + expected_array_length);
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testStandardHttpUrl) {
  string url = "http://www.example.com/";
  uint8_t expected_array[] = {0x00,  // "http://www."
                              0x65, 0x78, 0x61, 0x6d,
                              0x70, 0x6c, 0x65,  // "example"
                              0x00};             // ".com/"
  size_t expected_array_length = sizeof(expected_array) / sizeof(uint8_t);

  size_t expected_result = expected_array_length;
  size_t actual_result;

  vector<uint8_t> expected_vector(expected_array,
                                  expected_array + expected_array_length);
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testStandardHttpUrlWithoutSuffixSlash) {
  string url = "http://www.example.com";
  uint8_t expected_array[] = {0x00,  // "http://www."
                              0x65, 0x78, 0x61, 0x6d,
                              0x70, 0x6c, 0x65,  // "example"
                              0x07};             // ".com"
  size_t expected_array_length = sizeof(expected_array) / sizeof(uint8_t);

  size_t expected_result = expected_array_length;
  size_t actual_result;

  vector<uint8_t> expected_vector(expected_array,
                                  expected_array + expected_array_length);
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testStandardInfoUrl) {
  string url = "https://www.example.info/";
  uint8_t expected_array[] = {0x01,  // "https://www."
                              0x65, 0x78, 0x61, 0x6d,
                              0x70, 0x6c, 0x65,  // "example"
                              0x04};             // ".info/"
  size_t expected_array_length = sizeof(expected_array) / sizeof(uint8_t);

  size_t expected_result = expected_array_length;
  size_t actual_result;

  vector<uint8_t> expected_vector(expected_array,
                                  expected_array + expected_array_length);
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testAllowsSubDomains) {
  string url = "https://www.example.cs.com/";
  uint8_t expected_array[] = {0x01,  // "https://www."
                              0x65, 0x78, 0x61, 0x6d,
                              0x70, 0x6c, 0x65,  // "example"
                              0x2e, 0x63, 0x73,  // ".cs"
                              0x00};             // ".com/"
  size_t expected_array_length = sizeof(expected_array) / sizeof(uint8_t);

  size_t expected_result = expected_array_length;
  size_t actual_result;

  vector<uint8_t> expected_vector(expected_array,
                                  expected_array + expected_array_length);
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testAllowsPaths) {
  string url = "https://www.example.cs.com/r";
  uint8_t expected_array[] = {0x01,  // "https://www."
                              0x65, 0x78, 0x61, 0x6d,
                              0x70, 0x6c, 0x65,  // "example"
                              0x2e, 0x63, 0x73,  // ".cs"
                              0x00,              // ".com/"
                              0x72};             // "r"
  size_t expected_array_length = sizeof(expected_array) / sizeof(uint8_t);

  size_t expected_result = expected_array_length;
  size_t actual_result;

  vector<uint8_t> expected_vector(expected_array,
                                  expected_array + expected_array_length);
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testAllowsMultiplePaths) {
  string url = "https://www.example.cs.com/r/red/it";
  uint8_t expected_array[] = {
      0x01,                                             // "https://www."
      0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65,         // "example"
      0x2e, 0x63, 0x73,                                 // ".cs"
      0x00,                                             // ".com/"
      0x72, 0x2f, 0x72, 0x65, 0x64, 0x2f, 0x69, 0x74};  // "r/red/it"
  size_t expected_array_length = sizeof(expected_array) / sizeof(uint8_t);

  size_t expected_result = expected_array_length;
  size_t actual_result;

  vector<uint8_t> expected_vector(expected_array,
                                  expected_array + expected_array_length);
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testHiddenCompression1) {
  string url = "https://www.example.com/foo.com";
  uint8_t expected_array[] = {0x01,  // "https://www."
                              0x65, 0x78, 0x61, 0x6d,
                              0x70, 0x6c, 0x65,  // "example"
                              0x00,              // ".com/"
                              0x66, 0x6f, 0x6f,  // "foo"
                              0x07};             // ".com"
  size_t expected_array_length = sizeof(expected_array) / sizeof(uint8_t);

  size_t expected_result = expected_array_length;
  size_t actual_result;

  vector<uint8_t> expected_vector(expected_array,
                                  expected_array + expected_array_length);
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testHiddenCompression2) {
  string url = "https://www.example.com/foo.comma/";
  uint8_t expected_array[] = {0x01,  // "https://www."
                              0x65, 0x78, 0x61, 0x6d,
                              0x70, 0x6c, 0x65,   // "example"
                              0x00,               // ".com/"
                              0x66, 0x6f, 0x6f,   // "foo"
                              0x07,               // ".com"
                              0x6d, 0x61, 0x2f};  // "ma/"
  size_t expected_array_length = sizeof(expected_array) / sizeof(uint8_t);

  size_t expected_result = expected_array_length;
  size_t actual_result;

  vector<uint8_t> expected_vector(expected_array,
                                  expected_array + expected_array_length);
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testSharedCompression) {
  string url = "https://www.communities.com/";
  uint8_t expected_array[] = {0x01,              // "https://www."
                              0x63, 0x6f, 0x6d,  // "com"
                              0x6d, 0x75, 0x6e, 0x69,
                              0x74, 0x69, 0x65, 0x73,  //"munities"
                              0x00};                   // ".com/"
  size_t expected_array_length = sizeof(expected_array) / sizeof(uint8_t);

  size_t expected_result = expected_array_length;
  size_t actual_result;

  vector<uint8_t> expected_vector(expected_array,
                                  expected_array + expected_array_length);
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}

TEST_F(EddystoneEncoderTest, testSpecCollision) {
  // decode(encode(".com/aURL.com")) == decode(encode("http://www.aURL.com"))
  // without url validation
  string invalid_url = ".com/aURL.com";

  size_t invalid_expected_result = physical_web::kInvalidUrl;
  size_t invalid_actual_result;
  vector<uint8_t> invalid_expected_vector;
  vector<uint8_t> invalid_actual_vector;

  invalid_actual_result =
      physical_web::EncodeUrl(invalid_url, &invalid_actual_vector);

  EXPECT_TRUE(invalid_expected_result == invalid_actual_result);
  EXPECT_TRUE(ByteVectorEquals(invalid_expected_vector, invalid_actual_vector));

  // Now for the valid Url.
  string valid_url = "http://www.aURL.com";
  uint8_t valid_expected_array[] = {0x00,                    // "https://www."
                                    0x61, 0x55, 0x52, 0x4c,  // "aUrl"
                                    0x07};                   // ".com"
  size_t valid_expected_array_length =
      sizeof(valid_expected_array) / sizeof(uint8_t);

  size_t valid_expected_result = valid_expected_array_length;
  size_t valid_actual_result;

  vector<uint8_t> valid_expected_vector(
      valid_expected_array, valid_expected_array + valid_expected_array_length);
  vector<uint8_t> valid_actual_vector;

  valid_actual_result =
      physical_web::EncodeUrl(valid_url, &valid_actual_vector);

  EXPECT_TRUE(valid_expected_result == valid_actual_result);
  EXPECT_TRUE(ByteVectorEquals(valid_expected_vector, valid_actual_vector));

  EXPECT_FALSE(ByteVectorEquals(invalid_actual_vector, valid_actual_vector));
}

TEST_F(EddystoneEncoderTest, testComplexUrl) {
  string url = "http://user:pass@google.com:99/foo;bar?q=a#ref";
  uint8_t expected_array[] = {0x02,                                // "http://"
                              0x75, 0x73, 0x65, 0x72, 0x3a,        // "user:"
                              0x70, 0x61, 0x73, 0x73, 0x40,        // "pass@"
                              0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65,  // "google"
                              0x07,                                // ".com"
                              0x3a, 0x39, 0x39, 0x2f,              // ":99/"
                              0x66, 0x6f, 0x6f, 0x3b,              // "foo;"
                              0x62, 0x61, 0x72, 0x3f,              // "bar?"
                              0x71, 0x3d, 0x61, 0x23,              // "q=a#"
                              0x72, 0x65, 0x66};                   // "ref"
  size_t expected_array_length = sizeof(expected_array) / sizeof(uint8_t);

  size_t expected_result = expected_array_length;
  size_t actual_result;

  vector<uint8_t> expected_vector(expected_array,
                                  expected_array + expected_array_length);
  vector<uint8_t> actual_vector;

  actual_result = physical_web::EncodeUrl(url, &actual_vector);

  EXPECT_TRUE(expected_result == actual_result);
  EXPECT_TRUE(ByteVectorEquals(expected_vector, actual_vector));
}
