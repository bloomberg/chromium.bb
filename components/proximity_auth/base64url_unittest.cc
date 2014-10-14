// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/base64url.h"

#include "base/base64.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {

TEST(ProximityAuthBase64UrlTest, EncodeRegularString) {
  const std::string input = "Hello world!";
  const std::string expected_output = "SGVsbG8gd29ybGQh";

  std::string web_safe_output;
  Base64UrlEncode(input, &web_safe_output);
  EXPECT_EQ(expected_output, web_safe_output);

  // For good measure, make sure that the encoding matches the //base
  // implemenation as well.
  std::string non_web_safe_output;
  base::Base64Encode(input, &non_web_safe_output);
  EXPECT_EQ(web_safe_output, non_web_safe_output);
}

TEST(ProximityAuthBase64UrlTest, DecodeRegularString) {
  const std::string input = "SGVsbG8gd29ybGQh";
  const std::string expected_output = "Hello world!";

  std::string web_safe_output;
  EXPECT_TRUE(Base64UrlDecode(input, &web_safe_output));
  EXPECT_EQ(expected_output, web_safe_output);

  // For good measure, make sure that the encoding matches the //base
  // implemenation as well.
  std::string non_web_safe_output;
  EXPECT_TRUE(base::Base64Decode(input, &non_web_safe_output));
  EXPECT_EQ(web_safe_output, non_web_safe_output);
}

TEST(ProximityAuthBase64UrlTest, EncodeSpecialCharacters) {
  // This happens to be a stable encoding, i.e. encode(decode(s)) gives back s.
  const std::string encoded = "/+Y=";
  std::string decoded;
  ASSERT_TRUE(base::Base64Decode(encoded, &decoded));

  // Decoded strings that encode to special characters are non-printable, so,
  // for ease of testing, just compare the web-safe and non-web-safe encodings.
  std::string web_safe_encoded;
  Base64UrlEncode(decoded, &web_safe_encoded);
  EXPECT_EQ("_-Y=", web_safe_encoded);
}

TEST(ProximityAuthBase64UrlTest, DecodeSpecialCharacters) {
  const std::string encoded = "_-Y=";
  std::string decoded;
  ASSERT_TRUE(Base64UrlDecode(encoded, &decoded));

  // Decoded strings that encode to special characters are non-printable, so,
  // for ease of testing, just compare the web-safe and non-web-safe encodings.
  std::string non_web_safe_encoded;
  base::Base64Encode(decoded, &non_web_safe_encoded);
  EXPECT_EQ("/+Y=", non_web_safe_encoded);
}

}  // namespace proximity_auth
